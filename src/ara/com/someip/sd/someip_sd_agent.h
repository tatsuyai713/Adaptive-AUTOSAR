#ifndef SOMEIP_SD_AGENT_H
#define SOMEIP_SD_AGENT_H

#include <future>
#include <mutex>
#include <stdexcept>
#include "../../helper/machine_state.h"
#include "../../helper/network_layer.h"
#include "./someip_sd_message.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace sd
            {
                /// @brief SOME/IP service discovery agent (server/client base).
                template <typename T>
                class SomeIpSdAgent
                {
                private:
                    mutable std::mutex mStateMutex;
                    bool mStarted;
                    T mState;

                protected:
                    std::future<void> Future;

                    /// @brief Retained for compatibility with existing constructors.
                    helper::NetworkLayer<SomeIpSdMessage> *CommunicationLayer;

                    void SetState(T state)
                    {
                        std::lock_guard<std::mutex> _stateLock(mStateMutex);
                        mState = state;
                    }

                    virtual void StartAgent(T state) = 0;
                    virtual void StopAgent() = 0;

                public:
                    SomeIpSdAgent(
                        helper::NetworkLayer<SomeIpSdMessage> *networkLayer,
                        T initialState) : mStarted{false},
                                          mState{initialState},
                                          CommunicationLayer{networkLayer}
                    {
                    }

                    void Start()
                    {
                        if (mStarted)
                        {
                            throw std::logic_error(
                                "The state has been already activated.");
                        }

                        StartAgent(GetState());
                        mStarted = true;
                    }

                    T GetState() const noexcept
                    {
                        std::lock_guard<std::mutex> _stateLock(mStateMutex);
                        return mState;
                    }

                    void Join()
                    {
                        if (Future.valid())
                        {
                            Future.wait();
                        }
                    }

                    void Stop()
                    {
                        StopAgent();
                        mStarted = false;
                    }

                    virtual ~SomeIpSdAgent() noexcept = default;
                };
            }
        }
    }
}

#endif
