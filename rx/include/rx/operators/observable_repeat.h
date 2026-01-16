//
// Created by Gxin on 2026/1/12.
//

#ifndef RX_OBSERVABLE_REPEAT_H
#define RX_OBSERVABLE_REPEAT_H

#include "../observable.h"
#include "../disposables/sequential_disposable.h"
#include "../leak_observer.h"


namespace rx
{
class RepeatObserver : public Observer, public std::enable_shared_from_this<RepeatObserver>
{
public:
    explicit RepeatObserver(const ObserverPtr &observer, int64_t count, const SequentialDisposablePtr &sd, const ObservableSourcePtr &source)
        : mDownstream(observer), mSd(sd), mSource(source), mRemaining(count)
    {
        LeakObserver::make<RepeatObserver>();
    }

    ~RepeatObserver() override
    {
        LeakObserver::release<RepeatObserver>();
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
        if (const auto d = mDownstream) {
            d->onError(e);
        }
        cleanup();
    }

    void onComplete() override
    {
        const int64_t r = mRemaining;
        if (r != std::numeric_limits<int64_t>::max()) {
            mRemaining = r - 1;
        }
        if (r != 0) {
            subscribeNext();
        } else {
            if (const auto d = mDownstream) {
                d->onComplete();
            }
            cleanup();
        }
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
    std::atomic<int32_t> mWip;
};

class ObservableRepeat : public Observable
{
public:
    explicit ObservableRepeat(ObservableSourcePtr source, uint64_t times)
        : mSource(std::move(source)), mTimes(times)
    {
        LeakObserver::make<ObservableRepeat>();
    }

    ~ObservableRepeat() override
    {
        LeakObserver::release<ObservableRepeat>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        const auto sd = std::make_shared<SequentialDisposable>();
        observer->onSubscribe(sd);

        const auto rs = std::make_shared<RepeatObserver>(
            observer,
            mTimes != std::numeric_limits<uint64_t>::max() ? mTimes - 1 : std::numeric_limits<int64_t>::max(),
            sd, mSource);
        rs->subscribeNext();
    }

private:
    ObservableSourcePtr mSource;
    uint64_t mTimes;
};
} // rx

#endif //RX_OBSERVABLE_REPEAT_H
