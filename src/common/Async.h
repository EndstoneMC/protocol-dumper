#pragma once

#include <functional>
#include <memory>

namespace Bedrock::Threading {
class AsyncBase : public std::enable_shared_from_this<AsyncBase> {};
class IAsyncInfo {
public:
    virtual ~IAsyncInfo() = default;
};
template <typename TResult>
class IAsyncGetResult : public IAsyncInfo {
public:
    virtual TResult getResult() const = 0;
};
template <typename TResult>
class IAsyncResult : public AsyncBase, public IAsyncGetResult<TResult> {
public:
    using CompletionHandler = std::function<void(const IAsyncResult &)>;
    virtual void addOnComplete(CompletionHandler) = 0;
};

template <typename TResult>
class AsyncImplBase : public std::shared_ptr<IAsyncResult<TResult>> {
public:
};

template <typename TResult>
class Async : public AsyncImplBase<TResult> {};
}  // namespace Bedrock::Threading
