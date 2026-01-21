//
// Created by Gxin on 2026/1/15.
//

#ifndef RX_OBSERVABLE_TAKE_LAST_H
#define RX_OBSERVABLE_TAKE_LAST_H

#include "../observable.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"

#include <deque>


namespace rx
{
class TakeLastObserver : public Observer, public Disposable, public std::enable_shared_from_this<TakeLastObserver>
{
public:
    explicit TakeLastObserver(const ObserverPtr &observer, uint64_t count)
        : mDownstream(observer), mCount(count)
    {
        LeakObserver::make<TakeLastObserver>();
    }

    ~TakeLastObserver() override
    {
        LeakObserver::release<TakeLastObserver>();
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
        if (mCount == 0) {
            return;
        }

        if (mCount == mBuffer.size()) {
            mBuffer.pop_front();
        }
        mBuffer.push_back(value);
    }

    void onError(const GAnyException &e) override
    {
        mBuffer.clear();
        mDownstream->onError(e);

        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    void onComplete() override
    {
        for (const auto &item : mBuffer) {
            if (mCancelled.load(std::memory_order_acquire)) {
                break;
            }
            mDownstream->onNext(item);
        }
        mBuffer.clear();

        if (!mCancelled.load(std::memory_order_acquire)) {
            mDownstream->onComplete();
        }

        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    void dispose() override
    {
        if (!mCancelled.exchange(true, std::memory_order_acq_rel)) {
            if (const auto d = mUpstream) {
                d->dispose();
                mUpstream = nullptr;
            }
            mDownstream = nullptr;
        }
    }

    bool isDisposed() const override
    {
        return mCancelled.load(std::memory_order_acquire);
    }

private:
    ObserverPtr mDownstream;
    DisposablePtr mUpstream;
    uint64_t mCount;
    std::deque<GAny> mBuffer;
    std::atomic<bool> mCancelled = false;
};

class ObservableTakeLast : public Observable
{
public:
    explicit ObservableTakeLast(ObservableSourcePtr source, uint64_t count)
        : mSource(std::move(source)), mCount(count)
    {
        LeakObserver::make<ObservableTakeLast>();
    }

    ~ObservableTakeLast() override
    {
        LeakObserver::release<ObservableTakeLast>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<TakeLastObserver>(observer, mCount));
    }

private:
    ObservableSourcePtr mSource;
    uint64_t mCount;
};
} // rx

#endif //RX_OBSERVABLE_TAKE_LAST_H
