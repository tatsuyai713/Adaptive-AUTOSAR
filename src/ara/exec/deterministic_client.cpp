/// @file src/ara/exec/deterministic_client.cpp
/// @brief Implementation for deterministic client.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include <thread>
#include "./deterministic_client.h"

namespace ara
{
    namespace exec
    {
        const uint64_t DeterministicClient::cCycleDelayMs;
        std::atomic_uint8_t DeterministicClient::mCounter;
        std::future<void> DeterministicClient::mFuture;
        bool DeterministicClient::mRunning;
        std::mutex DeterministicClient::mCycleMutex;
        std::condition_variable DeterministicClient::mCycleConditionVariable;
        std::default_random_engine DeterministicClient::mGenerator;
        std::uniform_int_distribution<uint64_t> DeterministicClient::mDistribution;
        helper::AtomicOptional<uint64_t> DeterministicClient::mSeed;
        uint64_t DeterministicClient::mRandomNumber;
        DeterministicClient::TimeStamp  DeterministicClient::mActivationTime;
        std::atomic_bool DeterministicClient::mTerminationRequested{false};

        DeterministicClient::DeterministicClient() : mLifecycleState{ActivationReturnType::kRegisterService}
        {
            // Atomically increment and check whether this is the first instance
            uint8_t _previousCount = mCounter.fetch_add(1, std::memory_order_acq_rel);

            if (_previousCount == 0)
            {
                mRunning = true;
                mTerminationRequested = false;

                mFuture =
                    std::async(
                        std::launch::async, &DeterministicClient::activateCycle);
            }
        }

        void DeterministicClient::activateCycle()
        {
            const std::chrono::milliseconds cCycleDuration{cCycleDelayMs};

            while (mRunning)
            {
                // Apply the seed if it was requested
                if (mSeed.HasValue())
                {
                    mGenerator.seed(mSeed.Value());
                    mSeed.Reset();
                }

                mRandomNumber = mDistribution(mGenerator);
                mActivationTime = std::chrono::steady_clock::now();
                mCycleConditionVariable.notify_all();

                std::this_thread::sleep_for(cCycleDuration);
            }
        }

        ActivationReturnType DeterministicClient::advanceLifecycle()
        {
            ActivationReturnType _current = mLifecycleState;

            switch (mLifecycleState)
            {
            case ActivationReturnType::kRegisterService:
                mLifecycleState = ActivationReturnType::kServiceDiscovery;
                break;
            case ActivationReturnType::kServiceDiscovery:
                mLifecycleState = ActivationReturnType::kInit;
                break;
            case ActivationReturnType::kInit:
                mLifecycleState = ActivationReturnType::kRun;
                break;
            case ActivationReturnType::kRun:
            case ActivationReturnType::kTerminate:
                // kRun stays in kRun; kTerminate stays in kTerminate
                break;
            }

            return _current;
        }

        void DeterministicClient::RequestTerminate() noexcept
        {
            mTerminationRequested = true;
            mCycleConditionVariable.notify_all();
        }

        core::Result<ActivationReturnType> DeterministicClient::WaitForActivation()
        {
            std::unique_lock<std::mutex> _lock(mCycleMutex);
            mCycleConditionVariable.wait(_lock);

            if (mTerminationRequested || !mRunning)
            {
                mLifecycleState = ActivationReturnType::kTerminate;
                core::Result<ActivationReturnType> _result{ActivationReturnType::kTerminate};
                return _result;
            }

            ActivationReturnType _activationType = advanceLifecycle();
            core::Result<ActivationReturnType> _result{_activationType};
            return _result;
        }

        uint64_t DeterministicClient::GetRandom() noexcept
        {
            return mRandomNumber;
        }

        void DeterministicClient::SetRandomSeed(uint64_t seed) noexcept
        {
            mSeed = seed;
        }

        core::Result<DeterministicClient::TimeStamp> DeterministicClient::GetActivationTime() noexcept
        {
            core::Result<TimeStamp> _result{mActivationTime};
            return _result;
        }

        core::Result<DeterministicClient::TimeStamp> DeterministicClient::GetNextActivationTime()
        {
            const std::chrono::milliseconds cCycleDuration{cCycleDelayMs};
            // Estimate the next activation time based on the cycle duration
            TimeStamp _nextActivationTime = mActivationTime + cCycleDuration;
            core::Result<TimeStamp> _result{_nextActivationTime};

            return _result;
        }

        DeterministicClient::~DeterministicClient()
        {
            // Atomically decrement and check whether this is the last instance
            uint8_t _previousCount = mCounter.fetch_sub(1, std::memory_order_acq_rel);

            if (_previousCount == 1)
            {
                mRunning = false;

                // Wake up any blocked WaitForActivation callers
                mCycleConditionVariable.notify_all();

                // Wait for the cycling thread to exit gracefully
                mFuture.get();
            }
        }
    }
}