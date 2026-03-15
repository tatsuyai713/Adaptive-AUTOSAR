/// @file src/ara/com/dds/dds_participant_factory.h
/// @brief Shared DDS DomainParticipant factory.
/// @details Per DDS specification, creating multiple DomainParticipants
///          for the same domain within the same process is wasteful.
///          This factory provides a shared participant per domain ID,
///          reference-counted, so that multiple event/method bindings
///          can share the same underlying DDS participant.
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_DDS_PARTICIPANT_FACTORY_H
#define ARA_COM_DDS_PARTICIPANT_FACTORY_H

#include <cstdint>
#include <map>
#include <mutex>

#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
#include <dds/dds.h>
#endif

namespace ara
{
    namespace com
    {
        namespace dds
        {
            /// @brief Participant reference info for tracking shared participants.
            struct ParticipantInfo
            {
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                dds_entity_t Participant{0};
#else
                int Participant{0};
#endif
                std::uint32_t RefCount{0U};
            };

            /// @brief Thread-safe shared DDS participant factory.
            ///        Returns the same participant handle for the same domain ID,
            ///        reference-counted, with cleanup on last release.
            class DdsParticipantFactory
            {
            private:
                std::map<std::uint32_t, ParticipantInfo> mParticipants;
                mutable std::mutex mMutex;

                DdsParticipantFactory() = default;

            public:
                DdsParticipantFactory(const DdsParticipantFactory &) = delete;
                DdsParticipantFactory &operator=(const DdsParticipantFactory &) = delete;

                /// @brief Get the singleton factory instance.
                static DdsParticipantFactory &Instance() noexcept;

                /// @brief Acquire a shared participant for the given domain.
                /// @param domainId DDS domain ID
                /// @returns Participant handle (or 0 on error)
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                dds_entity_t AcquireParticipant(std::uint32_t domainId);
#else
                int AcquireParticipant(std::uint32_t domainId);
#endif

                /// @brief Release a previously acquired participant.
                /// @param domainId DDS domain ID
                /// @details Decrements reference count; deletes participant when zero.
                void ReleaseParticipant(std::uint32_t domainId) noexcept;

                /// @brief Get the current reference count for a domain.
                std::uint32_t GetRefCount(std::uint32_t domainId) const noexcept;

                /// @brief Get the number of active domains.
                std::size_t ActiveDomainCount() const noexcept;
            };
        }
    }
}

#endif
