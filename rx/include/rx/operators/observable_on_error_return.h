//
// Created by Gxin on 2026/2/6.
//

#ifndef RX_OBSERVABLE_ON_ERROR_RETURN_H
#define RX_OBSERVABLE_ON_ERROR_RETURN_H

#include "../observable.h"
#include "../observer.h"
#include "../leak_observer.h"


namespace rx
{
class OnErrorReturnObserver : public Observer, public Disposable, public std::enable_shared_from_this<OnErrorReturnObserver>
{
public:
    OnErrorReturnObserver(const ObserverPtr &downstream, const GAny &defaultValue)
        : mDownstream(downstream), mDefaultValue(defaultValue)
    {
        LeakObserver::make<OnErrorReturnObserver>();
    }

    ~OnErrorReturnObserver() override
    {
        LeakObserver::release<OnErrorReturnObserver>();
    }

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        if (DisposableHelper::validate(mUpstream, d)) {
            if (const auto ds = mDownstream) {
                mUpstream = d;
                ds->onSubscribe(this->shared_from_this());
            }
        }
    }

    void onNext(const GAny &value) override
    {
        if (mDone.load(std::memory_order_acquire)) {
            return;
        }
        if (const auto d = mDownstream) {
            d->onNext(value);
        }
    }

    void onError(const GAnyException &e) override
    {
        // 捕获错误，发射默认值，然后正常完成
        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        if (const auto d = mDownstream) {
            d->onNext(mDefaultValue);
            d->onComplete();
        }
        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    void onComplete() override
    {
        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        if (const auto d = mDownstream) {
            d->onComplete();
        }
        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    void dispose() override
    {
        if (const auto d = mUpstream) {
            d->dispose();
            mUpstream = nullptr;
        }
        mDownstream = nullptr;
    }

    bool isDisposed() const override
    {
        if (const auto d = mUpstream) {
            return d->isDisposed();
        }
        return true;
    }

private:
    ObserverPtr mDownstream;
    GAny mDefaultValue;
    DisposablePtr mUpstream;
    std::atomic<bool> mDone = false;
};

class ObservableOnErrorReturn : public Observable
{
public:
    ObservableOnErrorReturn(std::shared_ptr<Observable> source, GAny defaultValue)
        : mSource(std::move(source)), mDefaultValue(std::move(defaultValue))
    {
        LeakObserver::make<ObservableOnErrorReturn>();
    }

    ~ObservableOnErrorReturn() override
    {
        LeakObserver::release<ObservableOnErrorReturn>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<OnErrorReturnObserver>(observer, mDefaultValue));
    }

private:
    std::shared_ptr<Observable> mSource;
    GAny mDefaultValue;
};
} // rx

#endif //RX_OBSERVABLE_ON_ERROR_RETURN_H
