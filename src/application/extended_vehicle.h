/// @file src/application/extended_vehicle.h
/// @brief Declarations for extended vehicle.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef EXTENDED_VEHICLE_H
#define EXTENDED_VEHICLE_H

#include "../ara/exec/helper/modelled_process.h"
#include "../ara/phm/supervised_entity.h"
#include "../ara/com/someip/sd/someip_sd_server.h"
#include "./helper/network_configuration.h"
#include "./doip/doip_server.h"

namespace application
{
    /// @brief Platform health management checkpoint types
    enum class PhmCheckpointType : uint32_t
    {
        AliveCheckpoint = 0,            ///!< Alive supervision checkpoint
        DeadlineSourceCheckpoint = 1,   ///!< Deadline supervision source checkpoint
        DeadlineTargetCheckpoint = 2    ///!< Deadline supervision target checkpoint
    };

    /// @brief Extended vehicle adaptive application
    class ExtendedVehicle : public ara::exec::helper::ModelledProcess
    {
    private:
        static const std::string cAppId;
        static const ara::core::InstanceSpecifier cSeInstance;

        ara::phm::SupervisedEntity mSupervisedEntity;
        ara::com::helper::NetworkLayer<ara::com::someip::sd::SomeIpSdMessage> *mNetworkLayer;
        ara::com::someip::sd::SomeIpSdServer *mSdServer;
        doip::DoipServer *mDoipServer;

        std::string mResourcesUrl;

        void configureNetworkLayer(const arxml::ArxmlReader &reader);
        void configureMockVehicleData(std::string &vin);

        static helper::NetworkConfiguration getNetworkConfiguration(
            const arxml::ArxmlReader &reader);

        void configureSdServer(const arxml::ArxmlReader &reader);

        static DoipLib::ControllerConfig getDoipConfiguration(
            const arxml::ArxmlReader &reader);

        void configureDoipServer(
            const arxml::ArxmlReader &reader, std::string &&vin);

    protected:
        int Main(
            const std::atomic_bool *cancellationToken,
            const std::map<std::string, std::string> &arguments) override;

    public:
        /// @brief Constructor
        /// @param poller Global poller for network communication
        /// @param checkpointCommunicator Medium to communicate the supervision checkpoints
        ExtendedVehicle(
            AsyncBsdSocketLib::Poller *poller,
            ara::phm::CheckpointCommunicator *checkpointCommunicator);

        ~ExtendedVehicle() override;
    };
}

#endif
