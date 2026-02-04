//
// Created by Gxin on 2026/2/4.
//

#ifndef RX_OBSERVABLE_DISTINCT_UNTIL_CHANGED_H
#define RX_OBSERVABLE_DISTINCT_UNTIL_CHANGED_H

#include "../observable.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"


namespace rx
{
class DistinctUntilChangedObserver : public Observer, public Disposable, public std::enable_shared_from_this<DistinctUntilChangedObserver>
{
public:
    explicit DistinctUntilChangedObserver(const ObserverPtr &observer, 
                                          MapFunction keySelector,
                                          ComparatorFunction comparator)
        : mKeySelector(std::move(keySelector))
        , mComparator(std::move(comparator))
        , mDownstream(observer)
    {
        LeakObserver::make<DistinctUntilChangedObserver>();
    }

    ~DistinctUntilChangedObserver() override
    {
        LeakObserver::release<DistinctUntilChangedObserver>();
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
        if (mDone.load(std::memory_order_acquire)) {
            return;
        }

        GAny key;
        try {
            key = mKeySelector ? mKeySelector(value) : value;
        } catch (const GAnyException &e) {
            mUpstream->dispose();
            onError(e);
            return;
        }

        bool shouldEmit = false;
        bool hasError = false;
        std::shared_ptr<GAnyException> error;
        //
        {
            GLockerGuard locker(mLock);
            if (!mHasValue) {
                mHasValue = true;
                mLastKey = key;
                shouldEmit = true;
            } else {
                bool equal = false;
                try {
                    if (mComparator) {
                        equal = mComparator(mLastKey, key);
                    } else {
                        equal = (mLastKey == key);
                    }
                } catch (const GAnyException &e) {
                    hasError = true;
                    error = std::make_shared<GAnyException>(e);
                }
                if (!hasError && !equal) {
                    mLastKey = key;
                    shouldEmit = true;
                }
            }
        }

        if (hasError) {
            mUpstream->dispose();
            onError(*error);
            return;
        }

        if (shouldEmit) {
            if (const auto d = mDownstream) {
                d->onNext(value);
            }
        }
    }

    void onError(const GAnyException &e) override
    {
        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        if (const auto d = mDownstream) {
            d->onError(e);
        }
        clear();
    }

    void onComplete() override
    {
        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        if (const auto d = mDownstream) {
            d->onComplete();
        }
        clear();
    }

    void dispose() override
    {
        if (const auto d = mUpstream) {
            d->dispose();
            mUpstream = nullptr;
        }
        clear();
    }

    bool isDisposed() const override
    {
        if (const auto d = mUpstream) {
            return d->isDisposed();
        }
        return true;
    }

private:
    void clear()
    {
        mDownstream = nullptr;
        mUpstream = nullptr;
        GLockerGuard locker(mLock);
        mLastKey = GAny();
        mHasValue = false;
    }

private:
    MapFunction mKeySelector;
    ComparatorFunction mComparator;
    ObserverPtr mDownstream;
    DisposablePtr mUpstream;
    std::atomic<bool> mDone = false;
    GMutex mLock;
    GAny mLastKey;
    bool mHasValue = false;
};

class ObservableDistinctUntilChanged : public Observable
{
public:
    explicit ObservableDistinctUntilChanged(ObservableSourcePtr source, 
                                            MapFunction keySelector,
                                            ComparatorFunction comparator)
        : mSource(std::move(source))
        , mKeySelector(std::move(keySelector))
        , mComparator(std::move(comparator))
    {
        LeakObserver::make<ObservableDistinctUntilChanged>();
    }

    ~ObservableDistinctUntilChanged() override
    {
        LeakObserver::release<ObservableDistinctUntilChanged>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<DistinctUntilChangedObserver>(observer, mKeySelector, mComparator));
    }

private:
    ObservableSourcePtr mSource;
    MapFunction mKeySelector;
    ComparatorFunction mComparator;
};
} // rx

#endif //RX_OBSERVABLE_DISTINCT_UNTIL_CHANGED_H
