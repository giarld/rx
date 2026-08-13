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
#include <mutex>


namespace rx
{
class TimeoutObserver;

class TimeoutFallbackObserver : public Observer
{
public:
    explicit TimeoutFallbackObserver(const std::shared_ptr<TimeoutObserver> &parent)
        : mParent(parent)
    {
    }

public:
    void onSubscribe(const DisposablePtr &d) override;
    void onNext(const GAny &value) override;
    void onError(const GAnyException &e) override;
    void onComplete() override;

private:
    std::weak_ptr<TimeoutObserver> mParent;
};

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

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        bool disposeNow = false;
        ObserverPtr downstream;
        {
            std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
            disposeNow = mDisposed;
            downstream = mDownstream;
        }
        if (disposeNow) {
            d->dispose();
            return;
        }
        mUpstream->update(d);
        if (downstream) {
            downstream->onSubscribe(shared_from_this());
        }
        scheduleTimeout(0);
    }

    void onNext(const GAny &value) override
    {
        uint64_t idx;
        {
            std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
            if (mDisposed || mDone) {
                return;
            }
            idx = ++mIndex;
            if (const auto downstream = mDownstream) {
                downstream->onNext(value);
            }
        }
        scheduleTimeout(idx);
    }

    void onError(const GAnyException &e) override
    {
        ObserverPtr downstream;
        {
            std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
            if (mDisposed || mDone.exchange(true, std::memory_order_acq_rel)) {
                return;
            }
            mDisposed = true;
            downstream = std::move(mDownstream);
        }
        mTimeoutDisposable->dispose();
        mWorker->dispose();
        if (downstream) {
            downstream->onError(e);
        }
    }

    void onComplete() override
    {
        ObserverPtr downstream;
        {
            std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
            if (mDisposed || mDone.exchange(true, std::memory_order_acq_rel)) {
                return;
            }
            mDisposed = true;
            downstream = std::move(mDownstream);
        }
        mTimeoutDisposable->dispose();
        mWorker->dispose();
        if (downstream) {
            downstream->onComplete();
        }
    }

    void dispose() override
    {
        {
            std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
            if (mDisposed.exchange(true)) {
                return;
            }
            mDone = true;
            mDownstream = nullptr;
        }
        mUpstream->dispose();
        mTimeoutDisposable->dispose();
        mWorker->dispose();
    }

    bool isDisposed() const override
    {
        return mDisposed.load(std::memory_order_acquire);
    }

    void fallbackSubscribe(const DisposablePtr &d)
    {
        bool disposeNow;
        {
            std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
            disposeNow = mDisposed || !mInFallback;
        }
        if (disposeNow) {
            d->dispose();
            return;
        }
        mUpstream->update(d);
    }

    void fallbackNext(const GAny &value)
    {
        std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
        if (!mDisposed && mInFallback && mDownstream) {
            mDownstream->onNext(value);
        }
    }

    void fallbackError(const GAnyException &e)
    {
        ObserverPtr downstream;
        {
            std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
            if (mDisposed || !mInFallback) {
                return;
            }
            mDisposed = true;
            downstream = std::move(mDownstream);
        }
        mWorker->dispose();
        if (downstream) {
            downstream->onError(e);
        }
    }

    void fallbackComplete()
    {
        ObserverPtr downstream;
        {
            std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
            if (mDisposed || !mInFallback) {
                return;
            }
            mDisposed = true;
            downstream = std::move(mDownstream);
        }
        mWorker->dispose();
        if (downstream) {
            downstream->onComplete();
        }
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
        ObservableSourcePtr fallback;
        ObserverPtr downstream;
        bool timeoutError = false;
        {
            std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
            if (mDisposed || mIndex.load(std::memory_order_acquire) != idx
                || mDone.exchange(true, std::memory_order_acq_rel)) {
                return;
            }
            if (mFallback) {
                mInFallback.store(true, std::memory_order_release);
                fallback = mFallback;
            } else {
                mDisposed = true;
                timeoutError = true;
                downstream = std::move(mDownstream);
            }
        }

        mUpstream->update(nullptr);
        mTimeoutDisposable->dispose();
        if (fallback) {
            fallback->subscribe(std::make_shared<TimeoutFallbackObserver>(shared_from_this()));
        } else if (timeoutError) {
            mWorker->dispose();
            if (downstream) {
                downstream->onError(GAnyException("Timeout"));
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
    std::atomic<bool> mDisposed{false};
    std::recursive_mutex mSignalLock;
};

inline void TimeoutFallbackObserver::onSubscribe(const DisposablePtr &d)
{
    if (const auto parent = mParent.lock()) {
        parent->fallbackSubscribe(d);
    } else {
        d->dispose();
    }
}

inline void TimeoutFallbackObserver::onNext(const GAny &value)
{
    if (const auto parent = mParent.lock()) {
        parent->fallbackNext(value);
    }
}

inline void TimeoutFallbackObserver::onError(const GAnyException &e)
{
    if (const auto parent = mParent.lock()) {
        parent->fallbackError(e);
    }
}

inline void TimeoutFallbackObserver::onComplete()
{
    if (const auto parent = mParent.lock()) {
        parent->fallbackComplete();
    }
}

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
