//
// Created by Gxin on 2026/2/4.
//

#ifndef RX_OBSERVABLE_SWITCH_MAP_H
#define RX_OBSERVABLE_SWITCH_MAP_H

#include "../observable.h"
#include "../exception_helper.h"
#include "../observer.h"
#include "../disposables/atomic_disposable.h"
#include "../disposables/sequential_disposable.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"
#include <atomic>
#include <gx/gmutex.h>


namespace rx
{
class SwitchMapObserver;

class SwitchMapInnerObserver : public Observer
{
public:
    SwitchMapInnerObserver(const std::shared_ptr<SwitchMapObserver> &parent, uint64_t id);

    ~SwitchMapInnerObserver() override;

public:
    void onSubscribe(const DisposablePtr &d) override;

    void onNext(const GAny &value) override;

    void onError(const GAnyException &e) override;

    void onComplete() override;

private:
    std::weak_ptr<SwitchMapObserver> mParent;
    uint64_t mId;
};

class SwitchMapObserver : public Observer, public Disposable, public std::enable_shared_from_this<SwitchMapObserver>
{
public:
    SwitchMapObserver(const ObserverPtr &downstream, const FlatMapFunction &mapper);

    ~SwitchMapObserver() override;

public:
    void onSubscribe(const DisposablePtr &d) override;

    void onNext(const GAny &value) override;

    void onError(const GAnyException &e) override;

    void onComplete() override;

    void dispose() override;

    bool isDisposed() const override;

    // Inner callbacks
    void innerSubscribe(uint64_t id, const DisposablePtr &d);

    void innerNext(uint64_t id, const GAny &value);

    void innerError(uint64_t id, const GAnyException &e);

    void innerComplete(uint64_t id);

private:
    ObserverPtr mDownstream;
    FlatMapFunction mMapper;
    DisposablePtr mUpstream;
    std::shared_ptr<SequentialDisposable> mInnerDisposable;

    std::atomic<bool> mDone{false};
    std::atomic<bool> mDisposed{false};
    std::atomic<uint64_t> mIndex{0};

    // We assume only one active inner observer, but we need to track if it's active for onComplete logic
    std::atomic<bool> mInnerActive{false};

    GMutex mLock; // Protects against race between main onNext/onComplete and inner events
};

class ObservableSwitchMap : public Observable
{
public:
    ObservableSwitchMap(ObservableSourcePtr source, FlatMapFunction mapper)
        : mSource(std::move(source)), mMapper(std::move(mapper))
    {
        LeakObserver::make<ObservableSwitchMap>();
    }

    ~ObservableSwitchMap() override
    {
        LeakObserver::release<ObservableSwitchMap>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<SwitchMapObserver>(observer, mMapper));
    }

private:
    ObservableSourcePtr mSource;
    FlatMapFunction mMapper;
};

// ==========================================
// Implementation
// ==========================================

// SwitchMapInnerObserver
inline SwitchMapInnerObserver::SwitchMapInnerObserver(const std::shared_ptr<SwitchMapObserver> &parent, uint64_t id)
    : mParent(parent), mId(id)
{
    LeakObserver::make<SwitchMapInnerObserver>();
}

inline SwitchMapInnerObserver::~SwitchMapInnerObserver()
{
    LeakObserver::release<SwitchMapInnerObserver>();
}

inline void SwitchMapInnerObserver::onSubscribe(const DisposablePtr &d)
{
    if (const auto p = mParent.lock()) {
        p->innerSubscribe(mId, d);
    }
}

inline void SwitchMapInnerObserver::onNext(const GAny &value)
{
    if (const auto p = mParent.lock()) {
        p->innerNext(mId, value);
    }
}

inline void SwitchMapInnerObserver::onError(const GAnyException &e)
{
    if (const auto p = mParent.lock()) {
        p->innerError(mId, e);
    }
}

inline void SwitchMapInnerObserver::onComplete()
{
    if (const auto p = mParent.lock()) {
        p->innerComplete(mId);
    }
}

// SwitchMapObserver
inline SwitchMapObserver::SwitchMapObserver(const ObserverPtr &downstream, const FlatMapFunction &mapper)
    : mDownstream(downstream), mMapper(mapper)
{
    LeakObserver::make<SwitchMapObserver>();
    mInnerDisposable = std::make_shared<SequentialDisposable>();
}

inline SwitchMapObserver::~SwitchMapObserver()
{
    LeakObserver::release<SwitchMapObserver>();
}

inline void SwitchMapObserver::onSubscribe(const DisposablePtr &d)
{
    if (DisposableHelper::validate(mUpstream, d)) {
        mUpstream = d;
        mDownstream->onSubscribe(shared_from_this());
    }
}

inline void SwitchMapObserver::onNext(const GAny &value)
{
    if (mDone || mDisposed)
        return;

    uint64_t id;
    //
    {
        GLockerGuard lock(mLock);
        id = ++mIndex;
        mInnerActive = true;
    }

    // A new upstream value switches away from the previous inner immediately,
    // even if the mapper fails or returns a null Observable.
    mInnerDisposable->update(std::make_shared<AtomicDisposable>());

    std::shared_ptr<Observable> p;
    try {
        p = mMapper(value);
    } catch (...) {
        onError(ExceptionHelper::fromCurrentException("SwitchMap: Mapper failed"));
        return;
    }

    if (!p) {
        onError(GAnyException("SwitchMap: Mapper returned null Observable"));
        return;
    }

    const auto inner = std::make_shared<SwitchMapInnerObserver>(shared_from_this(), id);
    p->subscribe(inner);
}

inline void SwitchMapObserver::onError(const GAnyException &e)
{
    if (mDone || mDisposed)
        return;
    mDone = true;
    const auto downstream = mDownstream;
    dispose();
    if (downstream) {
        downstream->onError(e);
    }
}

inline void SwitchMapObserver::onComplete()
{
    if (mDone || mDisposed)
        return;

    ObserverPtr downstream;
    {
        GLockerGuard lock(mLock);
        mDone = true;
        if (!mInnerActive) {
            downstream = mDownstream;
        }
    }
    if (downstream) {
        downstream->onComplete();
    }
}

inline void SwitchMapObserver::dispose()
{
    if (!mDisposed.exchange(true)) {
        if (mUpstream)
            mUpstream->dispose();
        if (mInnerDisposable)
            mInnerDisposable->dispose();
    }
}

inline bool SwitchMapObserver::isDisposed() const
{
    return mDisposed;
}

inline void SwitchMapObserver::innerSubscribe(uint64_t id, const DisposablePtr &d)
{
    bool current = false;
    {
        GLockerGuard lock(mLock);
        current = !mDisposed && mIndex == id;
    }
    if (current) {
        mInnerDisposable->update(d);
    } else {
        d->dispose();
    }
}

inline void SwitchMapObserver::innerNext(uint64_t id, const GAny &value)
{
    ObserverPtr downstream;
    {
        GLockerGuard lock(mLock);
        if (!mDisposed && mIndex == id) {
            downstream = mDownstream;
        }
    }
    if (downstream) {
        downstream->onNext(value);
    }
}

inline void SwitchMapObserver::innerError(uint64_t id, const GAnyException &e)
{
    bool current = false;
    {
        GLockerGuard lock(mLock);
        current = !mDisposed && mIndex == id;
    }
    if (current) {
        onError(e);
    }
}

inline void SwitchMapObserver::innerComplete(uint64_t id)
{
    ObserverPtr downstream;
    {
        GLockerGuard lock(mLock);
        if (!mDisposed && mIndex == id) {
            mInnerActive = false;
            if (mDone) {
                downstream = mDownstream;
            }
        }
    }
    if (downstream) {
        downstream->onComplete();
    }
}
} // rx

#endif //RX_OBSERVABLE_SWITCH_MAP_H
