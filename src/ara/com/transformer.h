/// @file src/ara/com/transformer.h
/// @brief Pluggable transformer framework per AUTOSAR AP SWS_CM_00710.
/// @details The Transformer abstraction allows pluggable serialization/
///          deserialization chains to be applied to data before/after
///          transport. A TransformerChain is a sequence of Transformers
///          applied in order on outbound data and reverse order on inbound.

#ifndef ARA_COM_TRANSFORMER_H
#define ARA_COM_TRANSFORMER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "../core/result.h"
#include "./com_error_domain.h"

namespace ara
{
    namespace com
    {
        /// @brief Transformer type identifier per SWS_CM_00711.
        enum class TransformerType : std::uint8_t
        {
            kSerialization = 0U,   ///< Default serialization transformer.
            kProtection = 1U,      ///< E2E protection transformer.
            kSecurity = 2U,        ///< Authentication/encryption transformer.
            kCompression = 3U,     ///< Data compression transformer.
            kCustom = 0xFFU        ///< User-defined transformer.
        };

        /// @brief Configuration for a transformer instance per SWS_CM_00712.
        struct TransformerConfig
        {
            /// @brief Transformer type identifier.
            TransformerType Type{TransformerType::kSerialization};

            /// @brief Human-readable transformer name.
            std::string Name;

            /// @brief Whether this transformer is mandatory.
            bool IsMandatory{true};

            /// @brief Transformer-specific opaque configuration data.
            std::vector<std::uint8_t> ConfigData;
        };

        /// @brief Abstract base class for data transformation per SWS_CM_00710.
        ///        Subclasses implement specific serialization, protection,
        ///        compression, or custom data transformations.
        class Transformer
        {
        public:
            virtual ~Transformer() noexcept = default;

            /// @brief Transform outbound data (e.g., serialize, protect, compress).
            /// @param input Raw input bytes.
            /// @returns Transformed output bytes or error.
            virtual core::Result<std::vector<std::uint8_t>> Transform(
                const std::vector<std::uint8_t> &input) = 0;

            /// @brief Inverse-transform inbound data (e.g., deserialize, verify, decompress).
            /// @param input Transformed input bytes.
            /// @returns Original output bytes or error.
            virtual core::Result<std::vector<std::uint8_t>> InverseTransform(
                const std::vector<std::uint8_t> &input) = 0;

            /// @brief Get this transformer's type.
            /// @returns TransformerType of this instance.
            virtual TransformerType GetType() const noexcept = 0;
        };

        /// @brief Identity transformer that passes data through unmodified.
        ///        Used as the default transformer when no transformations
        ///        are configured.
        class IdentityTransformer : public Transformer
        {
        public:
            core::Result<std::vector<std::uint8_t>> Transform(
                const std::vector<std::uint8_t> &input) override
            {
                return core::Result<std::vector<std::uint8_t>>::FromValue(input);
            }

            core::Result<std::vector<std::uint8_t>> InverseTransform(
                const std::vector<std::uint8_t> &input) override
            {
                return core::Result<std::vector<std::uint8_t>>::FromValue(input);
            }

            TransformerType GetType() const noexcept override
            {
                return TransformerType::kSerialization;
            }
        };

        /// @brief Ordered chain of transformers applied sequentially per SWS_CM_00713.
        ///        Transform applies transformers in forward order.
        ///        InverseTransform applies in reverse order.
        class TransformerChain
        {
        private:
            std::vector<std::shared_ptr<Transformer>> mTransformers;

        public:
            /// @brief Construct an empty transformer chain.
            TransformerChain() noexcept = default;

            /// @brief Add a transformer to the end of the chain.
            /// @param transformer Transformer instance to append.
            void Add(std::shared_ptr<Transformer> transformer)
            {
                if (transformer)
                {
                    mTransformers.push_back(std::move(transformer));
                }
            }

            /// @brief Apply all transformers in forward order.
            /// @param input Input bytes.
            /// @returns Transformed bytes after all transformers.
            core::Result<std::vector<std::uint8_t>> Transform(
                const std::vector<std::uint8_t> &input)
            {
                auto current = input;
                for (auto &tf : mTransformers)
                {
                    auto result = tf->Transform(current);
                    if (!result.HasValue())
                    {
                        return result;
                    }
                    current = std::move(result).Value();
                }
                return core::Result<std::vector<std::uint8_t>>::FromValue(
                    std::move(current));
            }

            /// @brief Apply all inverse transformers in reverse order.
            /// @param input Input bytes.
            /// @returns Original bytes after inverse transformation.
            core::Result<std::vector<std::uint8_t>> InverseTransform(
                const std::vector<std::uint8_t> &input)
            {
                auto current = input;
                for (auto it = mTransformers.rbegin();
                     it != mTransformers.rend(); ++it)
                {
                    auto result = (*it)->InverseTransform(current);
                    if (!result.HasValue())
                    {
                        return result;
                    }
                    current = std::move(result).Value();
                }
                return core::Result<std::vector<std::uint8_t>>::FromValue(
                    std::move(current));
            }

            /// @brief Number of transformers in the chain.
            /// @returns Count of registered transformers.
            std::size_t Size() const noexcept
            {
                return mTransformers.size();
            }

            /// @brief Check if chain is empty.
            /// @returns true if no transformers added.
            bool Empty() const noexcept
            {
                return mTransformers.empty();
            }

            /// @brief Remove all transformers from the chain.
            void Clear() noexcept
            {
                mTransformers.clear();
            }
        };

        /// @brief Factory for creating transformers from configuration per SWS_CM_00714.
        class TransformerFactory
        {
        public:
            virtual ~TransformerFactory() noexcept = default;

            /// @brief Create a transformer from configuration.
            /// @param config Transformer configuration.
            /// @returns Created transformer instance or error.
            virtual core::Result<std::shared_ptr<Transformer>> Create(
                const TransformerConfig &config) = 0;
        };

        /// @brief Default transformer factory that creates IdentityTransformers.
        class DefaultTransformerFactory : public TransformerFactory
        {
        public:
            core::Result<std::shared_ptr<Transformer>> Create(
                const TransformerConfig &) override
            {
                return core::Result<std::shared_ptr<Transformer>>::FromValue(
                    std::make_shared<IdentityTransformer>());
            }
        };

        /// @brief SOME/IP serialization transformer per SWS_CM_00710.
        ///        Prepends a minimal SOME/IP-style header on Transform and
        ///        strips it on InverseTransform. The header is 8 bytes:
        ///        [ServiceId:2][MethodId:2][Length:4] in network byte order.
        class SomeIpTransformer : public Transformer
        {
        private:
            std::uint16_t mServiceId;
            std::uint16_t mMethodId;

            static constexpr std::size_t kHeaderSize{8U};

            static void PutUint16BE(
                std::vector<std::uint8_t> &buf,
                std::size_t offset,
                std::uint16_t value) noexcept
            {
                buf[offset] = static_cast<std::uint8_t>(value >> 8);
                buf[offset + 1] = static_cast<std::uint8_t>(value & 0xFF);
            }

            static std::uint16_t GetUint16BE(
                const std::vector<std::uint8_t> &buf,
                std::size_t offset) noexcept
            {
                return static_cast<std::uint16_t>(
                    (static_cast<std::uint16_t>(buf[offset]) << 8) |
                    buf[offset + 1]);
            }

            static void PutUint32BE(
                std::vector<std::uint8_t> &buf,
                std::size_t offset,
                std::uint32_t value) noexcept
            {
                buf[offset] = static_cast<std::uint8_t>(value >> 24);
                buf[offset + 1] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
                buf[offset + 2] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
                buf[offset + 3] = static_cast<std::uint8_t>(value & 0xFF);
            }

            static std::uint32_t GetUint32BE(
                const std::vector<std::uint8_t> &buf,
                std::size_t offset) noexcept
            {
                return (static_cast<std::uint32_t>(buf[offset]) << 24) |
                       (static_cast<std::uint32_t>(buf[offset + 1]) << 16) |
                       (static_cast<std::uint32_t>(buf[offset + 2]) << 8) |
                       static_cast<std::uint32_t>(buf[offset + 3]);
            }

        public:
            /// @brief Construct a SOME/IP transformer.
            /// @param serviceId Service identifier for the header.
            /// @param methodId Method/event identifier for the header.
            SomeIpTransformer(
                std::uint16_t serviceId,
                std::uint16_t methodId) noexcept
                : mServiceId{serviceId},
                  mMethodId{methodId}
            {
            }

            core::Result<std::vector<std::uint8_t>> Transform(
                const std::vector<std::uint8_t> &input) override
            {
                std::vector<std::uint8_t> output(kHeaderSize + input.size());
                PutUint16BE(output, 0, mServiceId);
                PutUint16BE(output, 2, mMethodId);
                PutUint32BE(output, 4, static_cast<std::uint32_t>(input.size()));
                std::copy(input.begin(), input.end(),
                          output.begin() + static_cast<std::ptrdiff_t>(kHeaderSize));
                return core::Result<std::vector<std::uint8_t>>::FromValue(
                    std::move(output));
            }

            core::Result<std::vector<std::uint8_t>> InverseTransform(
                const std::vector<std::uint8_t> &input) override
            {
                if (input.size() < kHeaderSize)
                {
                    return core::Result<std::vector<std::uint8_t>>::FromError(
                        MakeErrorCode(ComErrc::kSerializationError));
                }

                const std::uint32_t payloadLen = GetUint32BE(input, 4);
                if (input.size() < kHeaderSize + payloadLen)
                {
                    return core::Result<std::vector<std::uint8_t>>::FromError(
                        MakeErrorCode(ComErrc::kSerializationError));
                }

                std::vector<std::uint8_t> output(
                    input.begin() + static_cast<std::ptrdiff_t>(kHeaderSize),
                    input.begin() + static_cast<std::ptrdiff_t>(kHeaderSize + payloadLen));
                return core::Result<std::vector<std::uint8_t>>::FromValue(
                    std::move(output));
            }

            TransformerType GetType() const noexcept override
            {
                return TransformerType::kSerialization;
            }

            /// @brief Get the configured service ID.
            std::uint16_t GetServiceId() const noexcept { return mServiceId; }
            /// @brief Get the configured method/event ID.
            std::uint16_t GetMethodId() const noexcept { return mMethodId; }
        };
    }
}

#endif
