//
// Created by Gxin on 2026/2/4.
//

#ifndef RX_OBSERVABLE_RETRY_H
#define RX_OBSERVABLE_RETRY_H

#include "../observable.h"
#include "../disposables/sequential_disposable.h"
#include "../leak_observer.h"


namespace rx
{
class RetryObserver : public Observer, public std::enable_shared_from_this<RetryObserver>
{
public:
    explicit RetryObserver(const ObserverPtr &observer, int64_t count, const SequentialDisposablePtr &sd, const ObservableSourcePtr &source)
        : mDownstream(observer), mSd(sd), mSource(source), mRemaining(count)
    {
        LeakObserver::make<RetryObserver>();
    }

    ~RetryObserver() override
    {
        LeakObserver::release<RetryObserver>();
    }

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        if (const auto sd = mSd.lock()) {
            sd->replace(d);
        } else {
            d->dispose();
        }
    }

    void onNext(const GAny &value) override
    {
        if (const auto d = mDownstream) {
            d->onNext(value);
        }
    }

    void onError(const GAnyException &e) override
    {
        const int64_t r = mRemaining;
        if (r != std::numeric_limits<int64_t>::max()) {
            mRemaining = r - 1;
        }

        if (r != 0) {
            subscribeNext();
        } else {
            if (const auto d = mDownstream) {
                d->onError(e);
            }
            cleanup();
        }
    }

    void onComplete() override
    {
        if (const auto d = mDownstream) {
            d->onComplete();
        }
        cleanup();
    }

    void subscribeNext()
    {
        if (mWip.fetch_add(1, std::memory_order_acq_rel) == 0) {
            int32_t missed = 1;
            while (true) {
                const auto sd = mSd.lock();
                if (!sd || sd->isDisposed()) {
                    return;
                }

                mSource->subscribe(this->shared_from_this());

                missed = mWip.fetch_add(-missed, std::memory_order_acq_rel) + (-missed);
                if (missed == 0) {
                    break;
                }
            }
        }
    }

private:
    void cleanup()
    {
        mDownstream = nullptr;
        mSd.reset();
        mSource = nullptr;
    }

private:
    ObserverPtr mDownstream;
    std::weak_ptr<SequentialDisposable> mSd;
    ObservableSourcePtr mSource;
    int64_t mRemaining;
    std::atomic<int32_t> mWip{0};
};

class ObservableRetry : public Observable
{
public:
    explicit ObservableRetry(ObservableSourcePtr source, uint64_t times)
        : mSource(std::move(source)), mTimes(times)
    {
        LeakObserver::make<ObservableRetry>();
    }

    ~ObservableRetry() override
    {
        LeakObserver::release<ObservableRetry>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        const auto sd = std::make_shared<SequentialDisposable>();
        observer->onSubscribe(sd);

        const auto ro = std::make_shared<RetryObserver>(
            observer,
            mTimes != std::numeric_limits<uint64_t>::max() ? mTimes : std::numeric_limits<int64_t>::max(),
            sd, mSource);
        ro->subscribeNext();
    }

private:
    ObservableSourcePtr mSource;
    uint64_t mTimes;
};
} // rx

#endif //RX_OBSERVABLE_RETRY_H
