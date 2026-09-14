#include "counter_service.h"
#include <ara/core/promise.h>
#include <ara/log/logger.h>

ara::core::Future<demo::counter::CounterService::GetCounterOutput> CounterServiceImpl::GetCounter()
{
    demo::counter::CounterService::GetCounterOutput output;
    output.value = counter_->load();
    ara::log::CreateLogger("#COM", "com-serverd", ara::log::LogLevel::kDebug).LogInfo()
        << "CounterServer GetCounter value:" << output.value;
    ara::core::Promise<demo::counter::CounterService::GetCounterOutput> promise;
    promise.set_value(output);
    return promise.get_future();
}

ara::core::Result<void> CounterServiceImpl::PublishNext()
{
    const std::uint32_t value = counter_->fetch_add(1) + 1;
    auto result = CounterChanged.Send(value);
    auto& logger = ara::log::CreateLogger("#COM", "com-serverd", ara::log::LogLevel::kDebug);
    if (result)
        logger.LogInfo() << "CounterServer CounterChanged value:" << value;
    else
        logger.LogError() << "CounterServer Send failed:" << result.Error().Message();
    return result;
}
