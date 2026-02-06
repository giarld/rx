//
// Created by Gxin on 2026/1/17.
//

#ifndef RX_OBSERVABLE_COMBINE_LATEST_H
#define RX_OBSERVABLE_COMBINE_LATEST_H

#include "observable_empty.h"
#include "../observable.h"
#include "../leak_observer.h"


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
        GLockerGuard lock(mMutex);
        if (mDone.load(std::memory_order_acquire)) {
            d->dispose();
            return;
        }
        mDisposables[index] = d;
    }

    void onNext(size_t index, const GAny &value)
    {
        GLocker<GMutex> lock(mMutex);
        if (mDone.load(std::memory_order_acquire)) {
            return;
        }

        mValues[index] = value;
        if (!mHasValue[index]) {
            mHasValue[index] = true;
            mEmittedCount++;
        }

        if (mEmittedCount == mValues.size()) {
            GAny result;
            try {
                result = mCombiner(mValues);
            } catch (const GAnyException &e) {
                lock.unlock();
                onError(e);
                return;
            }
            if (const auto d = mDownstream) {
                d->onNext(result);
            }
        }
    }

    void onError(const GAnyException &e)
    {
        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }

        disposeAllInternal();

        if (const auto d = mDownstream) {
            d->onError(e);
        }
        mDownstream = nullptr;
    }

    void onComplete(size_t index)
    {
        GLocker<GMutex> lock(mMutex);
        if (mDone.load(std::memory_order_acquire)) {
            return;
        }

        if (!mHasValue[index]) {
            mDone.store(true, std::memory_order_release);
            disposeAllInternalNoLock();
            lock.unlock();
            if (const auto d = mDownstream) {
                d->onComplete();
            }
            mDownstream = nullptr;
            return;
        }

        mActiveCount--;
        if (mActiveCount == 0) {
            mDone.store(true, std::memory_order_release);
            disposeAllInternalNoLock();
            lock.unlock();
            if (const auto d = mDownstream) {
                d->onComplete();
            }
            mDownstream = nullptr;
        }
    }

    void dispose() override
    {
        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        disposeAllInternal();
        mDownstream = nullptr;
    }

    bool isDisposed() const override
    {
        return mDone.load(std::memory_order_acquire);
    }

private:
    void disposeAllInternal()
    {
        GLockerGuard lock(mMutex);
        disposeAllInternalNoLock();
    }

    void disposeAllInternalNoLock()
    {
        for (auto &d: mDisposables) {
            if (d) {
                d->dispose();
                d = nullptr;
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

    std::vector<DisposablePtr> mDisposables;
    std::atomic<bool> mDone = false;
    GMutex mMutex;
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
