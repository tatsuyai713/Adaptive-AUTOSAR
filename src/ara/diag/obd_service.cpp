/// @file src/ara/diag/obd_service.cpp
/// @brief OBD-II Service handler implementation.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./obd_service.h"
#include <stdexcept>
#include <algorithm>

namespace ara
{
    namespace diag
    {
        // -----------------------------------------------------------------------
        // Constructor
        // -----------------------------------------------------------------------
        ObdService::ObdService(ObdDataProvider *dataProvider)
            : mDataProvider{dataProvider}
        {
            if (mDataProvider == nullptr)
            {
                throw std::invalid_argument("ObdService: dataProvider must not be null");
            }
        }

        // -----------------------------------------------------------------------
        // IsPidSupported
        // -----------------------------------------------------------------------
        bool ObdService::IsPidSupported(uint8_t pid) const noexcept
        {
            switch (pid)
            {
            case 0x00: // Supported PIDs [01-20]
            case 0x01: // Monitor status
            case 0x04: // Engine load
            case 0x05: // Coolant temperature
            case 0x0B: // Intake manifold pressure
            case 0x0C: // Engine speed
            case 0x0D: // Vehicle speed
            case 0x0F: // Intake air temperature
            case 0x10: // MAF rate
            case 0x11: // Throttle position
            case 0x1C: // OBD standard
            case 0x20: // Supported PIDs [21-40]
                return true;
            default:
                return false;
            }
        }

        // -----------------------------------------------------------------------
        // Encoding helpers
        // -----------------------------------------------------------------------
        uint32_t ObdService::buildSupportedPids0120() const noexcept
        {
            // Bit N (MSB=bit31) corresponds to PID 0x01+N
            // Bit31=PID01, Bit30=PID02, ..., Bit0=PID20
            uint32_t mask = 0;
            mask |= (1U << 30); // PID 0x01 - monitor status
            mask |= (1U << 27); // PID 0x04 - engine load
            mask |= (1U << 26); // PID 0x05 - coolant temp
            mask |= (1U << 20); // PID 0x0B - intake pressure
            mask |= (1U << 19); // PID 0x0C - engine speed
            mask |= (1U << 18); // PID 0x0D - vehicle speed
            mask |= (1U << 16); // PID 0x0F - intake air temp
            mask |= (1U << 15); // PID 0x10 - MAF
            mask |= (1U << 14); // PID 0x11 - throttle
            mask |= (1U << 3);  // PID 0x1C - OBD standard
            mask |= (1U << 0);  // PID 0x20 - supported PIDs [21-40] (indicates more follow)
            return mask;
        }

        std::vector<uint8_t> ObdService::encodeEngineSpeed(float rpm) const noexcept
        {
            // Engine speed = (A*256 + B) / 4 RPM
            // → A*256 + B = rpm * 4
            const auto raw = static_cast<uint16_t>(
                std::min(65535.0f, std::max(0.0f, rpm * 4.0f)));
            return {static_cast<uint8_t>(raw >> 8),
                    static_cast<uint8_t>(raw & 0xFF)};
        }

        uint8_t ObdService::encodeEngineLoad(float loadPercent) const noexcept
        {
            // Engine load = A * 100 / 255 %
            // → A = load * 255 / 100
            return static_cast<uint8_t>(
                std::min(255.0f, std::max(0.0f, loadPercent * 255.0f / 100.0f)));
        }

        uint8_t ObdService::encodeCoolantTemp(int16_t tempCelsius) const noexcept
        {
            // Coolant temp = A - 40 °C → A = temp + 40
            const int val = static_cast<int>(tempCelsius) + 40;
            return static_cast<uint8_t>(std::min(255, std::max(0, val)));
        }

        std::vector<uint8_t> ObdService::encodeMafRate(float gramPerSec) const noexcept
        {
            // MAF = (A*256 + B) / 100 g/s
            const auto raw = static_cast<uint16_t>(
                std::min(65535.0f, std::max(0.0f, gramPerSec * 100.0f)));
            return {static_cast<uint8_t>(raw >> 8),
                    static_cast<uint8_t>(raw & 0xFF)};
        }

        uint8_t ObdService::encodeThrottlePos(float percent) const noexcept
        {
            // Throttle = A * 100 / 255 % → A = throttle * 255 / 100
            return static_cast<uint8_t>(
                std::min(255.0f, std::max(0.0f, percent * 255.0f / 100.0f)));
        }

        // -----------------------------------------------------------------------
        // HandleMode01Request — Show Current Data
        // -----------------------------------------------------------------------
        ara::core::Result<std::vector<uint8_t>> ObdService::HandleMode01Request(
            const std::vector<uint8_t> &request)
        {
            // request[0] = 0x01 (mode), request[1] = PID
            if (request.size() < 2)
            {
                return ara::core::Result<std::vector<uint8_t>>::FromError(
                    MakeErrorCode(DiagErrc::kRequestFailed));
            }

            const uint8_t pid = request[1];
            std::vector<uint8_t> response;

            // Response header: Mode + 0x40 (positive), PID
            response.push_back(static_cast<uint8_t>(0x41)); // Mode 01 positive response
            response.push_back(pid);

            switch (pid)
            {
            case 0x00:
            {
                // Supported PIDs [01-20] — 4 bytes bitmask
                const uint32_t mask = buildSupportedPids0120();
                response.push_back(static_cast<uint8_t>((mask >> 24) & 0xFF));
                response.push_back(static_cast<uint8_t>((mask >> 16) & 0xFF));
                response.push_back(static_cast<uint8_t>((mask >> 8) & 0xFF));
                response.push_back(static_cast<uint8_t>(mask & 0xFF));
                break;
            }
            case 0x01:
            {
                // Monitor status since DTCs cleared [4 bytes]
                const bool milOn = mDataProvider->IsMilOn();
                const uint8_t dtcCount = mDataProvider->GetActiveDtcCount();
                // Byte A: bit7=MIL, bits[6:0]=DTC count
                const uint8_t byteA =
                    static_cast<uint8_t>((milOn ? 0x80 : 0x00) | (dtcCount & 0x7F));
                response.push_back(byteA);
                response.push_back(0x00); // Continuous monitors supported
                response.push_back(0x00); // Continuous monitors ready
                response.push_back(0x00); // Non-continuous monitors
                break;
            }
            case 0x04:
                response.push_back(encodeEngineLoad(mDataProvider->GetEngineLoadPercent()));
                break;

            case 0x05:
                response.push_back(encodeCoolantTemp(mDataProvider->GetCoolantTemperatureCelsius()));
                break;

            case 0x0B:
                response.push_back(mDataProvider->GetIntakePressureKPa());
                break;

            case 0x0C:
            {
                const auto rpmBytes = encodeEngineSpeed(mDataProvider->GetEngineSpeedRpm());
                response.insert(response.end(), rpmBytes.begin(), rpmBytes.end());
                break;
            }
            case 0x0D:
                response.push_back(mDataProvider->GetVehicleSpeedKmh());
                break;

            case 0x0F:
                response.push_back(encodeCoolantTemp(mDataProvider->GetIntakeAirTemperatureCelsius()));
                break;

            case 0x10:
            {
                const auto mafBytes = encodeMafRate(mDataProvider->GetMafGramPerSecond());
                response.insert(response.end(), mafBytes.begin(), mafBytes.end());
                break;
            }
            case 0x11:
                response.push_back(encodeThrottlePos(mDataProvider->GetThrottlePositionPercent()));
                break;

            case 0x1C:
                response.push_back(static_cast<uint8_t>(mDataProvider->GetObdStandard()));
                break;

            case 0x20:
            {
                // Supported PIDs [21-40] — return 0 (none supported beyond 0x20)
                response.push_back(0x00);
                response.push_back(0x00);
                response.push_back(0x00);
                response.push_back(0x00);
                break;
            }
            default:
                // Negative response: serviceNotSupported / requestOutOfRange
                return ara::core::Result<std::vector<uint8_t>>::FromError(
                    MakeErrorCode(DiagErrc::kRequestFailed));
            }

            return response;
        }

        // -----------------------------------------------------------------------
        // HandleMode09Request — Request Vehicle Information
        // -----------------------------------------------------------------------
        ara::core::Result<std::vector<uint8_t>> ObdService::HandleMode09Request(
            const std::vector<uint8_t> &request)
        {
            if (request.size() < 2)
            {
                return ara::core::Result<std::vector<uint8_t>>::FromError(
                    MakeErrorCode(DiagErrc::kRequestFailed));
            }

            const uint8_t infoType = request[1];
            std::vector<uint8_t> response;
            response.push_back(0x49); // Mode 09 positive response
            response.push_back(infoType);

            if (infoType == 0x02)
            {
                // VIN (17 characters, count = 1)
                response.push_back(0x01); // count
                const std::string vin = mDataProvider->GetVin();
                for (char c : vin)
                {
                    response.push_back(static_cast<uint8_t>(c));
                }
                // Pad or truncate to 17 bytes
                while (response.size() < 3 + 17)
                {
                    response.push_back(0x00);
                }
                if (response.size() > 3 + 17)
                {
                    response.resize(3 + 17);
                }
            }
            else
            {
                return ara::core::Result<std::vector<uint8_t>>::FromError(
                    MakeErrorCode(DiagErrc::kRequestFailed));
            }

            return response;
        }

    } // namespace diag
} // namespace ara
