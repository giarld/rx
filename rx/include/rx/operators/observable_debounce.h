//
// Created by Gxin on 2026/1/21.
//

#ifndef RX_OBSERVABLE_DEBOUNCE_H
#define RX_OBSERVABLE_DEBOUNCE_H

#include "../observable.h"
#include "../scheduler.h"
#include "../disposables/sequential_disposable.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"


namespace rx
{
class DebounceObserver : public Observer, public Disposable, public std::enable_shared_from_this<DebounceObserver>
{
public:
    DebounceObserver(const ObserverPtr &downstream, uint64_t delay, const WorkerPtr &worker)
        : mDownstream(downstream),
          mDelay(delay),
          mWorker(worker),
          mDebounceDisposable(std::make_shared<SequentialDisposable>())
    {
        LeakObserver::make<DebounceObserver>();
    }

    ~DebounceObserver() override
    {
        LeakObserver::release<DebounceObserver>();
    }

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        if (DisposableHelper::validate(mUpstream, d)) {
            if (const auto ds = mDownstream) {
                mUpstream = d;
                ds->onSubscribe(shared_from_this());
            }
        }
    }

    void onNext(const GAny &value) override
    {
        if (mDone.load(std::memory_order_acquire))
            return;

        const uint64_t idx = [&] {
            GLockerGuard lock(mLock);
            mValue = value;
            mHasValue = true;
            return ++mIndex;
        }();

        std::weak_ptr<DebounceObserver> weakSelf = shared_from_this();
        const DisposablePtr d = mWorker->schedule([weakSelf, idx] {
            if (const auto strong = weakSelf.lock()) {
                strong->emit(idx);
            }
        }, mDelay);

        mDebounceDisposable->update(d);
    }

    void onError(const GAnyException &e) override
    {
        if (mDone.load(std::memory_order_acquire)) {
            return;
        }
        mDone.store(true, std::memory_order_release);

        mDebounceDisposable->dispose();
        if (const auto d = mDownstream) {
            d->onError(e);
        }
        mWorker->dispose();

        mDownstream = nullptr;
    }

    void onComplete() override
    {
        if (mDone.load(std::memory_order_acquire)) {
            return;
        }
        mDone.store(true, std::memory_order_release);

        bool shouldEmit = false;
        GAny valueToEmit;

        //
        {
            GLockerGuard lock(mLock);

            if (mHasValue) {
                shouldEmit = true;
                valueToEmit = mValue;
                mHasValue = false;
            }
        }

        mDebounceDisposable->dispose();

        if (const auto d = mDownstream) {
            if (shouldEmit) {
                d->onNext(valueToEmit);
            }
            d->onComplete();
            mWorker->dispose();
        }
        mDownstream = nullptr;
    }

    void dispose() override
    {
        if (const auto d = mUpstream) {
            d->dispose();
            mUpstream = nullptr;
        }
        mWorker->dispose();
        mDebounceDisposable->dispose();

        mDownstream = nullptr;
    }

    bool isDisposed() const override
    {
        return mWorker->isDisposed();
    }

    void emit(uint64_t idx)
    {
        GAny valueToEmit;
        bool shouldEmit = false;

        //
        {
            GLockerGuard lock(mLock);
            if (mIndex == idx && mHasValue && !mDone.load(std::memory_order_acquire)) {
                valueToEmit = mValue;
                mHasValue = false;
                shouldEmit = true;
            }
        }

        if (shouldEmit) {
            if (const auto d = mDownstream) {
                d->onNext(valueToEmit);
            }
        }
    }

private:
    ObserverPtr mDownstream;
    uint64_t mDelay;
    WorkerPtr mWorker;
    std::shared_ptr<SequentialDisposable> mDebounceDisposable;
    DisposablePtr mUpstream;

    GSpinLock mLock;
    GAny mValue;
    bool mHasValue = false;
    uint64_t mIndex = 0;
    std::atomic<bool> mDone = false;
};

class ObservableDebounce : public Observable
{
public:
    ObservableDebounce(ObservableSourcePtr source, uint64_t delay, SchedulerPtr scheduler)
        : mSource(std::move(source)), mDelay(delay), mScheduler(std::move(scheduler))
    {
        LeakObserver::make<ObservableDebounce>();
    }

    ~ObservableDebounce() override
    {
        LeakObserver::release<ObservableDebounce>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        WorkerPtr w = mScheduler->createWorker();
        mSource->subscribe(std::make_shared<DebounceObserver>(observer, mDelay, w));
    }

private:
    ObservableSourcePtr mSource;
    uint64_t mDelay;
    SchedulerPtr mScheduler;
};
} // rx

#endif // RX_OBSERVABLE_DEBOUNCE_H
