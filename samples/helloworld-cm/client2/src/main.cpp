// Disclaimer
//
// This work (specification and/or software implementation) and the material
// contained in it, as released by AUTOSAR, is for the purpose of information
// only. AUTOSAR and the companies that have contributed to it shall not be
// liable for any use of the work.
//
// The material contained in this work is protected by copyright and other
// types of intellectual property rights. The commercial exploitation of the
// material contained in this work requires a license to such intellectual
// property rights.
//
// This work may be utilized or reproduced without any modification, in any
// form or by any means, for informational purposes only. For any other
// purpose, no part of the work may be utilized or reproduced, in any form
// or by any means, without permission in writing from the publisher.
//
// The work has been developed for automotive applications only. It has
// neither been developed, nor tested for non-automotive applications.
//
// The word AUTOSAR and the AUTOSAR logo are registered trademarks.
// --------------------------------------------------------------------------

/// ===========================================================================================
///
/// @file       main.cpp
/// @brief
/// @details
/// @date       2026-09-14
/// @author     hh
/// @version    1.0.0
///
/// ===========================================================================================

#include <ara/exec/execution_client.h>
#include <ara/log/logger.h>

#include <atomic>
#include <csignal>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "ara/core/initialization.h"
#include "ara/core/promise.h"
#include "hello/world/cm/servicehelloworld_proxy.h"

namespace {

// Atomic flag for exit after SIGTERM caught
std::atomic_bool continueExecution{true};

struct EventCallbacks {
    std::mutex mutex;
    bool active{true};
};

const char* SubscriptionStateName(ara::com::SubscriptionState state)
{
    switch (state) {
    case ara::com::SubscriptionState::kSubscribed: return "Subscribed";
    case ara::com::SubscriptionState::kNotSubscribed: return "NotSubscribed";
    case ara::com::SubscriptionState::kSubscriptionPending: return "SubscriptionPending";
    }
    return "Unknown";
}

void SigTermHandler(int signal)
{
    if (signal == SIGTERM) {
        // set atomic exit flag
        continueExecution = false;
    }
}

bool RegisterSigTermHandler()
{
    struct sigaction sa;
    sa.sa_handler = SigTermHandler;
    sa.sa_flags   = 0;
    sigemptyset(&sa.sa_mask);
    // register signal handler
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        // Could not register a SIGTERM signal handler
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!ara::core::Initialize()) {
        return EXIT_FAILURE;
    }

    ara::log::Logger &logger = ara::log::CreateLogger("#COM", "com-client2d", ara::log::LogLevel::kDebug);

    if (!RegisterSigTermHandler()) {
        logger.LogError() << "iSOFT for CAPI: Helloworld-cm-Client2 Unable to register signal handler";
    }

    int exitCode = EXIT_SUCCESS;
    {  // auto release
        ara::exec::ExecutionClient{}.ReportExecutionState(ara::exec::ExecutionState::kRunning);
        logger.LogInfo() << "iSOFT for CAPI: Helloworld-cm-Client2 ReportExecutionState kRunning";

        using Proxy = hello::world::cm::proxy::ServiceHelloWorldProxy;
        ara::core::Promise< Proxy > promise;  // stack var!!
        auto future = promise.get_future();
        Proxy::StartFindService(
            [&promise, &logger](auto handles, auto handler) {
                if (handles.empty())
                    return;
                static bool promise_already_satisfied{false};
                if (promise_already_satisfied)
                    return;
                promise_already_satisfied = true;
                logger.LogInfo() << "iSOFT for CAPI: Helloworld-cm-Client2 StartFindService CB called";
                promise.set_value(std::move(Proxy::Create(handles[0])).Value());
                Proxy::StopFindService(handler);
            },
            ara::com::InstanceIdentifier::MakeAny());
        uint32_t nLoopCount{0};
        auto proxy = std::make_shared<Proxy>(future.get());
        auto callbacks = std::make_shared<EventCallbacks>();
        // A weak reference avoids a cycle between the proxy and its receive handler.
        std::weak_ptr<Proxy> weakProxy = proxy;
        auto check = [&logger, &exitCode](const ara::core::Result<void>& result, const char* operation) {
            if (result) return true;
            logger.LogError() << "Client2 testEvent " << operation << " failed: " << result.Error().Message();
            exitCode = EXIT_FAILURE;
            return false;
        };
        bool ready = check(proxy->testEvent.SetSubscriptionStateChangeHandler(
            [callbacks, &logger](ara::com::SubscriptionState state) {
                std::lock_guard<std::mutex> lock(callbacks->mutex);
                if (callbacks->active)
                    logger.LogInfo() << "Client2 testEvent subscription: " << SubscriptionStateName(state);
            }), "SetSubscriptionStateChangeHandler");
        if (ready) {
            ready = check(proxy->testEvent.SetReceiveHandler([weakProxy, callbacks, &logger]() {
                std::lock_guard<std::mutex> lock(callbacks->mutex);
                if (!callbacks->active) return;
                auto activeProxy = weakProxy.lock();
                if (!activeProxy) return;
                // The notification contains no payload; GetNewSamples drains the cached samples.
                auto result = activeProxy->testEvent.GetNewSamples([&logger](auto sample) {
                    const std::string payload(sample->begin(), sample->end());
                    logger.LogInfo() << "Client2 testEvent received: " << payload.c_str()
                                     << " bytes: " << sample->size();
                });
                if (!result)
                    logger.LogError() << "Client2 testEvent GetNewSamples failed: " << result.Error().Message();
            }), "SetReceiveHandler");
        }
        if (ready) {
            // Reserve space for up to eight samples; subscription acknowledgement is asynchronous.
            ready = check(proxy->testEvent.Subscribe(8), "Subscribe");
        }
        while (ready && continueExecution) {
            std::string stMsg = "Com-Client2-Test[";
            stMsg += std::to_string(nLoopCount + 1);
            stMsg += "]";
            logger.LogInfo() << "iSOFT for CAPI: Helloworld-cm-Client2 [" << nLoopCount << "] call EchoMethod recv echo:"
                             << proxy->EchoMethod(stMsg.c_str()).GetResult().Value().echo;
            nLoopCount += 1;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        // Wait for any active callback, then prevent queued callbacks from touching the proxy/logger.
        {
            std::lock_guard<std::mutex> lock(callbacks->mutex);
            callbacks->active = false;
        }
        proxy->testEvent.Unsubscribe();
        check(proxy->testEvent.UnsetReceiveHandler(), "UnsetReceiveHandler");
        proxy->testEvent.UnsetSubscriptionStateChangeHandler();
        logger.LogInfo() << "Client2 testEvent subscription stopped";
    }

    if (!ara::core::Deinitialize()) {
        return EXIT_FAILURE;
    }

    return exitCode;
}
