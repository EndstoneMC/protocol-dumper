#pragma once

#include <memory>

namespace Bedrock::PubSub {
class SubscriptionBase {
    std::weak_ptr<void /*Bedrock::PubSub::Detail::SubscriptionBodyBase*/> mBody;
};

class Subscription : public SubscriptionBase {};
}  // namespace Bedrock::PubSub
