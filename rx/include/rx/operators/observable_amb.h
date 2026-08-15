//
// Created by Gxin on 2026/2/6.
//

#ifndef RX_OBSERVABLE_AMB_H
#define RX_OBSERVABLE_AMB_H

#include "../observable.h"
#include "../leak_observer.h"
#include <vector>
#include <atomic>


namespace rx
{
class AmbCoordinator;

class AmbInnerObserver : public Observer
{
public:
    AmbInnerObserver(std::shared_ptr<AmbCoordinator> parent, size_t index)
        : mParent(std::move(parent)), mIndex(index)
    {
        LeakObserver::make<AmbInnerObserver>();
    }

    ~AmbInnerObserver() override
    {
        LeakObserver::release<AmbInnerObserver>();
    }

public:
    void onSubscribe(const DisposablePtr &d) override;

    void onNext(const GAny &value) override;

    void onError(const GAnyException &e) override;

    void onComplete() override;

private:
    std::weak_ptr<AmbCoordinator> mParent;
    size_t mIndex;
};

class AmbCoordinator : public Disposable, public std::enable_shared_from_this<AmbCoordinator>
{
public:
    AmbCoordinator(const ObserverPtr &downstream, size_t count)
        : mDownstream(downstream), mDisposables(count)
    {
        LeakObserver::make<AmbCoordinator>();
    }

    ~AmbCoordinator() override
    {
        LeakObserver::release<AmbCoordinator>();
    }

public:
    void subscribe(const std::vector<std::shared_ptr<Observable> > &sources)
    {
        for (size_t i = 0; i < sources.size(); ++i) {
            auto observer = std::make_shared<AmbInnerObserver>(shared_from_this(), i);
            if (isDisposed()) {
                return;
            }
            sources[i]->subscribe(observer);
        }
    }

    void onSubscribe(size_t index, const DisposablePtr &d)
    {
        bool disposeNow = false;
        {
            GLockerGuard lock(mLock);
            if (mDisposables[index]) {
                disposeNow = true;
            } else {
                mDisposables[index] = d;
                const int winner = mWinner.load(std::memory_order_acquire);
                disposeNow = (winner != -1 && static_cast<size_t>(winner) != index) || mIsDisposed;
                if (disposeNow) {
                    mDisposables[index] = DisposableHelper::disposed();
                }
            }
        }
        if (disposeNow) {
            d->dispose();
        }
    }

    bool tryWin(size_t index)
    {
        if (mDone.load(std::memory_order_acquire) || mIsDisposed.load(std::memory_order_acquire)) {
            return false;
        }
        int expected = -1;
        if (mWinner.compare_exchange_strong(expected, static_cast<int>(index), std::memory_order_acq_rel)) {
            // Won
            disposeAll(index);
            return true;
        }
        // If failed, check if I am the winner (re-entry?)
        return !mDone.load(std::memory_order_acquire)
               && !mIsDisposed.load(std::memory_order_acquire)
               && mWinner.load(std::memory_order_acquire) == static_cast<int>(index);
    }

    void onNext(size_t index, const GAny &value)
    {
        if (tryWin(index)) {
            if (const auto d = mDownstream) {
                d->onNext(value);
            }
        }
    }

    void onError(size_t index, const GAnyException &e)
    {
        if (!tryWin(index) || mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        const auto downstream = mDownstream;
        mIsDisposed.store(true, std::memory_order_release);
        disposeAll((size_t) -1);
        if (downstream) {
            downstream->onError(e);
        }
    }

    void onComplete(size_t index)
    {
        if (!tryWin(index) || mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        const auto downstream = mDownstream;
        mIsDisposed.store(true, std::memory_order_release);
        disposeAll((size_t) -1);
        if (downstream) {
            downstream->onComplete();
        }
    }

    void dispose() override
    {
        if (!mIsDisposed.exchange(true, std::memory_order_acq_rel)) {
            mDone.store(true, std::memory_order_release);
            disposeAll((size_t) -1);
        }
    }

    bool isDisposed() const override
    {
        return mIsDisposed.load(std::memory_order_acquire);
    }

private:
    void disposeAll(size_t keepIndex)
    {
        std::vector<DisposablePtr> disposables;
        {
            GLockerGuard lock(mLock);
            for (size_t i = 0; i < mDisposables.size(); ++i) {
                if (i != keepIndex) {
                    auto d = mDisposables[i];
                    if (d && d != DisposableHelper::disposed()) {
                        disposables.push_back(d);
                    }
                    mDisposables[i] = DisposableHelper::disposed();
                }
            }
            if (keepIndex == (size_t) -1) {
                mDownstream = nullptr;
            }
        }
        for (const auto &disposable: disposables) {
            disposable->dispose();
        }
    }

private:
    ObserverPtr mDownstream;
    std::vector<DisposablePtr> mDisposables;
    std::atomic<int> mWinner{-1};
    GMutex mLock;
    std::atomic<bool> mDone{false};
    std::atomic<bool> mIsDisposed{false};
};

inline void AmbInnerObserver::onSubscribe(const DisposablePtr &d)
{
    if (const auto p = mParent.lock()) {
        p->onSubscribe(mIndex, d);
    }
}

inline void AmbInnerObserver::onNext(const GAny &value)
{
    if (const auto p = mParent.lock()) {
        p->onNext(mIndex, value);
    }
}

inline void AmbInnerObserver::onError(const GAnyException &e)
{
    if (const auto p = mParent.lock()) {
        p->onError(mIndex, e);
    }
}

inline void AmbInnerObserver::onComplete()
{
    if (const auto p = mParent.lock()) {
        p->onComplete(mIndex);
    }
}

// --------------------------------------------------------

class ObservableAmb : public Observable
{
public:
    explicit ObservableAmb(std::vector<std::shared_ptr<Observable> > sources)
        : mSources(std::move(sources))
    {
        LeakObserver::make<ObservableAmb>();
    }

    ~ObservableAmb() override
    {
        LeakObserver::release<ObservableAmb>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        if (mSources.empty()) {
            observer->onSubscribe(DisposableHelper::disposed());
            observer->onComplete();
            return;
        }

        if (mSources.size() == 1) {
            mSources[0]->subscribe(observer);
            return;
        }

        const auto coordinator = std::make_shared<AmbCoordinator>(observer, mSources.size());
        observer->onSubscribe(coordinator);
        coordinator->subscribe(mSources);
    }

private:
    std::vector<std::shared_ptr<Observable> > mSources;
};
} // rx

#endif //RX_OBSERVABLE_AMB_H
