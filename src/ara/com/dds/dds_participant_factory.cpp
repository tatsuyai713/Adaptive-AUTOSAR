/// @file src/ara/com/dds/dds_participant_factory.cpp
/// @brief Implementation for shared DDS participant factory.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./dds_participant_factory.h"

namespace ara
{
    namespace com
    {
        namespace dds
        {
            DdsParticipantFactory &DdsParticipantFactory::Instance() noexcept
            {
                static DdsParticipantFactory sInstance;
                return sInstance;
            }

#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
            dds_entity_t DdsParticipantFactory::AcquireParticipant(
                std::uint32_t domainId)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto &info = mParticipants[domainId];
                if (info.RefCount == 0U)
                {
                    info.Participant = dds_create_participant(
                        static_cast<dds_domainid_t>(domainId),
                        nullptr, nullptr);
                    if (info.Participant < 0)
                    {
                        mParticipants.erase(domainId);
                        return 0;
                    }
                }
                ++info.RefCount;
                return info.Participant;
            }
#else
            int DdsParticipantFactory::AcquireParticipant(
                std::uint32_t domainId)
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto &info = mParticipants[domainId];
                if (info.RefCount == 0U)
                {
                    info.Participant = static_cast<int>(domainId + 1);
                }
                ++info.RefCount;
                return info.Participant;
            }
#endif

            void DdsParticipantFactory::ReleaseParticipant(
                std::uint32_t domainId) noexcept
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = mParticipants.find(domainId);
                if (it == mParticipants.end())
                {
                    return;
                }

                if (it->second.RefCount > 0U)
                {
                    --it->second.RefCount;
                }

                if (it->second.RefCount == 0U)
                {
#if defined(ARA_COM_USE_CYCLONEDDS) && (ARA_COM_USE_CYCLONEDDS == 1)
                    if (it->second.Participant > 0)
                    {
                        dds_delete(it->second.Participant);
                    }
#endif
                    mParticipants.erase(it);
                }
            }

            std::uint32_t DdsParticipantFactory::GetRefCount(
                std::uint32_t domainId) const noexcept
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = mParticipants.find(domainId);
                if (it == mParticipants.end())
                {
                    return 0U;
                }
                return it->second.RefCount;
            }

            std::size_t DdsParticipantFactory::ActiveDomainCount() const noexcept
            {
                std::lock_guard<std::mutex> lock(mMutex);
                return mParticipants.size();
            }
        }
    }
}
