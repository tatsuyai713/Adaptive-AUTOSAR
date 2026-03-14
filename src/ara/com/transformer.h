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
    }
}

#endif
