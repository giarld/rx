//
// Created by Gxin on 2026/1/13.
//

#ifndef RX_OBSERVABLE_LAST_H
#define RX_OBSERVABLE_LAST_H

#include "../observable.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"


namespace rx
{
class LastObserver : public Observer, public Disposable, public std::enable_shared_from_this<LastObserver>
{
public:
    explicit LastObserver(const ObserverPtr &observer, const GAny &defaultValue, bool hasDefault)
        : mDownstream(observer), mDefaultValue(defaultValue), mHasDefault(hasDefault), mHasValue(false)
    {
        LeakObserver::make<LastObserver>();
    }

    ~LastObserver() override
    {
        LeakObserver::release<LastObserver>();
    }

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        if (DisposableHelper::validate(mUpstream, d)) {
            mUpstream = d;
            mDownstream->onSubscribe(this->shared_from_this());
        }
    }

    void onNext(const GAny &value) override
    {
        if (mDone.load(std::memory_order_acquire)) {
            return;
        }

        // 保存最后一个值
        mLastValue = value;
        mHasValue = true;
    }

    void onError(const GAnyException &e) override
    {
        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        mDownstream->onError(e);

        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    void onComplete() override
    {
        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }

        if (mHasValue) {
            // 发出最后一个值
            mDownstream->onNext(mLastValue);
            mDownstream->onComplete();
        } else if (mHasDefault) {
            // 没有值但有默认值
            mDownstream->onNext(mDefaultValue);
            mDownstream->onComplete();
        } else {
            // 没有值也没有默认值，发出错误
            mDownstream->onError(GAnyException("No elements in sequence"));
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
    DisposablePtr mUpstream;
    GAny mLastValue;
    GAny mDefaultValue;
    bool mHasDefault;
    bool mHasValue;
    std::atomic<bool> mDone = false;
};

class ObservableLast : public Observable
{
public:
    explicit ObservableLast(ObservableSourcePtr source, const GAny &defaultValue, bool hasDefault)
        : mSource(std::move(source)), mDefaultValue(defaultValue), mHasDefault(hasDefault)
    {
        LeakObserver::make<ObservableLast>();
    }

    ~ObservableLast() override
    {
        LeakObserver::release<ObservableLast>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<LastObserver>(observer, mDefaultValue, mHasDefault));
    }

private:
    ObservableSourcePtr mSource;
    GAny mDefaultValue;
    bool mHasDefault;
};
} // rx

#endif //RX_OBSERVABLE_LAST_H
