//
// Created by Gxin on 2026/1/11.
//

#ifndef RX_OBSERVABLE_FLAT_MAP_H
#define RX_OBSERVABLE_FLAT_MAP_H

#include "../observable.h"
#include <gx/gmutex.h>
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
    }

    ~InnerObserver() override = default;

    void onSubscribe(const DisposablePtr &d) override
    {
        mDisposable = d;
    }

    void onNext(const GAny &value) override;

    void onError(const GAnyException &e) override;

    void onComplete() override;

    void dispose() override
    {
        if (const auto d = mDisposable) {
            d->dispose();
            mDisposable = nullptr;
        }
    }

    bool isDisposed() const override
    {
        if (const auto d = mDisposable) {
            return d->isDisposed();
        }
        return true;
    }

    DisposablePtr getDisposable() { return mDisposable; }

private:
    std::weak_ptr<FlatMapObserver> mParent;
    uint64_t mId;
    DisposablePtr mDisposable;
};

class FlatMapObserver : public Observer, public Disposable, public std::enable_shared_from_this<FlatMapObserver>
{
public:
    explicit FlatMapObserver(const ObserverPtr &observer, const FlatMapFunction &function)
        : mDownstream(observer), mFunction(function)
    {
        mActiveCount.store(1);
    }

    ~FlatMapObserver() override = default;

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        if (Disposable::validate(mUpstream.get(), d.get())) {
            mUpstream = d;
            mDownstream->onSubscribe(this->shared_from_this());
        }
    }

    void onNext(const GAny &value) override
    {
        if (mDown.load()) {
            return;
        }

        std::shared_ptr<Observable> p;
        try {
            p = mFunction(value);
        } catch (const GAnyException &e) {
            dispose();
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
        if (mDown.exchange(true)) {
            return;
        }
        dispose();
        mDownstream->onError(e);
    }

    void onComplete() override
    {
        if (mActiveCount.fetch_sub(1) == 1) {
            if (!mDown.exchange(true)) {
                mDownstream->onComplete();
            }
        }
    }

    void dispose() override
    {
        if (!mDisposed.exchange(true)) {
            if (mUpstream) {
                mUpstream->dispose();
                mUpstream = nullptr;
            }

            GLockerGuard lock(mInnerLock);
            for (const auto &pair: mInnerObservers) {
                pair.second->dispose();
            }
            mInnerObservers.clear();
        }
    }

    bool isDisposed() const override
    {
        return mDisposed.load();
    }

    void innerNext(const GAny &value)
    {
        if (isDisposed())
            return;

        GLockerGuard lock(mGate);
        mDownstream->onNext(value);
    }

    void innerError(const GAnyException &e)
    {
        if (mDown.exchange(true)) {
            return;
        }
        dispose();
        mDownstream->onError(e);
    }

    void innerComplete(uint64_t id)
    {
        removeInner(id);
        if (mActiveCount.fetch_sub(1) == 1) {
            if (!mDown.exchange(true)) {
                mDownstream->onComplete();
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
    }

    ~ObservableFlatMap() override = default;

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
