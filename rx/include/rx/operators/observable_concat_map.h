//
// Created by Gxin on 2026/2/4.
//

#ifndef RX_OBSERVABLE_CONCAT_MAP_H
#define RX_OBSERVABLE_CONCAT_MAP_H

#include "../observable.h"
#include "../observer.h"
#include "../disposables/sequential_disposable.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"
#include <deque>
#include <atomic>
#include <gx/gmutex.h>


namespace rx
{
class ConcatMapObserver;

class ConcatMapInnerObserver : public Observer
{
public:
    explicit ConcatMapInnerObserver(const std::shared_ptr<ConcatMapObserver> &parent);

    ~ConcatMapInnerObserver() override;

    void onSubscribe(const DisposablePtr &d) override;

    void onNext(const GAny &value) override;

    void onError(const GAnyException &e) override;

    void onComplete() override;

private:
    std::shared_ptr<ConcatMapObserver> mParent;
};

class ConcatMapObserver : public Observer, public Disposable, public std::enable_shared_from_this<ConcatMapObserver>
{
public:
    ConcatMapObserver(const ObserverPtr &downstream, const FlatMapFunction &mapper);

    ~ConcatMapObserver() override;

    void onSubscribe(const DisposablePtr &d) override;

    void onNext(const GAny &value) override;

    void onError(const GAnyException &e) override;

    void onComplete() override;

    void dispose() override;

    bool isDisposed() const override;

    // Inner callbacks
    void innerSubscribe(const DisposablePtr &d);

    void innerNext(const GAny &value);

    void innerError(const GAnyException &e);

    void innerComplete();

    // Helper
    void drain();

private:
    ObserverPtr mDownstream;
    FlatMapFunction mMapper;
    DisposablePtr mUpstream;
    std::shared_ptr<SequentialDisposable> mInnerDisposable;

    std::deque<GAny> mQueue;
    GMutex mLock;

    std::atomic<bool> mDone{false};
    std::atomic<bool> mDisposed{false};
    std::atomic<bool> mActive{false};
    std::atomic<int32_t> mWip{0};
};

class ObservableConcatMap : public Observable
{
public:
    ObservableConcatMap(ObservableSourcePtr source, FlatMapFunction mapper)
        : mSource(std::move(source)), mMapper(std::move(mapper))
    {
        LeakObserver::make<ObservableConcatMap>();
    }

    ~ObservableConcatMap() override
    {
        LeakObserver::release<ObservableConcatMap>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<ConcatMapObserver>(observer, mMapper));
    }

private:
    ObservableSourcePtr mSource;
    FlatMapFunction mMapper;
};

// ==========================================
// Implementation
// ==========================================

// ConcatMapInnerObserver
inline ConcatMapInnerObserver::ConcatMapInnerObserver(const std::shared_ptr<ConcatMapObserver> &parent)
    : mParent(parent)
{
    LeakObserver::make<ConcatMapInnerObserver>();
}

inline ConcatMapInnerObserver::~ConcatMapInnerObserver()
{
    LeakObserver::release<ConcatMapInnerObserver>();
}

inline void ConcatMapInnerObserver::onSubscribe(const DisposablePtr &d)
{
    if (mParent)
        mParent->innerSubscribe(d);
}

inline void ConcatMapInnerObserver::onNext(const GAny &value)
{
    if (mParent)
        mParent->innerNext(value);
}

inline void ConcatMapInnerObserver::onError(const GAnyException &e)
{
    if (mParent)
        mParent->innerError(e);
}

inline void ConcatMapInnerObserver::onComplete()
{
    if (mParent)
        mParent->innerComplete();
}

// ConcatMapObserver
inline ConcatMapObserver::ConcatMapObserver(const ObserverPtr &downstream, const FlatMapFunction &mapper)
    : mDownstream(downstream), mMapper(mapper)
{
    LeakObserver::make<ConcatMapObserver>();
    mInnerDisposable = std::make_shared<SequentialDisposable>();
}

inline ConcatMapObserver::~ConcatMapObserver()
{
    LeakObserver::release<ConcatMapObserver>();
}

inline void ConcatMapObserver::onSubscribe(const DisposablePtr &d)
{
    if (DisposableHelper::validate(mUpstream, d)) {
        mUpstream = d;
        mDownstream->onSubscribe(shared_from_this());
    }
}

inline void ConcatMapObserver::onNext(const GAny &value)
{
    if (mDone) {
        return;
    }
    //
    {
        GLockerGuard lock(mLock);
        mQueue.push_back(value);
    }
    drain();
}

inline void ConcatMapObserver::onError(const GAnyException &e)
{
    if (mDone)
        return;
    mDone = true;
    dispose();
    mDownstream->onError(e);
}

inline void ConcatMapObserver::onComplete()
{
    if (mDone)
        return;
    mDone = true;
    drain();
}

inline void ConcatMapObserver::dispose()
{
    mDisposed = true;
    if (mUpstream)
        mUpstream->dispose();
    if (mInnerDisposable)
        mInnerDisposable->dispose();

    GLockerGuard lock(mLock);
    mQueue.clear();
}

inline bool ConcatMapObserver::isDisposed() const
{
    return mDisposed;
}

inline void ConcatMapObserver::innerSubscribe(const DisposablePtr &d)
{
    mInnerDisposable->replace(d);
}

inline void ConcatMapObserver::innerNext(const GAny &value)
{
    mDownstream->onNext(value);
}

inline void ConcatMapObserver::innerError(const GAnyException &e)
{
    dispose();
    mDownstream->onError(e);
}

inline void ConcatMapObserver::innerComplete()
{
    mActive = false;
    drain();
}

inline void ConcatMapObserver::drain()
{
    if (mWip.fetch_add(1) != 0) {
        return;
    }

    int32_t missed = 1;
    while (true) {
        if (isDisposed()) {
            GLockerGuard lock(mLock);
            mQueue.clear();
            return;
        }

        if (!mActive) {
            bool d = mDone;
            bool empty;
            GAny v; {
                GLockerGuard lock(mLock);
                empty = mQueue.empty();
                if (!empty) {
                    v = mQueue.front();
                    mQueue.pop_front();
                }
            }

            // Log("Drain: d={}, empty={}, active={}", d, empty, mActive.load());

            if (d && empty) {
                mDisposed = true;
                if (mUpstream)
                    mUpstream->dispose();
                if (mInnerDisposable)
                    mInnerDisposable->dispose();

                if (const auto ds = mDownstream) {
                    ds->onComplete();
                }
                return;
            }

            if (!empty) {
                mActive = true;

                std::shared_ptr<Observable> p;
                try {
                    p = mMapper(v);
                } catch (const GAnyException &e) {
                    dispose();
                    mDownstream->onError(e);
                    return;
                } catch (...) {
                    dispose();
                    mDownstream->onError(GAnyException("ConcatMap: Mapper failed"));
                    return;
                }

                if (!p) {
                    // Mapper returned null, ignore and move to next
                    mActive = false;
                    continue;
                }

                auto inner = std::make_shared<ConcatMapInnerObserver>(shared_from_this());
                p->subscribe(inner);
            }
        }

        if (mWip.fetch_sub(missed) == missed) {
            break;
        }

        const int32_t old = mWip.fetch_add(-missed);
        if (old == missed) {
            break;
        }
        missed = old - missed;
    }
}
} // rx

#endif //RX_OBSERVABLE_CONCAT_MAP_H
