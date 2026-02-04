//
// Created by Gxin on 2026/1/29.
//

#ifndef RX_OBSERVABLE_TIMEOUT_H
#define RX_OBSERVABLE_TIMEOUT_H

#include "../observable.h"
#include "../scheduler.h"
#include "../disposables/sequential_disposable.h"
#include "../leak_observer.h"
#include <atomic>


namespace rx
{
class TimeoutObserver : public Observer, public Disposable, public std::enable_shared_from_this<TimeoutObserver>
{
public:
    TimeoutObserver(const ObserverPtr &downstream, uint64_t timeout, const WorkerPtr &worker, ObservableSourcePtr fallback)
        : mDownstream(downstream),
          mTimeout(timeout),
          mWorker(worker),
          mFallback(std::move(fallback)),
          mUpstream(std::make_shared<SequentialDisposable>()),
          mTimeoutDisposable(std::make_shared<SequentialDisposable>())
    {
        LeakObserver::make<TimeoutObserver>();
    }

    ~TimeoutObserver() override
    {
        LeakObserver::release<TimeoutObserver>();
    }

    void onSubscribe(const DisposablePtr &d) override
    {
        if (mInFallback.load(std::memory_order_acquire)) {
            mUpstream->update(d);
            return;
        }
        mUpstream->update(d);
        if (const auto ds = mDownstream) {
            ds->onSubscribe(shared_from_this());
        }
        scheduleTimeout(0);
    }

    void onNext(const GAny &value) override
    {
        if (mInFallback.load(std::memory_order_acquire)) {
            if (const auto ds = mDownstream) {
                ds->onNext(value);
            }
            return;
        }

        if (mDone.load(std::memory_order_acquire)) {
            return;
        }

        const uint64_t idx = ++mIndex;
        if (const auto ds = mDownstream) {
            ds->onNext(value);
        }
        scheduleTimeout(idx);
    }

    void onError(const GAnyException &e) override
    {
        if (mInFallback.load(std::memory_order_acquire)) {
            if (const auto ds = mDownstream) {
                ds->onError(e);
            }
            mDownstream = nullptr;
            mWorker->dispose();
            return;
        }

        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }

        mTimeoutDisposable->dispose();
        if (const auto ds = mDownstream) {
            ds->onError(e);
        }
        mWorker->dispose();
        mDownstream = nullptr;
    }

    void onComplete() override
    {
        if (mInFallback.load(std::memory_order_acquire)) {
            if (const auto ds = mDownstream) {
                ds->onComplete();
            }
            mDownstream = nullptr;
            mWorker->dispose();
            return;
        }

        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }

        mTimeoutDisposable->dispose();
        if (const auto ds = mDownstream) {
            ds->onComplete();
        }
        mWorker->dispose();
        mDownstream = nullptr;
    }

    void dispose() override
    {
        mUpstream->dispose();
        mTimeoutDisposable->dispose();
        mWorker->dispose();
        mDownstream = nullptr;
    }

    bool isDisposed() const override
    {
        return mWorker->isDisposed();
    }

private:
    void scheduleTimeout(uint64_t idx)
    {
        std::weak_ptr<TimeoutObserver> weakSelf = shared_from_this();
        const DisposablePtr d = mWorker->schedule([weakSelf, idx] {
            if (const auto strong = weakSelf.lock()) {
                strong->onTimeout(idx);
            }
        }, mTimeout);
        mTimeoutDisposable->update(d);
    }

    void onTimeout(uint64_t idx)
    {
        if (mIndex.load(std::memory_order_acquire) == idx) {
            if (mDone.exchange(true, std::memory_order_acq_rel)) {
                return;
            }

            if (mFallback) {
                mInFallback.store(true, std::memory_order_release);
                mUpstream->update(nullptr);
                mTimeoutDisposable->dispose();
                mFallback->subscribe(shared_from_this());
            } else {
                mUpstream->dispose();
                mTimeoutDisposable->dispose();

                if (const auto ds = mDownstream) {
                    ds->onError(GAnyException("Timeout"));
                }
                mWorker->dispose();
                mDownstream = nullptr;
            }
        }
    }

private:
    ObserverPtr mDownstream;
    uint64_t mTimeout;
    WorkerPtr mWorker;
    ObservableSourcePtr mFallback;
    std::shared_ptr<SequentialDisposable> mUpstream;
    std::shared_ptr<SequentialDisposable> mTimeoutDisposable;
    std::atomic<uint64_t> mIndex{0};
    std::atomic<bool> mDone{false};
    std::atomic<bool> mInFallback{false};
};

class ObservableTimeout : public Observable
{
public:
    ObservableTimeout(ObservableSourcePtr source, uint64_t timeout, SchedulerPtr scheduler, ObservableSourcePtr fallback)
        : mSource(std::move(source)), mTimeout(timeout), mScheduler(std::move(scheduler)), mFallback(std::move(fallback))
    {
        LeakObserver::make<ObservableTimeout>();
    }

    ~ObservableTimeout() override
    {
        LeakObserver::release<ObservableTimeout>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        WorkerPtr w = mScheduler->createWorker();
        mSource->subscribe(std::make_shared<TimeoutObserver>(observer, mTimeout, w, mFallback));
    }

private:
    ObservableSourcePtr mSource;
    uint64_t mTimeout;
    SchedulerPtr mScheduler;
    ObservableSourcePtr mFallback;
};
} // rx

#endif //RX_OBSERVABLE_TIMEOUT_H
