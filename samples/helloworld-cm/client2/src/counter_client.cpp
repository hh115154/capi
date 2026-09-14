#include "counter_client.h"

CounterClient::~CounterClient()
{
    if (proxy_) {
        proxy_->CounterChanged.Unsubscribe();
        logger_.LogInfo() << "CounterClient stopped";
    }
}

bool CounterClient::Tick()
{
    if (!proxy_) {
        // Resolve the Required port from the deployment, rather than accepting an arbitrary instance.
        auto handles = Proxy::FindService(ara::core::InstanceSpecifier{"client2d/client2/counter_RPort"});
        if (!handles) {
            logger_.LogError() << "CounterClient FindService failed:" << handles.Error().Message();
            return false;
        }
        if (handles.Value().empty()) return true;  // Service discovery has not completed yet.
        auto created = Proxy::Create(handles.Value().front());
        if (!created) {
            logger_.LogError() << "CounterClient Create failed:" << created.Error().Message();
            return false;
        }
        proxy_ = std::make_unique<Proxy>(std::move(created).Value());
        auto subscribed = proxy_->CounterChanged.Subscribe(8);
        if (!subscribed) {
            logger_.LogError() << "CounterClient Subscribe failed:" << subscribed.Error().Message();
            return false;
        }
    }
    const auto state = proxy_->CounterChanged.GetSubscriptionState();
    if (state != state_) {
        state_ = state;
        const char* name = state == ara::com::SubscriptionState::kSubscribed ? "Subscribed"
            : state == ara::com::SubscriptionState::kSubscriptionPending ? "SubscriptionPending" : "NotSubscribed";
        logger_.LogInfo() << "CounterClient subscription:" << name;
    }
    if (state == ara::com::SubscriptionState::kSubscribed) {
        // Polling is the alternative to SetReceiveHandler: drain samples without retaining SamplePtr objects.
        auto samples = proxy_->CounterChanged.GetNewSamples([this](auto sample) {
            logger_.LogInfo() << "CounterClient CounterChanged value:" << *sample;
        });
        if (!samples) {
            logger_.LogError() << "CounterClient GetNewSamples failed:" << samples.Error().Message();
            return false;
        }
        if (!requested_) {
            requested_ = true;
            requestTime_ = std::chrono::steady_clock::now();
            reply_ = std::make_unique<Reply>(proxy_->GetCounter());
        }
    }
    if (reply_) {
        if (reply_->wait_for(std::chrono::milliseconds(0)) == ara::core::future_status::ready) {
            auto result = reply_->GetResult();
            reply_.reset();
            if (!result) {
                logger_.LogError() << "CounterClient GetCounter failed:" << result.Error().Message();
                return false;
            }
            logger_.LogInfo() << "CounterClient GetCounter value:" << result.Value().value;
        } else if (std::chrono::steady_clock::now() - requestTime_ > std::chrono::seconds(3)) {
            logger_.LogError() << "CounterClient GetCounter failed: timeout";
            return false;
        }
    }
    return true;
}
