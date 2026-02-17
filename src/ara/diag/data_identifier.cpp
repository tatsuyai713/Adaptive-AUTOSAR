/// @file src/ara/diag/data_identifier.cpp
/// @brief Implementation for ara::diag::DataIdentifierService.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./data_identifier.h"
#include <future>

namespace ara
{
    namespace diag
    {
        // -----------------------------------------------------------------------
        // Constructor
        // -----------------------------------------------------------------------
        DataIdentifierService::DataIdentifierService(
            const ara::core::InstanceSpecifier &specifier,
            ReentrancyType /*reentrancyType*/,
            uint8_t serviceId)
            : routing::RoutableUdsService{specifier, serviceId}
        {
        }

        // -----------------------------------------------------------------------
        // Handler registration
        // -----------------------------------------------------------------------
        void DataIdentifierService::RegisterReadHandler(uint16_t did, DidReadHandler handler)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mReadHandlers[did] = std::move(handler);
        }

        void DataIdentifierService::RegisterWriteHandler(uint16_t did, DidWriteHandler handler)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mWriteHandlers[did] = std::move(handler);
        }

        void DataIdentifierService::UnregisterReadHandler(uint16_t did)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mReadHandlers.erase(did);
        }

        void DataIdentifierService::UnregisterWriteHandler(uint16_t did)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mWriteHandlers.erase(did);
        }

        bool DataIdentifierService::HasReadHandler(uint16_t did) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mReadHandlers.count(did) > 0;
        }

        bool DataIdentifierService::HasWriteHandler(uint16_t did) const
        {
            std::lock_guard<std::mutex> lock(mMutex);
            return mWriteHandlers.count(did) > 0;
        }

        // -----------------------------------------------------------------------
        // HandleMessage — dispatch to read or write handler
        // -----------------------------------------------------------------------
        std::future<OperationOutput> DataIdentifierService::HandleMessage(
            const std::vector<uint8_t> &requestData,
            MetaInfo & /*metaInfo*/,
            CancellationHandler && /*cancellationHandler*/)
        {
            std::promise<OperationOutput> promise;

            if (requestData.empty())
            {
                OperationOutput out;
                GenerateNegativeResponse(out, cIncorrectMessageLength);
                promise.set_value(out);
                return promise.get_future();
            }

            const uint8_t sid = requestData[0];
            OperationOutput out;

            if (sid == cSidRead)
            {
                out = handleReadRequest(requestData);
            }
            else if (sid == cSidWrite)
            {
                out = handleWriteRequest(requestData);
            }
            else
            {
                GenerateNegativeResponse(out, cSubFunctionNotSupported);
            }

            promise.set_value(out);
            return promise.get_future();
        }

        // -----------------------------------------------------------------------
        // handleReadRequest — UDS 0x22: ReadDataByIdentifier
        // Request:  [0x22, DID_H, DID_L, (DID_H2, DID_L2, ...)]
        // Response: [0x62, DID_H, DID_L, <data>, (DID_H2, DID_L2, <data2>, ...)]
        // -----------------------------------------------------------------------
        OperationOutput DataIdentifierService::handleReadRequest(
            const std::vector<uint8_t> &request)
        {
            OperationOutput out;

            // Minimum request: [0x22, DID_H, DID_L]
            if (request.size() < 3 || ((request.size() - 1) % 2) != 0)
            {
                GenerateNegativeResponse(out, cIncorrectMessageLength);
                return out;
            }

            // Build positive response
            out.responseData.push_back(
                static_cast<uint8_t>(cSidRead + cPositiveResponseSidIncrement)); // 0x62

            const std::size_t numDids = (request.size() - 1) / 2;
            for (std::size_t i = 0; i < numDids; ++i)
            {
                const uint16_t did =
                    (static_cast<uint16_t>(request[1 + i * 2]) << 8) |
                     static_cast<uint16_t>(request[2 + i * 2]);

                DidReadHandler handler;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    auto it = mReadHandlers.find(did);
                    if (it == mReadHandlers.end())
                    {
                        // DID not supported → NRC 0x31
                        out.responseData.clear();
                        GenerateNegativeResponse(out, cNrcRequestOutOfRange);
                        return out;
                    }
                    handler = it->second;
                }

                // Invoke handler (outside lock to avoid deadlock)
                const std::vector<uint8_t> didData = handler();

                // Append DID + data to response
                out.responseData.push_back(static_cast<uint8_t>(did >> 8));
                out.responseData.push_back(static_cast<uint8_t>(did & 0xFF));
                out.responseData.insert(out.responseData.end(),
                                        didData.begin(), didData.end());
            }

            return out;
        }

        // -----------------------------------------------------------------------
        // handleWriteRequest — UDS 0x2E: WriteDataByIdentifier
        // Request:  [0x2E, DID_H, DID_L, <data bytes>]
        // Response: [0x6E, DID_H, DID_L]
        // -----------------------------------------------------------------------
        OperationOutput DataIdentifierService::handleWriteRequest(
            const std::vector<uint8_t> &request)
        {
            OperationOutput out;

            // Minimum request: [0x2E, DID_H, DID_L, at least 1 data byte]
            if (request.size() < 4)
            {
                GenerateNegativeResponse(out, cIncorrectMessageLength);
                return out;
            }

            const uint16_t did =
                (static_cast<uint16_t>(request[1]) << 8) |
                 static_cast<uint16_t>(request[2]);

            const std::vector<uint8_t> writeData(request.begin() + 3, request.end());

            DidWriteHandler handler;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                auto it = mWriteHandlers.find(did);
                if (it == mWriteHandlers.end())
                {
                    GenerateNegativeResponse(out, cNrcRequestOutOfRange);
                    return out;
                }
                handler = it->second;
            }

            // Invoke write handler (outside lock)
            const bool accepted = handler(writeData);
            if (!accepted)
            {
                GenerateNegativeResponse(out, cNrcConditionsNotCorrect);
                return out;
            }

            // Positive response: [0x6E, DID_H, DID_L]
            out.responseData.push_back(
                static_cast<uint8_t>(cSidWrite + cPositiveResponseSidIncrement)); // 0x6E
            out.responseData.push_back(static_cast<uint8_t>(did >> 8));
            out.responseData.push_back(static_cast<uint8_t>(did & 0xFF));

            return out;
        }

    } // namespace diag
} // namespace ara
