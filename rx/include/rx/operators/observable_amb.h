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
        GLockerGuard lock(mLock);
        if (mDisposables[index]) {
            d->dispose();
            return;
        }
        mDisposables[index] = d;
        // If we picked a winner before this subscription arrived, and it wasn't this one (impossible if serial, but possible if async),
        // or if generally disposed.
        // Actually, check general disposal or winner state.
        const int winner = mWinner.load(std::memory_order_acquire);
        if (winner != -1 && static_cast<size_t>(winner) != index) {
            // Someone else already won, dispose this new one immediately
            d->dispose();
        } else if (winner == -1 && mIsDisposed) {
            d->dispose();
        }
    }

    bool tryWin(size_t index)
    {
        int expected = -1;
        if (mWinner.compare_exchange_strong(expected, static_cast<int>(index), std::memory_order_acq_rel)) {
            // Won
            disposeAll(index);
            return true;
        }
        // If failed, check if I am the winner (re-entry?)
        return mWinner.load(std::memory_order_acquire) == static_cast<int>(index);
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
        if (tryWin(index)) {
            if (const auto d = mDownstream) {
                d->onError(e);
            }
            // Cleanup self
            // mDownstream = nullptr; // Don't clear downstream, we might need it? No, standard pattern.
        }
    }

    void onComplete(size_t index)
    {
        if (tryWin(index)) {
            if (const auto d = mDownstream) {
                d->onComplete();
            }
        }
    }

    void dispose() override
    {
        if (!mIsDisposed) {
            mIsDisposed = true;
            disposeAll((size_t) -1);
        }
    }

    bool isDisposed() const override
    {
        return mIsDisposed;
    }

private:
    void disposeAll(size_t keepIndex)
    {
        GLockerGuard lock(mLock);
        for (size_t i = 0; i < mDisposables.size(); ++i) {
            if (i != keepIndex) {
                auto d = mDisposables[i];
                if (d) {
                    d->dispose();
                    mDisposables[i] = DisposableHelper::disposed();
                }
            }
        }
        if (keepIndex == (size_t) -1) {
            mDownstream = nullptr;
        }
    }

private:
    ObserverPtr mDownstream;
    std::vector<DisposablePtr> mDisposables;
    std::atomic<int> mWinner{-1};
    GMutex mLock;
    bool mIsDisposed = false; // Flag for downstream disposal
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
