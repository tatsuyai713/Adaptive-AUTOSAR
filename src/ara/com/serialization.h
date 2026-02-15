/// @file src/ara/com/serialization.h
/// @brief Declarations for serialization.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_SERIALIZATION_H
#define ARA_COM_SERIALIZATION_H

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>
#include "../core/result.h"
#include "./com_error_domain.h"

namespace ara
{
    namespace com
    {
        /// @brief Default serializer for trivially-copyable types.
        ///        Provides POD serialization via memcpy. Specialize this template
        ///        for complex or non-trivially-copyable types.
        template <typename T, typename Enable = void>
        struct Serializer
        {
            static_assert(
                std::is_trivially_copyable<T>::value,
                "Default Serializer requires trivially copyable types. "
                "Provide a Serializer<T> specialization for complex types.");

            /// @brief Serializes a trivially-copyable value into raw bytes.
            /// @param value Value to serialize.
            /// @returns Byte vector containing a binary copy of `value`.
            static std::vector<std::uint8_t> Serialize(const T &value)
            {
                std::vector<std::uint8_t> buffer(sizeof(T));
                std::memcpy(buffer.data(), &value, sizeof(T));
                return buffer;
            }

            /// @brief Deserializes a trivially-copyable value from raw bytes.
            /// @param data Pointer to input payload buffer.
            /// @param size Payload size in bytes.
            /// @returns `Result<T>` with value on success or `kFieldValueIsNotValid`.
            static core::Result<T> Deserialize(
                const std::uint8_t *data,
                std::size_t size)
            {
                if (size < sizeof(T))
                {
                    return core::Result<T>::FromError(
                        MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                }

                T value;
                std::memcpy(&value, data, sizeof(T));
                return core::Result<T>::FromValue(std::move(value));
            }
        };

        /// @brief Serializer specialization for std::string
        template <>
        struct Serializer<std::string, void>
        {
            /// @brief Serializes a string as `<uint32 length><bytes>`.
            /// @param value String value to serialize.
            /// @returns Encoded byte vector.
            static std::vector<std::uint8_t> Serialize(const std::string &value)
            {
                std::vector<std::uint8_t> buffer;
                std::uint32_t len = static_cast<std::uint32_t>(value.size());
                buffer.resize(sizeof(len) + len);
                std::memcpy(buffer.data(), &len, sizeof(len));
                std::memcpy(buffer.data() + sizeof(len), value.data(), len);
                return buffer;
            }

            /// @brief Deserializes a string from `<uint32 length><bytes>`.
            /// @param data Pointer to encoded payload.
            /// @param size Payload size in bytes.
            /// @returns `Result<std::string>` on success or validation error.
            static core::Result<std::string> Deserialize(
                const std::uint8_t *data,
                std::size_t size)
            {
                if (size < sizeof(std::uint32_t))
                {
                    return core::Result<std::string>::FromError(
                        MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                }

                std::uint32_t len;
                std::memcpy(&len, data, sizeof(len));

                if (size < sizeof(len) + len)
                {
                    return core::Result<std::string>::FromError(
                        MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                }

                return core::Result<std::string>::FromValue(
                    std::string(
                        reinterpret_cast<const char *>(data + sizeof(len)),
                        len));
            }
        };

        /// @brief Serializer specialization for std::vector<uint8_t> (raw bytes passthrough)
        template <>
        struct Serializer<std::vector<std::uint8_t>, void>
        {
            /// @brief Returns the input bytes without transformation.
            /// @param value Byte vector to forward.
            /// @returns Same content as `value`.
            static std::vector<std::uint8_t> Serialize(
                const std::vector<std::uint8_t> &value)
            {
                return value;
            }

            /// @brief Copies raw payload bytes into a vector.
            /// @param data Payload start pointer.
            /// @param size Payload size in bytes.
            /// @returns Byte vector containing copied payload.
            static core::Result<std::vector<std::uint8_t>> Deserialize(
                const std::uint8_t *data,
                std::size_t size)
            {
                return core::Result<std::vector<std::uint8_t>>::FromValue(
                    std::vector<std::uint8_t>(data, data + size));
            }
        };
    }
}

#endif
