//
// Created by Gxin on 2026/1/17.
//

#ifndef RX_OBSERVABLE_COMBINE_LATEST_H
#define RX_OBSERVABLE_COMBINE_LATEST_H

#include "observable_empty.h"
#include "../observable.h"
#include "../exception_helper.h"
#include "../leak_observer.h"
#include <mutex>


namespace rx
{
class CombineLatestObserver;

class CombineLatestInnerObserver : public Observer
{
public:
    CombineLatestInnerObserver(const std::shared_ptr<CombineLatestObserver> &parent, size_t index)
        : mParent(parent), mIndex(index)
    {
    }

public:
    void onSubscribe(const DisposablePtr &d) override;

    void onNext(const GAny &value) override;

    void onError(const GAnyException &e) override;

    void onComplete() override;

private:
    std::weak_ptr<CombineLatestObserver> mParent;
    size_t mIndex;
    DisposablePtr mUpstream;
};

class CombineLatestObserver : public Disposable, public std::enable_shared_from_this<CombineLatestObserver>
{
public:
    CombineLatestObserver(const ObserverPtr &downstream,
                          CombineLatestFunction combiner,
                          size_t count)
        : mDownstream(downstream), mCombiner(std::move(combiner)), mValues(count), mHasValue(count, false),
          mActiveCount(count), mEmittedCount(0), mDisposables(count)
    {
        LeakObserver::make<CombineLatestObserver>();
    }

    ~CombineLatestObserver() override
    {
        LeakObserver::release<CombineLatestObserver>();
    }

public:
    void subscribe(const std::vector<std::shared_ptr<Observable> > &sources)
    {
        for (size_t i = 0; i < sources.size(); ++i) {
            if (mDone.load(std::memory_order_acquire)) {
                break;
            }
            auto inner = std::make_shared<CombineLatestInnerObserver>(this->shared_from_this(), i);
            sources[i]->subscribe(inner);
        }
    }

    void onSubscribe(size_t index, const DisposablePtr &d)
    {
        bool disposeNow = false;
        {
            GLockerGuard lock(mMutex);
            if (mDone.load(std::memory_order_acquire)) {
                disposeNow = true;
            } else {
                mDisposables[index] = d;
            }
        }
        if (disposeNow) {
            d->dispose();
        }
    }

    void onNext(size_t index, const GAny &value)
    {
        std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
        std::vector<GAny> values;
        ObserverPtr downstream;
        {
            GLockerGuard lock(mMutex);
            if (mDone.load(std::memory_order_acquire)) {
                return;
            }

            mValues[index] = value;
            if (!mHasValue[index]) {
                mHasValue[index] = true;
                mEmittedCount++;
            }

            if (mEmittedCount != mValues.size()) {
                return;
            }
            values = mValues;
            ++mInFlight;
            downstream = mDownstream;
        }

        if (mDone.load(std::memory_order_acquire)) {
            GLockerGuard lock(mMutex);
            --mInFlight;
            return;
        }

        GAny result;
        try {
            result = mCombiner(values);
        } catch (...) {
            onError(ExceptionHelper::fromCurrentException("CombineLatest: Combiner failed"));
            return;
        }

        if (!mDone.load(std::memory_order_acquire) && downstream) {
            downstream->onNext(result);
        }

        ObserverPtr completionDownstream;
        std::vector<DisposablePtr> disposables;
        {
            GLockerGuard lock(mMutex);
            if (mDone.load(std::memory_order_acquire)) {
                return;
            }
            --mInFlight;
            if (mActiveCount == 0 && mInFlight == 0) {
                mDone.store(true, std::memory_order_release);
                completionDownstream = mDownstream;
                mDownstream = nullptr;
                takeDisposables(disposables);
            }
        }
        disposeAll(disposables);
        if (completionDownstream) {
            completionDownstream->onComplete();
        }
    }

    void onError(const GAnyException &e)
    {
        std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }

        ObserverPtr downstream;
        std::vector<DisposablePtr> disposables;
        {
            GLockerGuard lock(mMutex);
            downstream = mDownstream;
            mDownstream = nullptr;
            takeDisposables(disposables);
        }
        disposeAll(disposables);
        if (downstream) {
            downstream->onError(e);
        }
    }

    void onComplete(size_t index)
    {
        std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
        ObserverPtr downstream;
        std::vector<DisposablePtr> disposables;
        {
            GLockerGuard lock(mMutex);
            if (mDone.load(std::memory_order_acquire)) {
                return;
            }

            if (mHasValue[index]) {
                mActiveCount--;
                if (mActiveCount != 0 || mInFlight != 0) {
                    return;
                }
            }
            mDone.store(true, std::memory_order_release);
            downstream = mDownstream;
            mDownstream = nullptr;
            takeDisposables(disposables);
        }
        disposeAll(disposables);
        if (downstream) {
            downstream->onComplete();
        }
    }

    void dispose() override
    {
        std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        std::vector<DisposablePtr> disposables;
        {
            GLockerGuard lock(mMutex);
            mDownstream = nullptr;
            takeDisposables(disposables);
        }
        disposeAll(disposables);
    }

    bool isDisposed() const override
    {
        return mDone.load(std::memory_order_acquire);
    }

private:
    void takeDisposables(std::vector<DisposablePtr> &disposables)
    {
        disposables = std::move(mDisposables);
        mDisposables.clear();
    }

    static void disposeAll(const std::vector<DisposablePtr> &disposables)
    {
        for (const auto &disposable: disposables) {
            if (disposable) {
                disposable->dispose();
            }
        }
    }

private:
    ObserverPtr mDownstream;
    CombineLatestFunction mCombiner;

    std::vector<GAny> mValues;
    std::vector<bool> mHasValue;
    size_t mActiveCount;
    size_t mEmittedCount;
    size_t mInFlight = 0;

    std::vector<DisposablePtr> mDisposables;
    std::atomic<bool> mDone = false;
    GMutex mMutex;
    std::recursive_mutex mSignalLock;
};

inline void CombineLatestInnerObserver::onSubscribe(const DisposablePtr &d)
{
    if (const auto parent = mParent.lock()) {
        mUpstream = d;
        parent->onSubscribe(mIndex, d);
    } else {
        d->dispose();
    }
}

inline void CombineLatestInnerObserver::onNext(const GAny &value)
{
    if (const auto parent = mParent.lock()) {
        parent->onNext(mIndex, value);
    }
}

inline void CombineLatestInnerObserver::onError(const GAnyException &e)
{
    if (const auto parent = mParent.lock()) {
        parent->onError(e);
    }
}

inline void CombineLatestInnerObserver::onComplete()
{
    if (const auto parent = mParent.lock()) {
        parent->onComplete(mIndex);
    }
}

class ObservableCombineLatest : public Observable
{
public:
    ObservableCombineLatest(std::vector<std::shared_ptr<Observable> > sources, CombineLatestFunction combiner)
        : mSources(std::move(sources)), mCombiner(std::move(combiner))
    {
        LeakObserver::make<ObservableCombineLatest>();
    }

    ~ObservableCombineLatest() override
    {
        LeakObserver::release<ObservableCombineLatest>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        if (mSources.empty()) {
            EmptyDisposable::complete(observer.get());
            return;
        }

        const auto parent = std::make_shared<CombineLatestObserver>(observer, mCombiner, mSources.size());
        observer->onSubscribe(parent);
        parent->subscribe(mSources);
    }

private:
    std::vector<std::shared_ptr<Observable> > mSources;
    CombineLatestFunction mCombiner;
};
} // namespace rx

#endif // RX_OBSERVABLE_COMBINE_LATEST_H
