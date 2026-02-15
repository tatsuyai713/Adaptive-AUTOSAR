#ifndef ARA_COM_DDS_DDS_PUBSUB_H
#define ARA_COM_DDS_DDS_PUBSUB_H

#include "./cyclonedds_pubsub.h"

namespace ara
{
    namespace com
    {
        namespace dds
        {
            /// @brief Vendor-neutral DDS publisher abstraction for ara::com users.
            ///        Current binding is Cyclone DDS.
            template <typename SampleType>
            using DdsPublisher = CyclonePublisher<SampleType>;

            /// @brief Vendor-neutral DDS subscriber abstraction for ara::com users.
            ///        Current binding is Cyclone DDS.
            template <typename SampleType>
            using DdsSubscriber = CycloneSubscriber<SampleType>;
        }
    }
}

#endif
