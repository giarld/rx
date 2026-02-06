//
// Created by Gxin on 2026/1/20.
//

#ifndef RX_OBSERVABLE_JOIN_H
#define RX_OBSERVABLE_JOIN_H

#include "../observable.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"
#include <map>
#include <vector>


namespace rx
{
class JoinMainObserver;

class JoinSupportObserver : public Observer, public Disposable
{
public:
    JoinSupportObserver(std::shared_ptr<JoinMainObserver> parent, bool isLeft)
        : mParent(std::move(parent)), mIsLeft(isLeft)
    {
        LeakObserver::make<JoinSupportObserver>();
    }

    ~JoinSupportObserver() override
    {
        LeakObserver::release<JoinSupportObserver>();
    }

public:
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

private:
    std::weak_ptr<JoinMainObserver> mParent;
    bool mIsLeft;
    DisposablePtr mDisposable;
    GMutex mLock;
};

class JoinDurationObserver : public Observer, public Disposable
{
public:
    JoinDurationObserver(std::shared_ptr<JoinMainObserver> parent, uint64_t id, bool isLeft)
        : mParent(std::move(parent)), mId(id), mIsLeft(isLeft)
    {
        LeakObserver::make<JoinDurationObserver>();
    }

    ~JoinDurationObserver() override
    {
        LeakObserver::release<JoinDurationObserver>();
    }

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        DisposableHelper::setOnce(mDisposable, d, mLock);
    }

    void onNext(const GAny &value) override
    {
        onComplete();
    }

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

private:
    std::weak_ptr<JoinMainObserver> mParent;
    uint64_t mId;
    bool mIsLeft;
    DisposablePtr mDisposable;
    GMutex mLock;
};

class JoinMainObserver : public Disposable, public std::enable_shared_from_this<JoinMainObserver>
{
public:
    JoinMainObserver(ObserverPtr downstream,
                     FlatMapFunction leftDurationSelector,
                     FlatMapFunction rightDurationSelector,
                     BiFunction resultSelector)
        : mDownstream(std::move(downstream)),
          mLeftDurationSelector(std::move(leftDurationSelector)),
          mRightDurationSelector(std::move(rightDurationSelector)),
          mResultSelector(std::move(resultSelector))
    {
        LeakObserver::make<JoinMainObserver>();
        mActiveCount.store(2);
    }

    ~JoinMainObserver() override
    {
        LeakObserver::release<JoinMainObserver>();
    }

public:
    void subscribe(const ObservableSourcePtr &left, const ObservableSourcePtr &right)
    {
        const auto leftObs = std::make_shared<JoinSupportObserver>(shared_from_this(), true);
        mDisposables.push_back(leftObs);
        left->subscribe(leftObs);

        const auto rightObs = std::make_shared<JoinSupportObserver>(shared_from_this(), false);
        mDisposables.push_back(rightObs);
        right->subscribe(rightObs);
    }

    void dispose() override
    {
        if (!mCancelled.exchange(true)) {
            for (const auto &d: mDisposables) {
                d->dispose();
            }
            mDisposables.clear();

            GLockerGuard lock(mGate);
            for (const auto &pair: mLeftDurations) {
                pair.second->dispose();
            }
            mLeftDurations.clear();
            mLefts.clear();

            for (const auto &pair: mRightDurations) {
                pair.second->dispose();
            }
            mRightDurations.clear();
            mRights.clear();
        }
    }

    bool isDisposed() const override
    {
        return mCancelled.load(std::memory_order_acquire);
    }

    void innerError(const GAnyException &e)
    {
        GLockerGuard lock(mGate);
        if (mCancelled.load(std::memory_order_acquire))
            return;

        mDownstream->onError(e);

        mCancelled.store(true);
        mLeftDurations.clear();
        mRightDurations.clear();
        mLefts.clear();
        mRights.clear();
    }

    void innerComplete(bool /*isLeft*/)
    {
        if (mActiveCount.fetch_sub(1) == 1) {
            GLockerGuard lock(mGate);
            if (!mCancelled.load(std::memory_order_acquire)) {
                mCancelled.store(true);
                mDownstream->onComplete();
                mLeftDurations.clear();
                mRightDurations.clear();
            }
        }
    }

    void innerValue(bool isLeft, const GAny &value)
    {
        GLocker lock(mGate);
        if (mCancelled.load(std::memory_order_acquire))
            return;

        uint64_t id = mIdGenerator++;

        ObservableSourcePtr durationObservable;
        try {
            if (isLeft) {
                mLefts[id] = value;
                durationObservable = mLeftDurationSelector(value);
            } else {
                mRights[id] = value;
                durationObservable = mRightDurationSelector(value);
            }
        } catch (const GAnyException &e) {
            lock.unlock();
            innerError(e);
            return;
        }

        if (!durationObservable) {
            lock.unlock();
            innerError(GAnyException("Join: Duration Selector returned null"));
            return;
        }

        const auto durationObserver = std::make_shared<JoinDurationObserver>(shared_from_this(), id, isLeft);
        if (isLeft) {
            mLeftDurations[id] = durationObserver;
        } else {
            mRightDurations[id] = durationObserver;
        }
        durationObservable->subscribe(durationObserver);

        if (isLeft) {
            for (const auto &pair: mRights) {
                emitResult(value, pair.second);
            }
        } else {
            for (const auto &pair: mLefts) {
                emitResult(pair.second, value);
            }
        }
    }

    void innerClose(bool isLeft, uint64_t id)
    {
        GLockerGuard lock(mGate);
        if (isLeft) {
            mLefts.erase(id);
            mLeftDurations.erase(id);
        } else {
            mRights.erase(id);
            mRightDurations.erase(id);
        }
    }

private:
    void emitResult(const GAny &left, const GAny &right)
    {
        GAny result;
        try {
            result = mResultSelector(left, right);
        } catch (const GAnyException &e) {
            innerError(e);
            return;
        }
        mDownstream->onNext(result);
    }

private:
    ObserverPtr mDownstream;
    FlatMapFunction mLeftDurationSelector;
    FlatMapFunction mRightDurationSelector;
    BiFunction mResultSelector;

    GMutex mGate;
    std::atomic<bool> mCancelled{false};
    std::atomic<int> mActiveCount{0};
    uint64_t mIdGenerator{0};

    std::vector<DisposablePtr> mDisposables;

    std::map<uint64_t, GAny> mLefts;
    std::map<uint64_t, GAny> mRights;

    std::map<uint64_t, DisposablePtr> mLeftDurations;
    std::map<uint64_t, DisposablePtr> mRightDurations;
};

inline void JoinSupportObserver::onNext(const GAny &value)
{
    if (const auto p = mParent.lock()) {
        p->innerValue(mIsLeft, value);
    }
}

inline void JoinSupportObserver::onError(const GAnyException &e)
{
    if (const auto p = mParent.lock()) {
        p->innerError(e);
    }
}

inline void JoinSupportObserver::onComplete()
{
    if (const auto p = mParent.lock()) {
        p->innerComplete(mIsLeft);
    }
}

inline void JoinDurationObserver::onError(const GAnyException &e)
{
    if (const auto p = mParent.lock()) {
        p->innerError(e);
    }
}

inline void JoinDurationObserver::onComplete()
{
    if (const auto p = mParent.lock()) {
        p->innerClose(mIsLeft, mId);
    }
}

class ObservableJoin : public Observable
{
public:
    ObservableJoin(ObservableSourcePtr source,
                   ObservableSourcePtr other,
                   FlatMapFunction leftDurationSelector,
                   FlatMapFunction rightDurationSelector,
                   BiFunction resultSelector)
        : mSource(std::move(source)),
          mOther(std::move(other)),
          mLeftDurationSelector(std::move(leftDurationSelector)),
          mRightDurationSelector(std::move(rightDurationSelector)),
          mResultSelector(std::move(resultSelector))
    {
        LeakObserver::make<ObservableJoin>();
    }

    ~ObservableJoin() override
    {
        LeakObserver::release<ObservableJoin>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        const auto parent = std::make_shared<JoinMainObserver>(
            observer,
            mLeftDurationSelector,
            mRightDurationSelector,
            mResultSelector
        );

        observer->onSubscribe(parent);
        parent->subscribe(mSource, mOther);
    }

private:
    ObservableSourcePtr mSource;
    ObservableSourcePtr mOther;
    FlatMapFunction mLeftDurationSelector;
    FlatMapFunction mRightDurationSelector;
    BiFunction mResultSelector;
};
} // rx

#endif // RX_OBSERVABLE_JOIN_H
