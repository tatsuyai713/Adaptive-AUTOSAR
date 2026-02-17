/// @file src/ara/diag/obd_service.h
/// @brief OBD-II (ISO 15031-5 / SAE J1979) Service handler.
/// @details Provides handlers for OBD-II Mode 01 (current data) and Mode 03/07
///          (DTC retrieval), commonly used in vehicle diagnostics.
///
///          Supported PIDs (Mode 01):
///          - 0x00: Supported PIDs [01-20]
///          - 0x01: Monitor status since DTCs cleared
///          - 0x04: Calculated engine load (%)
///          - 0x05: Engine coolant temperature (°C)
///          - 0x0B: Intake manifold absolute pressure (kPa)
///          - 0x0C: Engine speed (RPM)
///          - 0x0D: Vehicle speed (km/h)
///          - 0x0F: Intake air temperature (°C)
///          - 0x10: Mass air flow rate (g/s)
///          - 0x11: Throttle position (%)
///          - 0x1C: OBD standards compliance
///          - 0x20: Supported PIDs [21-40]
///          - 0x49: Vehicle Identification Number (VIN)
///
///          Reference: ISO 15031-5:2015, SAE J1979
///          Reference: AUTOSAR_SWS_DiagnosticCommunicationManager (UDS/OBD overlay)
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_DIAG_OBD_SERVICE_H
#define ARA_DIAG_OBD_SERVICE_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "../core/result.h"
#include "./diag_error_domain.h"

namespace ara
{
    namespace diag
    {
        /// @brief OBD-II standard compliance identifier (Mode 01, PID 0x1C).
        enum class ObdStandard : uint8_t
        {
            kObd = 0x01,            ///< OBD-II (USA)
            kObd1 = 0x02,           ///< OBD-I (USA)
            kObdAndObdII = 0x03,    ///< OBD + OBD-II (USA)
            kEobd = 0x05,           ///< EOBD (Europe)
            kEobdAndObd = 0x06,     ///< EOBD + OBD (Europe/USA)
            kJobd = 0x09,           ///< JOBD (Japan)
            kWwobd = 0x11,          ///< World-Wide OBD
        };

        /// @brief Live OBD-II vehicle data provider interface.
        /// @details Implement this interface to supply real-time vehicle data to the
        ///          OBD-II service handler. The handler calls these functions on
        ///          receipt of Mode 01 PID requests.
        class ObdDataProvider
        {
        public:
            virtual ~ObdDataProvider() = default;

            /// @brief Engine speed in RPM (Mode 01, PID 0x0C).
            /// @returns RPM value in range [0, 16383.75].
            virtual float GetEngineSpeedRpm() const noexcept { return 0.0f; }

            /// @brief Vehicle speed in km/h (Mode 01, PID 0x0D).
            virtual uint8_t GetVehicleSpeedKmh() const noexcept { return 0; }

            /// @brief Engine coolant temperature in °C (Mode 01, PID 0x05).
            /// @returns Temperature in range [-40, 215] °C. Raw value = temp + 40.
            virtual int16_t GetCoolantTemperatureCelsius() const noexcept { return -40; }

            /// @brief Calculated engine load % (Mode 01, PID 0x04).
            /// @returns Load in range [0.0, 100.0] %.
            virtual float GetEngineLoadPercent() const noexcept { return 0.0f; }

            /// @brief Intake manifold absolute pressure in kPa (Mode 01, PID 0x0B).
            virtual uint8_t GetIntakePressureKPa() const noexcept { return 101; }

            /// @brief Intake air temperature in °C (Mode 01, PID 0x0F).
            virtual int16_t GetIntakeAirTemperatureCelsius() const noexcept { return 25; }

            /// @brief Mass air flow rate in g/s (Mode 01, PID 0x10).
            virtual float GetMafGramPerSecond() const noexcept { return 0.0f; }

            /// @brief Throttle position % (Mode 01, PID 0x11).
            virtual float GetThrottlePositionPercent() const noexcept { return 0.0f; }

            /// @brief Vehicle Identification Number (Mode 09, PID 0x02).
            virtual std::string GetVin() const noexcept { return "00000000000000000"; }

            /// @brief Number of active DTCs (for Mode 01 PID 0x01).
            virtual uint8_t GetActiveDtcCount() const noexcept { return 0; }

            /// @brief Check if MIL (Malfunction Indicator Lamp) is on.
            virtual bool IsMilOn() const noexcept { return false; }

            /// @brief OBD standard compliance (Mode 01, PID 0x1C).
            virtual ObdStandard GetObdStandard() const noexcept
            {
                return ObdStandard::kWwobd;
            }
        };

        /// @brief OBD-II service handler for Mode 01/02/03/07/09.
        /// @details Processes OBD-II requests and builds responses using data from
        ///          an ObdDataProvider. Used to integrate OBD-II service into a
        ///          UDS routing framework (via GenericUdsService handler).
        ///
        /// @example
        /// @code
        /// class MyEcuData : public ara::diag::ObdDataProvider {
        ///     float GetEngineSpeedRpm() const noexcept override { return current_rpm_; }
        ///     uint8_t GetVehicleSpeedKmh() const noexcept override { return speed_kmh_; }
        ///     // ...
        /// };
        ///
        /// MyEcuData dataSource;
        /// ara::diag::ObdService obdService(&dataSource);
        ///
        /// // On receiving Mode 01 request with PID 0x0C:
        /// auto response = obdService.HandleMode01Request({0x01, 0x0C});
        /// @endcode
        class ObdService
        {
        public:
            /// @brief Construct with an OBD data provider.
            /// @param dataProvider Pointer to the data source (must outlive ObdService).
            explicit ObdService(ObdDataProvider *dataProvider);

            ~ObdService() = default;

            /// @brief Handle OBD-II Mode 01 (Show current data) request.
            /// @param request Request bytes: [Mode(0x01), PID1, PID2, ...]
            /// @returns Response bytes or error if PID is unsupported.
            ara::core::Result<std::vector<uint8_t>> HandleMode01Request(
                const std::vector<uint8_t> &request);

            /// @brief Handle OBD-II Mode 09 (Request vehicle information) request.
            /// @param request Request bytes: [Mode(0x09), InfoType, ...]
            /// @returns Response bytes or error if InfoType is unsupported.
            ara::core::Result<std::vector<uint8_t>> HandleMode09Request(
                const std::vector<uint8_t> &request);

            /// @brief Check if a Mode 01 PID is supported.
            bool IsPidSupported(uint8_t pid) const noexcept;

        private:
            ObdDataProvider *mDataProvider;

            /// @brief Build supported PID bitmask for PIDs 0x01-0x20.
            uint32_t buildSupportedPids0120() const noexcept;

            /// @brief Encode engine speed (RPM) per OBD-II encoding: A=rpm/64, B=rpm%64
            std::vector<uint8_t> encodeEngineSpeed(float rpm) const noexcept;

            /// @brief Encode engine load %: A = load * 255 / 100
            uint8_t encodeEngineLoad(float loadPercent) const noexcept;

            /// @brief Encode coolant temperature: A = temp + 40
            uint8_t encodeCoolantTemp(int16_t tempCelsius) const noexcept;

            /// @brief Encode MAF rate: A,B = maf * 100 (big-endian)
            std::vector<uint8_t> encodeMafRate(float gramPerSec) const noexcept;

            /// @brief Encode throttle position: A = throttle * 255 / 100
            uint8_t encodeThrottlePos(float percent) const noexcept;
        };

    } // namespace diag
} // namespace ara

#endif // ARA_DIAG_OBD_SERVICE_H
