//
// Created by Gxin on 2026/2/4.
//

#ifndef RX_OBSERVABLE_REDUCE_H
#define RX_OBSERVABLE_REDUCE_H

#include "../observable.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"


namespace rx
{
class ReduceObserver : public Observer, public Disposable, public std::enable_shared_from_this<ReduceObserver>
{
public:
    explicit ReduceObserver(const ObserverPtr &observer, const BiFunction &accumulator)
        : mDownstream(observer), mAccumulator(accumulator)
    {
        LeakObserver::make<ReduceObserver>();
    }

    ~ReduceObserver() override
    {
        LeakObserver::release<ReduceObserver>();
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

    void onNext(const GAny &t) override
    {
        if (mDone.load(std::memory_order_acquire)) {
            return;
        }

        if (!mHasValue) {
            mValue = t;
            mHasValue = true;
            return;
        }

        GAny u;
        try {
            u = mAccumulator(mValue, t);
        } catch (const std::exception &e) {
            if (const auto up = mUpstream) {
                up->dispose();
            }
            onError(GAnyException(e.what()));
            return;
        }
        mValue = u;
    }

    void onError(const GAnyException &e) override
    {
        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        if (const auto d = mDownstream) {
            d->onError(e);
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
            if (mHasValue) {
                d->onNext(mValue);
                d->onComplete();
            } else {
                d->onError(GAnyException("No elements in sequence"));
            }
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
    BiFunction mAccumulator;
    DisposablePtr mUpstream;
    std::atomic<bool> mDone = false;
    GAny mValue;
    bool mHasValue = false;
};

class ObservableReduce : public Observable
{
public:
    explicit ObservableReduce(ObservableSourcePtr source, const BiFunction &accumulator)
        : mSource(std::move(source)), mAccumulator(accumulator)
    {
        LeakObserver::make<ObservableReduce>();
    }

    ~ObservableReduce() override
    {
        LeakObserver::release<ObservableReduce>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<ReduceObserver>(observer, mAccumulator));
    }

private:
    ObservableSourcePtr mSource;
    BiFunction mAccumulator;
};
} // rx

#endif //RX_OBSERVABLE_REDUCE_H
