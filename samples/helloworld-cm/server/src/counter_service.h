#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include "demo/counter/counterservice_skeleton.h"

class CounterServiceImpl : public demo::counter::skeleton::CounterServiceSkeleton {
public:
    using Skeleton = demo::counter::skeleton::CounterServiceSkeleton;
    CounterServiceImpl(ara::core::InstanceSpecifier instance, ara::com::MethodCallProcessingMode mode)
        : Skeleton(std::move(instance), mode) {}

    ara::core::Future<demo::counter::CounterService::GetCounterOutput> GetCounter() override;
    ara::core::Result<void> PublishNext();

private:
    // Methods may run on a COM thread. Shared storage also keeps this factory-created skeleton movable.
    std::shared_ptr<std::atomic<std::uint32_t>> counter_{std::make_shared<std::atomic<std::uint32_t>>(0)};
};
