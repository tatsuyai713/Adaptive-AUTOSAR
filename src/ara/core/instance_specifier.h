/// @file src/ara/core/instance_specifier.h
/// @brief Declarations for instance specifier.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef INSTANCE_SPECIFIER_H
#define INSTANCE_SPECIFIER_H

#include <string>
#include <vector>
#include "./result.h"

namespace ara
{
    namespace core
    {
        /// @brief AUTOSAR shortname-path wrapper
        class InstanceSpecifier final
        {
        private:
            std::string mMetaModelIdentifier;

        public:
            /// @brief Constructor
            /// @param metaModelIdentifier Shortname-path
            explicit InstanceSpecifier(std::string metaModelIdentifier);

            /// @brief Copy constructor.
            /// @param other Source instance.
            InstanceSpecifier(const InstanceSpecifier &other);
            /// @brief Move constructor.
            /// @param other Source instance.
            InstanceSpecifier(InstanceSpecifier &&other) noexcept;

            /// @brief Copy assignment.
            /// @param other Source instance.
            /// @returns Reference to `*this`.
            InstanceSpecifier &operator=(const InstanceSpecifier &other);
            /// @brief Move assignment.
            /// @param other Source instance.
            /// @returns Reference to `*this`.
            InstanceSpecifier &operator=(InstanceSpecifier &&other);

            InstanceSpecifier() = delete;
            ~InstanceSpecifier() noexcept = default;

            /// @brief InstanceSpecifier factory
            /// @param metaModelIdentifier Shortname-path
            /// @returns Result containing the created InstanceSpecifier
            static Result<InstanceSpecifier> Create(std::string metaModelIdentifier);

            /// @brief Equality comparison with another instance specifier.
            inline bool operator==(const InstanceSpecifier &other) const noexcept
            {
                return mMetaModelIdentifier == other.mMetaModelIdentifier;
            }

            /// @brief Equality comparison with a string path.
            inline bool operator==(std::string other) const noexcept
            {
                return mMetaModelIdentifier == other;
            }

            /// @brief Inequality comparison with another instance specifier.
            inline bool operator!=(const InstanceSpecifier &other) const noexcept
            {
                return mMetaModelIdentifier != other.mMetaModelIdentifier;
            }

            /// @brief Inequality comparison with a string path.
            inline bool operator!=(std::string other) const noexcept
            {
                return mMetaModelIdentifier != other;
            }

            /// @brief Lexicographical order comparison.
            inline bool operator<(const InstanceSpecifier &other) const noexcept
            {
                return mMetaModelIdentifier < other.mMetaModelIdentifier;
            }

            /// @brief Reverse lexicographical order comparison.
            inline bool operator>(const InstanceSpecifier &other) const noexcept
            {
                return mMetaModelIdentifier > other.mMetaModelIdentifier;
            }

            /// @brief Convert the instance to a string
            /// @returns Meta-model ID (Shortname-path)
            std::string ToString() const noexcept;

            /// @brief Serialized the object
            /// @param[out] serializedObject Serialized object byte vector
            /// @note This is not an ARA specified method.
            void Serialize(std::vector<uint8_t> &serializedObject) const;
        };

        inline bool operator==(std::string lhs, const InstanceSpecifier &rhs) noexcept
        {
            return lhs == rhs.ToString();
        }

        /// @brief Non-member inequality comparison with string on left side.
        inline bool operator!=(std::string lhs, const InstanceSpecifier &rhs) noexcept
        {
            return lhs != rhs.ToString();
        }
    }
}

#endif
