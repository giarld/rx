//
// Created by Gxin on 2026/1/11.
//

#ifndef RX_OBSERVABLE_FLAT_MAP_H
#define RX_OBSERVABLE_FLAT_MAP_H

#include "../observable.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"
#include <map>
#include <atomic>


namespace rx
{
class FlatMapObserver;

class InnerObserver : public Observer, public Disposable
{
public:
    explicit InnerObserver(const std::shared_ptr<FlatMapObserver> &parent, uint64_t id)
        : mParent(parent), mId(id)
    {
        LeakObserver::make<InnerObserver>();
    }

    ~InnerObserver() override
    {
        LeakObserver::release<InnerObserver>();
    }

    void onSubscribe(const DisposablePtr &d) override
    {
        DisposableHelper::setOnce(mDisposable, d, mLock);
    }

    void onNext(const GAny &value) override;

    void onError(const GAnyException &e) override;

    void onComplete() override;

    void dispose() override
    {
        DisposableHelper::dispose(mDisposable, mLock);
    }

    bool isDisposed() const override
    {
        return DisposableHelper::isDisposed(mDisposable);
    }

    DisposablePtr getDisposable() { return mDisposable; }

private:
    std::weak_ptr<FlatMapObserver> mParent;
    uint64_t mId;
    DisposablePtr mDisposable;
    GMutex mLock;
};

class FlatMapObserver : public Observer, public Disposable, public std::enable_shared_from_this<FlatMapObserver>
{
public:
    explicit FlatMapObserver(const ObserverPtr &observer, const FlatMapFunction &function)
        : mDownstream(observer), mFunction(function)
    {
        LeakObserver::make<FlatMapObserver>();
        mActiveCount.store(1);
    }

    ~FlatMapObserver() override
    {
        LeakObserver::release<FlatMapObserver>();
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
        if (mDown.load(std::memory_order_acquire)) {
            return;
        }

        std::shared_ptr<Observable> p;
        try {
            p = mFunction(value);
        } catch (const GAnyException &e) {
            mUpstream->dispose();
            onError(e);
            return;
        }

        if (!p) {
            return;
        }

        mActiveCount.fetch_add(1);
        uint64_t id = mIdGenerator.fetch_add(1);
        const auto inner = std::make_shared<InnerObserver>(this->shared_from_this(), id);
        addInner(id, inner);
        p->subscribe(inner);
    }

    void onError(const GAnyException &e) override
    {
        if (mDown.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        if (const auto d = mDownstream) {
            d->onError(e);
        }

        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    void onComplete() override
    {
        if (mActiveCount.fetch_sub(1) == 1) {
            if (!mDown.exchange(true, std::memory_order_acq_rel)) {
                if (const auto d = mDownstream) {
                    d->onComplete();
                }

                mDownstream = nullptr;
                mUpstream = nullptr;
            }
        }
    }

    void dispose() override
    {
        if (!mDisposed.exchange(true)) {
            if (const auto d = mUpstream) {
                d->dispose();
                mUpstream = nullptr;
            }

            GLockerGuard lock(mInnerLock);
            for (const auto &pair: mInnerObservers) {
                pair.second->dispose();
            }
            mInnerObservers.clear();

            mDownstream = nullptr;
        }
    }

    bool isDisposed() const override
    {
        return mDisposed.load(std::memory_order_acquire);
    }

    void innerNext(const GAny &value)
    {
        if (isDisposed())
            return;

        GLockerGuard lock(mGate);
        if (const auto d = mDownstream) {
            d->onNext(value);
        }
    }

    void innerError(const GAnyException &e)
    {
        if (mDown.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        dispose();
        if (const auto d = mDownstream) {
            d->onError(e);
        }
    }

    void innerComplete(uint64_t id)
    {
        removeInner(id);
        if (mActiveCount.fetch_sub(1) == 1) {
            if (!mDown.exchange(true, std::memory_order_acq_rel)) {
                if (const auto d = mDownstream) {
                    d->onComplete();
                }
            }
        }
    }

private:
    void addInner(uint64_t id, const std::shared_ptr<InnerObserver> &inner)
    {
        GLockerGuard lock(mInnerLock);
        if (!isDisposed()) {
            mInnerObservers[id] = inner;
        }
    }

    void removeInner(uint64_t id)
    {
        GLockerGuard lock(mInnerLock);
        mInnerObservers.erase(id);
    }

private:
    ObserverPtr mDownstream;
    FlatMapFunction mFunction;
    DisposablePtr mUpstream;

    std::atomic<bool> mDown = false;
    std::atomic<bool> mDisposed = false;

    std::atomic<size_t> mActiveCount = 0;

    std::map<uint64_t, std::shared_ptr<InnerObserver> > mInnerObservers;
    GMutex mInnerLock;
    std::atomic<uint64_t> mIdGenerator = 0;
    GMutex mGate;
};

class ObservableFlatMap : public Observable
{
public:
    explicit ObservableFlatMap(const ObservableSourcePtr &source, const FlatMapFunction &function)
        : mSource(source), mFunction(function)
    {
        LeakObserver::make<ObservableFlatMap>();
    }

    ~ObservableFlatMap() override
    {
        LeakObserver::release<ObservableFlatMap>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<FlatMapObserver>(observer, mFunction));
    }

private:
    ObservableSourcePtr mSource;
    FlatMapFunction mFunction;
};


// ===================

inline void InnerObserver::onNext(const GAny &value)
{
    if (const auto p = mParent.lock()) {
        p->innerNext(value);
    }
}

inline void InnerObserver::onError(const GAnyException &e)
{
    if (const auto p = mParent.lock()) {
        p->innerError(e);
    }
}

inline void InnerObserver::onComplete()
{
    if (const auto p = mParent.lock()) {
        p->innerComplete(mId);
    }
}
} // rx

#endif //RX_OBSERVABLE_FLAT_MAP_H
