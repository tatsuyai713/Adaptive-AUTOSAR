#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include "./vsomeip_application.h"

namespace ara
{
    namespace com
    {
        namespace someip
        {
            namespace
            {
                class ManagedApplication
                {
                private:
                    std::mutex mMutex;
                    std::condition_variable mConditionVariable;
                    std::thread mThread;
                    std::shared_ptr<vsomeip::application> mApplication;
                    bool mRegistered;

                public:
                    ManagedApplication() noexcept : mRegistered{false}
                    {
                    }

                    ~ManagedApplication() noexcept
                    {
                        Stop();
                    }

                    std::shared_ptr<vsomeip::application> EnsureRunning(
                        std::string appName, bool setRoutingManager)
                    {
                        const std::chrono::seconds cRegistrationTimeout{10};

                        std::unique_lock<std::mutex> _lock(mMutex);
                        if (mApplication)
                        {
                            return mApplication;
                        }

                        if (setRoutingManager &&
                            std::getenv("VSOMEIP_ROUTING") == nullptr)
                        {
                            setenv("VSOMEIP_ROUTING", appName.c_str(), 0);
                        }

                        mApplication =
                            vsomeip::runtime::get()->create_application(appName);
                        if (!mApplication)
                        {
                            throw std::runtime_error(
                                "Failed to create vsomeip application: " + appName);
                        }

                        if (!mApplication->init())
                        {
                            throw std::runtime_error(
                                "Failed to initialize vsomeip application: " + appName);
                        }

                        mApplication->register_state_handler(
                            [this](vsomeip::state_type_e state)
                            {
                                if (state == vsomeip::state_type_e::ST_REGISTERED)
                                {
                                    std::lock_guard<std::mutex> _registrationLock(mMutex);
                                    mRegistered = true;
                                    mConditionVariable.notify_all();
                                }
                            });

                        mThread =
                            std::thread(
                                [this]()
                                {
                                    mApplication->start();
                                });

                        bool _registered{
                            mConditionVariable.wait_for(
                                _lock,
                                cRegistrationTimeout,
                                [this]()
                                { return mRegistered; })};

                        if (!_registered)
                        {
                            std::cerr
                                << "[WARN] vsomeip registration timeout."
                                << std::endl;
                        }

                        return mApplication;
                    }

                    void Stop() noexcept
                    {
                        std::unique_lock<std::mutex> _lock(mMutex);
                        auto _application{mApplication};
                        _lock.unlock();

                        if (_application)
                        {
                            _application->stop();
                        }

                        if (mThread.joinable())
                        {
                            if (mThread.get_id() == std::this_thread::get_id())
                            {
                                mThread.detach();
                            }
                            else
                            {
                                mThread.join();
                            }
                        }

                        _lock.lock();
                        mApplication.reset();
                        mRegistered = false;
                    }
                };

                std::string GetEnvOrDefault(const char *key, std::string fallback)
                {
                    const char *cValue{std::getenv(key)};
                    return cValue ? cValue : std::move(fallback);
                }

                bool IsReadableFile(const char *filepath)
                {
                    if (filepath == nullptr)
                    {
                        return false;
                    }

                    std::ifstream _stream(filepath);
                    return static_cast<bool>(_stream);
                }

                void EnsureConfiguration()
                {
                    if (std::getenv("VSOMEIP_CONFIGURATION") != nullptr)
                    {
                        return;
                    }

                    const char *cCustomPath{
                        std::getenv("ADAPTIVE_AUTOSAR_VSOMEIP_CONFIG")};
                    const char *cCandidates[]{
                        cCustomPath,
                        "./configuration/vsomeip-local.json",
                        "../configuration/vsomeip-local.json"};

                    for (const char *cCandidate : cCandidates)
                    {
                        if (IsReadableFile(cCandidate))
                        {
                            setenv("VSOMEIP_CONFIGURATION", cCandidate, 0);
                            break;
                        }
                    }
                }

                ManagedApplication &GetServerContext() noexcept
                {
                    static ManagedApplication sServerContext;
                    return sServerContext;
                }

                ManagedApplication &GetClientContext() noexcept
                {
                    static ManagedApplication sClientContext;
                    return sClientContext;
                }

                void RegisterAtexit()
                {
                    static const bool cRegistered{
                        []()
                        {
                            std::atexit(
                                []()
                                {
                                    VsomeipApplication::StopAll();
                                });
                            return true;
                        }()};

                    (void)cRegistered;
                }
            }

            std::shared_ptr<vsomeip::application> VsomeipApplication::GetServerApplication()
            {
                RegisterAtexit();
                EnsureConfiguration();
                const std::string cServerAppName{
                    GetEnvOrDefault(
                        "ADAPTIVE_AUTOSAR_VSOMEIP_SERVER_APP",
                        "adaptive_autosar_server")};

                return GetServerContext().EnsureRunning(cServerAppName, true);
            }

            std::shared_ptr<vsomeip::application> VsomeipApplication::GetClientApplication()
            {
                RegisterAtexit();
                EnsureConfiguration();
                const std::string cClientAppName{
                    GetEnvOrDefault(
                        "ADAPTIVE_AUTOSAR_VSOMEIP_CLIENT_APP",
                        "adaptive_autosar_client")};

                return GetClientContext().EnsureRunning(cClientAppName, false);
            }

            void VsomeipApplication::StopAll() noexcept
            {
                GetClientContext().Stop();
                GetServerContext().Stop();
            }
        }
    }
}
