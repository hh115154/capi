#pragma once

#include <chrono>
#include <memory>
#include <ara/log/logger.h>
#include "demo/counter/counterservice_proxy.h"

// Called once per main-loop iteration; event samples and method results are polled without blocking.
class CounterClient {
public:
    explicit CounterClient(ara::log::Logger& logger) : logger_(logger) {}
    ~CounterClient();
    bool Tick();
    CounterClient(const CounterClient&) = delete;
    CounterClient& operator=(const CounterClient&) = delete;

private:
    using Proxy = demo::counter::proxy::CounterServiceProxy;
    using Reply = ara::core::Future<demo::counter::CounterService::GetCounterOutput>;
    ara::log::Logger& logger_;
    std::unique_ptr<Proxy> proxy_;
    std::unique_ptr<Reply> reply_;
    ara::com::SubscriptionState state_{ara::com::SubscriptionState::kNotSubscribed};
    std::chrono::steady_clock::time_point requestTime_;
    bool requested_{false};
};
