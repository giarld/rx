//
// Created by Gxin on 2026/2/4.
//

#ifndef RX_OBSERVABLE_SAMPLE_H
#define RX_OBSERVABLE_SAMPLE_H

#include "../observable.h"
#include "../scheduler.h"
#include "../disposables/sequential_disposable.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"


namespace rx
{
class SampleObserver : public Observer, public Disposable, public std::enable_shared_from_this<SampleObserver>
{
public:
    SampleObserver(const ObserverPtr &downstream, uint64_t period, const WorkerPtr &worker)
        : mDownstream(downstream),
          mPeriod(period),
          mWorker(worker),
          mTimerDisposable(std::make_shared<SequentialDisposable>())
    {
        LeakObserver::make<SampleObserver>();
    }

    ~SampleObserver() override
    {
        LeakObserver::release<SampleObserver>();
    }

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        if (DisposableHelper::validate(mUpstream, d)) {
            if (const auto ds = mDownstream) {
                mUpstream = d;
                ds->onSubscribe(shared_from_this());
                scheduleNext();
            }
        }
    }

    void onNext(const GAny &value) override
    {
        if (mDone.load(std::memory_order_acquire)) {
            return;
        }

        GLockerGuard lock(mLock);
        mLatest = value;
        mHasValue = true;
    }

    void onError(const GAnyException &e) override
    {
        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }

        mTimerDisposable->dispose();
        if (const auto ds = mDownstream) {
            ds->onError(e);
        }
        mWorker->dispose();
        mDownstream = nullptr;
    }

    void onComplete() override
    {
        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }

        mTimerDisposable->dispose();

        GAny valueToEmit;
        bool shouldEmit = false;
        //
        {
            GLockerGuard lock(mLock);
            if (mHasValue) {
                valueToEmit = mLatest;
                mHasValue = false;
                shouldEmit = true;
            }
        }

        if (const auto ds = mDownstream) {
            if (shouldEmit) {
                ds->onNext(valueToEmit);
            }
            ds->onComplete();
        }
        mWorker->dispose();
        mDownstream = nullptr;
    }

    void dispose() override
    {
        mDone.store(true, std::memory_order_release);
        if (const auto d = mUpstream) {
            d->dispose();
            mUpstream = nullptr;
        }
        mTimerDisposable->dispose();
        mWorker->dispose();
        mDownstream = nullptr;
    }

    bool isDisposed() const override
    {
        return mWorker->isDisposed();
    }

private:
    void scheduleNext()
    {
        std::weak_ptr<SampleObserver> weakSelf = shared_from_this();
        const DisposablePtr d = mWorker->schedule([weakSelf] {
            if (const auto strong = weakSelf.lock()) {
                strong->tick();
            }
        }, mPeriod);
        mTimerDisposable->update(d);
    }

    void tick()
    {
        if (mDone.load(std::memory_order_acquire)) {
            return;
        }

        GAny valueToEmit;
        bool shouldEmit = false;
        //
        {
            GLockerGuard lock(mLock);
            if (mHasValue) {
                valueToEmit = mLatest;
                mHasValue = false;
                shouldEmit = true;
            }
        }

        if (shouldEmit) {
            if (const auto ds = mDownstream) {
                ds->onNext(valueToEmit);
            }
        }

        if (!mDone.load(std::memory_order_acquire)) {
            scheduleNext();
        }
    }

private:
    ObserverPtr mDownstream;
    uint64_t mPeriod;
    WorkerPtr mWorker;
    std::shared_ptr<SequentialDisposable> mTimerDisposable;
    DisposablePtr mUpstream;

    GSpinLock mLock;
    GAny mLatest;
    bool mHasValue = false;
    std::atomic<bool> mDone = false;
};

class ObservableSample : public Observable
{
public:
    ObservableSample(ObservableSourcePtr source, uint64_t period, SchedulerPtr scheduler)
        : mSource(std::move(source)), mPeriod(period), mScheduler(std::move(scheduler))
    {
        LeakObserver::make<ObservableSample>();
    }

    ~ObservableSample() override
    {
        LeakObserver::release<ObservableSample>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        WorkerPtr w = mScheduler->createWorker();
        mSource->subscribe(std::make_shared<SampleObserver>(observer, mPeriod, w));
    }

private:
    ObservableSourcePtr mSource;
    uint64_t mPeriod;
    SchedulerPtr mScheduler;
};
} // rx

#endif // RX_OBSERVABLE_SAMPLE_H
