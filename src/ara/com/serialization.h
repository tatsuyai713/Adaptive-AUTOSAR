/// @file src/ara/com/serialization.h
/// @brief Declarations for serialization.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_SERIALIZATION_H
#define ARA_COM_SERIALIZATION_H

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
#include <org/eclipse/cyclonedds/core/cdr/basic_cdr_ser.hpp>
#endif

#include "../core/result.h"
#include "./com_error_domain.h"

namespace ara
{
    namespace com
    {
        namespace detail
        {
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
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
#else
            /// @brief Fallback: when CycloneDDS is not available, CDR detection
            ///        always evaluates to false.
            template <typename T>
            class HasCdrSerializerOps
            {
            public:
                static constexpr bool value = false;
            };
#endif
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

            /// @brief Deserialize from offset; returns (value, bytes_consumed).
            ///        Used by SkeletonMethod for sequential multi-arg deserialization.
            static core::Result<std::pair<T, std::size_t>> DeserializeAt(
                const std::uint8_t *data,
                std::size_t maxSize)
            {
                if (maxSize < sizeof(T))
                {
                    return core::Result<std::pair<T, std::size_t>>::FromError(
                        MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                }

                T value;
                std::memcpy(&value, data, sizeof(T));
                return core::Result<std::pair<T, std::size_t>>::FromValue(
                    std::make_pair(std::move(value), sizeof(T)));
            }
        };

        /// @brief CDR serializer for ROS/CycloneDDS generated message types.
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
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

            /// @brief Deserialize from offset; returns (value, bytes_consumed).
            ///        Used by SkeletonMethod for sequential multi-arg deserialization.
            static core::Result<std::pair<T, std::size_t>> DeserializeAt(
                const std::uint8_t *data,
                std::size_t maxSize)
            {
                using namespace org::eclipse::cyclonedds::core::cdr;

                if (data == nullptr || maxSize <= 4U)
                {
                    return core::Result<std::pair<T, std::size_t>>::FromError(
                        MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                }

                try
                {
                    T value{};
                    basic_cdr_stream reader;
                    reader.set_buffer(
                        reinterpret_cast<char *>(const_cast<std::uint8_t *>(data + 4U)),
                        maxSize - 4U);
                    read(reader, value, false);
                    const std::size_t consumed = 4U + reader.position();
                    return core::Result<std::pair<T, std::size_t>>::FromValue(
                        std::make_pair(std::move(value), consumed));
                }
                catch (...)
                {
                    return core::Result<std::pair<T, std::size_t>>::FromError(
                        MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                }
            }
        };
#endif // ARA_COM_USE_CYCLONEDDS

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

            /// @brief Deserialize from offset; returns (value, bytes_consumed).
            static core::Result<std::pair<std::string, std::size_t>> DeserializeAt(
                const std::uint8_t *data,
                std::size_t maxSize)
            {
                if (maxSize < sizeof(std::uint32_t))
                {
                    return core::Result<std::pair<std::string, std::size_t>>::FromError(
                        MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                }

                std::uint32_t len;
                std::memcpy(&len, data, sizeof(len));

                if (maxSize < sizeof(len) + len)
                {
                    return core::Result<std::pair<std::string, std::size_t>>::FromError(
                        MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                }

                std::string value(
                    reinterpret_cast<const char *>(data + sizeof(len)), len);
                const std::size_t consumed = sizeof(len) + len;
                return core::Result<std::pair<std::string, std::size_t>>::FromValue(
                    std::make_pair(std::move(value), consumed));
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

            /// @brief Deserialize from offset; consumes all remaining bytes.
            ///        Note: use as the last (or only) argument in multi-arg contexts.
            static core::Result<std::pair<std::vector<std::uint8_t>, std::size_t>>
            DeserializeAt(
                const std::uint8_t *data,
                std::size_t maxSize)
            {
                return core::Result<std::pair<std::vector<std::uint8_t>, std::size_t>>::
                    FromValue(
                        std::make_pair(
                            std::vector<std::uint8_t>(data, data + maxSize),
                            maxSize));
            }
        };

        /// @brief Serializer for std::vector<T> where T is not uint8_t.
        ///        Format: [uint32_t count][element0][element1]...
        template <typename T>
        struct Serializer<
            std::vector<T>,
            typename std::enable_if<
                !std::is_same<T, std::uint8_t>::value>::type>
        {
            static std::vector<std::uint8_t> Serialize(
                const std::vector<T> &value)
            {
                std::vector<std::uint8_t> buffer;
                const std::uint32_t count =
                    static_cast<std::uint32_t>(value.size());
                buffer.resize(sizeof(count));
                std::memcpy(buffer.data(), &count, sizeof(count));

                for (const auto &elem : value)
                {
                    auto elemBytes = Serializer<T>::Serialize(elem);
                    buffer.insert(buffer.end(),
                                  elemBytes.begin(), elemBytes.end());
                }
                return buffer;
            }

            static core::Result<std::vector<T>> Deserialize(
                const std::uint8_t *data,
                std::size_t size)
            {
                auto result = DeserializeAt(data, size);
                if (!result.HasValue())
                {
                    return core::Result<std::vector<T>>::FromError(
                        result.Error());
                }
                return core::Result<std::vector<T>>::FromValue(
                    std::move(result).Value().first);
            }

            static core::Result<std::pair<std::vector<T>, std::size_t>>
            DeserializeAt(
                const std::uint8_t *data,
                std::size_t maxSize)
            {
                if (maxSize < sizeof(std::uint32_t))
                {
                    return core::Result<std::pair<std::vector<T>, std::size_t>>::
                        FromError(MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                }

                std::uint32_t count;
                std::memcpy(&count, data, sizeof(count));
                std::size_t offset = sizeof(count);
                std::vector<T> result;
                result.reserve(count);

                for (std::uint32_t i = 0; i < count; ++i)
                {
                    if (offset > maxSize)
                    {
                        return core::Result<std::pair<std::vector<T>, std::size_t>>::
                            FromError(MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                    }
                    auto elemResult = Serializer<T>::DeserializeAt(
                        data + offset, maxSize - offset);
                    if (!elemResult.HasValue())
                    {
                        return core::Result<std::pair<std::vector<T>, std::size_t>>::
                            FromError(elemResult.Error());
                    }
                    result.push_back(std::move(elemResult).Value().first);
                    offset += elemResult.Value().second;
                }

                return core::Result<std::pair<std::vector<T>, std::size_t>>::
                    FromValue(std::make_pair(std::move(result), offset));
            }
        };

        /// @brief Serializer for std::map<K, V>.
        ///        Format: [uint32_t count][key0][val0][key1][val1]...
        template <typename K, typename V>
        struct Serializer<std::map<K, V>, void>
        {
            static std::vector<std::uint8_t> Serialize(
                const std::map<K, V> &value)
            {
                std::vector<std::uint8_t> buffer;
                const std::uint32_t count =
                    static_cast<std::uint32_t>(value.size());
                buffer.resize(sizeof(count));
                std::memcpy(buffer.data(), &count, sizeof(count));

                for (const auto &kv : value)
                {
                    auto keyBytes = Serializer<K>::Serialize(kv.first);
                    buffer.insert(buffer.end(),
                                  keyBytes.begin(), keyBytes.end());
                    auto valBytes = Serializer<V>::Serialize(kv.second);
                    buffer.insert(buffer.end(),
                                  valBytes.begin(), valBytes.end());
                }
                return buffer;
            }

            static core::Result<std::map<K, V>> Deserialize(
                const std::uint8_t *data,
                std::size_t size)
            {
                auto result = DeserializeAt(data, size);
                if (!result.HasValue())
                {
                    return core::Result<std::map<K, V>>::FromError(
                        result.Error());
                }
                return core::Result<std::map<K, V>>::FromValue(
                    std::move(result).Value().first);
            }

            static core::Result<std::pair<std::map<K, V>, std::size_t>>
            DeserializeAt(
                const std::uint8_t *data,
                std::size_t maxSize)
            {
                if (maxSize < sizeof(std::uint32_t))
                {
                    return core::Result<std::pair<std::map<K, V>, std::size_t>>::
                        FromError(MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                }

                std::uint32_t count;
                std::memcpy(&count, data, sizeof(count));
                std::size_t offset = sizeof(count);
                std::map<K, V> result;

                for (std::uint32_t i = 0; i < count; ++i)
                {
                    if (offset > maxSize)
                    {
                        return core::Result<std::pair<std::map<K, V>, std::size_t>>::
                            FromError(MakeErrorCode(ComErrc::kFieldValueIsNotValid));
                    }
                    auto keyResult = Serializer<K>::DeserializeAt(
                        data + offset, maxSize - offset);
                    if (!keyResult.HasValue())
                    {
                        return core::Result<std::pair<std::map<K, V>, std::size_t>>::
                            FromError(keyResult.Error());
                    }
                    offset += keyResult.Value().second;

                    auto valResult = Serializer<V>::DeserializeAt(
                        data + offset, maxSize - offset);
                    if (!valResult.HasValue())
                    {
                        return core::Result<std::pair<std::map<K, V>, std::size_t>>::
                            FromError(valResult.Error());
                    }
                    offset += valResult.Value().second;

                    result.emplace(
                        std::move(keyResult).Value().first,
                        std::move(valResult).Value().first);
                }

                return core::Result<std::pair<std::map<K, V>, std::size_t>>::
                    FromValue(std::make_pair(std::move(result), offset));
            }
        };
    } // namespace com
} // namespace ara

#endif
