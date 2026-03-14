/// @file src/ara/com/service_version.h
/// @brief Service versioning and compatibility checking per SWS_CM_00150.
/// @details Defines ServiceVersion type for major/minor version tracking
///          and compatibility policy for proxy-skeleton version negotiation.

#ifndef ARA_COM_SERVICE_VERSION_H
#define ARA_COM_SERVICE_VERSION_H

#include <cstdint>
#include <string>

namespace ara
{
    namespace com
    {
        /// @brief Version compatibility policy per AUTOSAR AP.
        enum class VersionCheckPolicy : std::uint8_t
        {
            kExact = 0U,           ///< Major and minor must match exactly.
            kMajorOnly = 1U,       ///< Only major version must match.
            kMinorBackward = 2U,   ///< Major must match; skeleton minor >= proxy minor.
            kNoCheck = 3U          ///< No version checking performed.
        };

        /// @brief Service interface version descriptor.
        struct ServiceVersion
        {
            std::uint8_t MajorVersion{1U};
            std::uint32_t MinorVersion{0U};

            /// @brief Construct a version.
            /// @param major Major version number.
            /// @param minor Minor version number.
            ServiceVersion(std::uint8_t major = 1U,
                           std::uint32_t minor = 0U) noexcept
                : MajorVersion{major}, MinorVersion{minor}
            {
            }

            /// @brief Check compatibility with another version.
            /// @param other The version to check against (typically skeleton's version).
            /// @param policy Compatibility policy to apply.
            /// @returns True if versions are compatible per the given policy.
            bool IsCompatibleWith(const ServiceVersion &other,
                                  VersionCheckPolicy policy =
                                      VersionCheckPolicy::kMinorBackward) const noexcept
            {
                switch (policy)
                {
                case VersionCheckPolicy::kExact:
                    return MajorVersion == other.MajorVersion &&
                           MinorVersion == other.MinorVersion;

                case VersionCheckPolicy::kMajorOnly:
                    return MajorVersion == other.MajorVersion;

                case VersionCheckPolicy::kMinorBackward:
                    return MajorVersion == other.MajorVersion &&
                           other.MinorVersion >= MinorVersion;

                case VersionCheckPolicy::kNoCheck:
                    return true;

                default:
                    return false;
                }
            }

            /// @brief Convert to string representation.
            /// @returns Version string in "major.minor" format.
            std::string ToString() const
            {
                return std::to_string(MajorVersion) + "." +
                       std::to_string(MinorVersion);
            }

            bool operator==(const ServiceVersion &other) const noexcept
            {
                return MajorVersion == other.MajorVersion &&
                       MinorVersion == other.MinorVersion;
            }

            bool operator!=(const ServiceVersion &other) const noexcept
            {
                return !(*this == other);
            }

            bool operator<(const ServiceVersion &other) const noexcept
            {
                if (MajorVersion != other.MajorVersion)
                {
                    return MajorVersion < other.MajorVersion;
                }
                return MinorVersion < other.MinorVersion;
            }
        };
    }
}

#endif
