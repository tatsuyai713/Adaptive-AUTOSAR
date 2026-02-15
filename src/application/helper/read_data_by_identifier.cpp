/// @file src/application/helper/read_data_by_identifier.cpp
/// @brief Implementation for read data by identifier.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "../../ara/diag/conversation.h"
#include "./read_data_by_identifier.h"

namespace application
{
    namespace helper
    {
        const uint8_t ReadDataByIdentifier::cSid;
        const uint16_t ReadDataByIdentifier::cAverageSpeedDid;
        const uint16_t ReadDataByIdentifier::cFuelAmountDid;
        const uint16_t ReadDataByIdentifier::cExternalTemperatureDid;
        const uint16_t ReadDataByIdentifier::cAverageFuelConsumptionDid;
        const uint16_t ReadDataByIdentifier::cEngineCoolantTemperatureDid;
        const uint16_t ReadDataByIdentifier::cOdometerValueDid;
        const std::chrono::seconds ReadDataByIdentifier::cCacheLifetime{60};
        const ara::core::InstanceSpecifier ReadDataByIdentifier::cSpecifer("ReadDataByIdentifier");

        ReadDataByIdentifier::ReadDataByIdentifier() : ara::diag::routing::RoutableUdsService(cSpecifer, cSid),
                                                       mCache(cCacheLifetime)
        {
        }

        uint16_t ReadDataByIdentifier::getDid(const std::vector<uint8_t> &requestData)
        {
            const size_t cDidMsbIndex{1};
            const size_t cDidLsbIndex{2};

            auto _result{static_cast<uint16_t>(requestData.at(cDidMsbIndex) << 8)};
            _result |= requestData.at(cDidLsbIndex);

            return _result;
        }

        void ReadDataByIdentifier::generateResponse(
            uint16_t did,
            ara::diag::OperationOutput &response)
        {
            const auto cResponseSid{
                static_cast<uint8_t>(cSid + cPositiveResponseSidIncrement)};
            response.responseData.push_back(cResponseSid);

            const auto cDidMsb{static_cast<uint8_t>(did >> 8)};
            response.responseData.push_back(cDidMsb);

            const uint16_t cMask{0x00ff};
            const auto cDidLsb{static_cast<uint8_t>(did & cMask)};
            response.responseData.push_back(cDidLsb);
        }

        void ReadDataByIdentifier::getAverageSpeed(
            ara::diag::OperationOutput &response)
        {
            generateResponse(cAverageSpeedDid, response);
            const auto cAverageSpeed{static_cast<uint8_t>(60)};
            response.responseData.push_back(cAverageSpeed);
            mCache.Add(cAverageSpeedDid, response);
        }

        void ReadDataByIdentifier::getFuelAmount(
            ara::diag::OperationOutput &response)
        {
            const double cConversionGain{2.55};
            generateResponse(cFuelAmountDid, response);
            const auto cFuelAmount{static_cast<uint8_t>(cConversionGain * 35)};
            response.responseData.push_back(cFuelAmount);
            mCache.Add(cFuelAmountDid, response);
        }

        void ReadDataByIdentifier::getExternalTemperature(
            ara::diag::OperationOutput &response)
        {
            const uint8_t cCompensationValue{40};
            generateResponse(cExternalTemperatureDid, response);
            const auto cExternalTemperature{static_cast<uint8_t>(22 + cCompensationValue)};
            response.responseData.push_back(cExternalTemperature);
            mCache.Add(cExternalTemperatureDid, response);
        }

        void ReadDataByIdentifier::getAverageFuelConsumption(
            ara::diag::OperationOutput &response)
        {
            const uint16_t cConversionGain{20};
            const uint16_t cConversionBase{256};
            generateResponse(cAverageFuelConsumptionDid, response);
            const auto cAverageFuelConsumptionInt{
                static_cast<uint16_t>(cConversionGain * 7.5)};

            const auto cAverageFuelConsumptionMsb{
                static_cast<uint8_t>(cAverageFuelConsumptionInt / cConversionBase)};
            response.responseData.push_back(cAverageFuelConsumptionMsb);

            const auto cAverageFuelConsumptionLsb{
                static_cast<uint8_t>(cAverageFuelConsumptionInt % cConversionBase)};
            response.responseData.push_back(cAverageFuelConsumptionLsb);
            mCache.Add(cAverageFuelConsumptionDid, response);
        }

        void ReadDataByIdentifier::getEngineCoolantTemperature(
            ara::diag::OperationOutput &response)
        {
            const uint8_t cCompensationValue{40};
            generateResponse(cEngineCoolantTemperatureDid, response);
            const auto cEngineCoolantTemperature{static_cast<uint8_t>(90 + cCompensationValue)};
            response.responseData.push_back(cEngineCoolantTemperature);
            mCache.Add(cEngineCoolantTemperatureDid, response);
        }

        void ReadDataByIdentifier::getOdometerValue(
            ara::diag::OperationOutput &response)
        {
            constexpr size_t cCount{3};
            const uint32_t cConversionGain{10};
            const uint32_t cConversionBases[cCount]{16777216, 65536, 256};
            generateResponse(cOdometerValueDid, response);
            auto _odometerValueInt{
                static_cast<uint32_t>(cConversionGain * 15000.0)};

            for (size_t i = 0; i < cCount; ++i)
            {
                const auto cOdometerValueMsb{
                    static_cast<uint8_t>(
                        _odometerValueInt / cConversionBases[i])};

                response.responseData.push_back(cOdometerValueMsb);
                _odometerValueInt %= cConversionBases[i];
            }

            const auto cOdometerValueLsb{
                static_cast<uint8_t>(_odometerValueInt)};
            response.responseData.push_back(cOdometerValueLsb);
            mCache.Add(cOdometerValueDid, response);
        }

        std::future<ara::diag::OperationOutput> ReadDataByIdentifier::HandleMessage(
            const std::vector<uint8_t> &requestData,
            ara::diag::MetaInfo &metaInfo,
            ara::diag::CancellationHandler &&cancellationHandler)
        {
            const uint16_t cDid{getDid(requestData)};
            ara::diag::OperationOutput _response;
            std::promise<ara::diag::OperationOutput> _resultPromise;

            if (!mCache.TryGet(cDid, _response))
            {
                ara::diag::MetaInfo _metaInfo(ara::diag::Context::kDoIP);
                auto _conversation{
                    ara::diag::Conversation::GetConversation(_metaInfo)};

                switch (cDid)
                {
                case cAverageSpeedDid:
                    getAverageSpeed(_response);
                    break;
                case cFuelAmountDid:
                    getFuelAmount(_response);
                    break;
                case cExternalTemperatureDid:
                    getExternalTemperature(_response);
                    break;
                case cAverageFuelConsumptionDid:
                    getAverageFuelConsumption(_response);
                    break;
                case cEngineCoolantTemperatureDid:
                    getEngineCoolantTemperature(_response);
                    break;
                case cOdometerValueDid:
                    getOdometerValue(_response);
                    break;
                default:
                    GenerateNegativeResponse(_response, cRequestOutOfRangeNrc);
                }

                if (_conversation.HasValue())
                {
                    _conversation.Value().get().Deactivate();
                }
            }

            _resultPromise.set_value(_response);
            std::future<ara::diag::OperationOutput> _result{
                _resultPromise.get_future()};

            return _result;
        }
    }
}
