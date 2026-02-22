/// @file src/ara/com/serialization.h
/// @brief Declarations for serialization.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_SERIALIZATION_H
#define ARA_COM_SERIALIZATION_H

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <org/eclipse/cyclonedds/core/cdr/basic_cdr_ser.hpp>

#include "../core/result.h"
#include "./com_error_domain.h"

namespace ara
{
    namespace com
    {
        namespace detail
        {
            using org::eclipse::cyclonedds::core::cdr::move;
            using org::eclipse::cyclonedds::core::cdr::read;
            using org::eclipse::cyclonedds::core::cdr::write;

            template <typename T>
            class HasCdrSerializerOps
            {
            private:
                template <typename U>
                static auto Test(int)
                    -> decltype(
                        move(
                            std::declval<org::eclipse::cyclonedds::core::cdr::basic_cdr_stream &>(),
                            std::declval<U &>(),
                            false),
                        write(
                            std::declval<org::eclipse::cyclonedds::core::cdr::basic_cdr_stream &>(),
                            std::declval<U &>(),
                            false),
                        read(
                            std::declval<org::eclipse::cyclonedds::core::cdr::basic_cdr_stream &>(),
                            std::declval<U &>(),
                            false),
                        std::true_type{});

                template <typename>
                static std::false_type Test(...);

            public:
                static constexpr bool value = decltype(Test<T>(0))::value;
            };
        } // namespace detail

        template <typename T, typename Enable = void>
        struct Serializer;

        /// @brief Default serializer for trivially-copyable types.
        template <typename T>
        struct Serializer<T, typename std::enable_if<std::is_trivially_copyable<T>::value>::type>
        {
            static std::vector<std::uint8_t> Serialize(const T &value)
            {
                std::vector<std::uint8_t> buffer(sizeof(T));
                std::memcpy(buffer.data(), &value, sizeof(T));
                return buffer;
            }

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

        /// @brief CDR serializer for ROS/CycloneDDS generated message types.
        template <typename T>
        struct Serializer<
            T,
            typename std::enable_if<
                !std::is_trivially_copyable<T>::value &&
                detail::HasCdrSerializerOps<T>::value>::type>
        {
            static std::vector<std::uint8_t> Serialize(const T &value)
            {
                using namespace org::eclipse::cyclonedds::core::cdr;

                T mutable_value = value;
                basic_cdr_stream sizer;
                move(sizer, mutable_value, false);
                const std::size_t payload_size = sizer.position();

                std::vector<std::uint8_t> buffer(payload_size + 4U, 0U);
                buffer[0] = 0x00;
                buffer[1] = 0x01;
                buffer[2] = 0x00;
                buffer[3] = 0x00;

                basic_cdr_stream writer;
                writer.set_buffer(reinterpret_cast<char *>(buffer.data() + 4U), payload_size);
                write(writer, mutable_value, false);
                return buffer;
            }

            static core::Result<T> Deserialize(
                const std::uint8_t *data,
                std::size_t size)
            {
                using namespace org::eclipse::cyclonedds::core::cdr;

                if (data == nullptr || size <= 4U)
                {
                    return core::Result<T>::FromError(
                        MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                }

                try
                {
                    T value{};
                    basic_cdr_stream reader;
                    reader.set_buffer(
                        reinterpret_cast<char *>(const_cast<std::uint8_t *>(data + 4U)),
                        size - 4U);
                    read(reader, value, false);
                    return core::Result<T>::FromValue(std::move(value));
                }
                catch (...)
                {
                    return core::Result<T>::FromError(
                        MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                }
            }
        };

        /// @brief Compile-time error for unsupported complex types.
        template <typename T>
        struct Serializer<
            T,
            typename std::enable_if<
                !std::is_trivially_copyable<T>::value &&
                !detail::HasCdrSerializerOps<T>::value>::type>
        {
            static_assert(
                std::is_trivially_copyable<T>::value || detail::HasCdrSerializerOps<T>::value,
                "Serializer<T> requires either trivially-copyable type or Cyclone CDR move/write/read support.");
        };

        /// @brief Serializer specialization for std::string
        template <>
        struct Serializer<std::string, void>
        {
            static std::vector<std::uint8_t> Serialize(const std::string &value)
            {
                std::vector<std::uint8_t> buffer;
                std::uint32_t len = static_cast<std::uint32_t>(value.size());
                buffer.resize(sizeof(len) + len);
                std::memcpy(buffer.data(), &len, sizeof(len));
                std::memcpy(buffer.data() + sizeof(len), value.data(), len);
                return buffer;
            }

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
            static std::vector<std::uint8_t> Serialize(
                const std::vector<std::uint8_t> &value)
            {
                return value;
            }

            static core::Result<std::vector<std::uint8_t>> Deserialize(
                const std::uint8_t *data,
                std::size_t size)
            {
                return core::Result<std::vector<std::uint8_t>>::FromValue(
                    std::vector<std::uint8_t>(data, data + size));
            }
        };
    } // namespace com
} // namespace ara

#endif
