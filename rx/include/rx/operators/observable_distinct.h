//
// Created by Gxin on 2026/2/4.
//

#ifndef RX_OBSERVABLE_DISTINCT_H
#define RX_OBSERVABLE_DISTINCT_H

#include "../observable.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"

#include <vector>


namespace rx
{
class DistinctObserver : public Observer, public Disposable, public std::enable_shared_from_this<DistinctObserver>
{
public:
    explicit DistinctObserver(const ObserverPtr &observer, MapFunction keySelector)
        : mKeySelector(std::move(keySelector)), mDownstream(observer)
    {
        LeakObserver::make<DistinctObserver>();
    }

    ~DistinctObserver() override
    {
        LeakObserver::release<DistinctObserver>();
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

        // Check if key already exists using linear search
        bool isNew = false;
        bool hasError = false;
        std::shared_ptr<GAnyException> error;
        {
            GLockerGuard locker(mLock);
            try {
                bool found = false;
                for (const auto &k : mSeenKeys) {
                    if (k == key) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    mSeenKeys.push_back(key);
                    isNew = true;
                }
            } catch (const GAnyException &e) {
                hasError = true;
                error = std::make_shared<GAnyException>(e);
            }
        }

        if (hasError) {
            mUpstream->dispose();
            onError(*error);
            return;
        }

        if (isNew) {
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
        mSeenKeys.clear();
    }

private:
    MapFunction mKeySelector;
    ObserverPtr mDownstream;
    DisposablePtr mUpstream;
    std::atomic<bool> mDone = false;
    GMutex mLock;
    std::vector<GAny> mSeenKeys;
};

class ObservableDistinct : public Observable
{
public:
    explicit ObservableDistinct(ObservableSourcePtr source, MapFunction keySelector)
        : mSource(std::move(source)), mKeySelector(std::move(keySelector))
    {
        LeakObserver::make<ObservableDistinct>();
    }

    ~ObservableDistinct() override
    {
        LeakObserver::release<ObservableDistinct>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<DistinctObserver>(observer, mKeySelector));
    }

private:
    ObservableSourcePtr mSource;
    MapFunction mKeySelector;
};
} // rx

#endif //RX_OBSERVABLE_DISTINCT_H
