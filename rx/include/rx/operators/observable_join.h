//
// Created by Gxin on 2026/1/20.
//

#ifndef RX_OBSERVABLE_JOIN_H
#define RX_OBSERVABLE_JOIN_H

#include "../observable.h"
#include "../exception_helper.h"
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
        const auto rightObs = std::make_shared<JoinSupportObserver>(shared_from_this(), false);
        {
            GLockerGuard lock(mGate);
            if (mCancelled.load(std::memory_order_acquire)) {
                return;
            }
            mDisposables.push_back(leftObs);
            mDisposables.push_back(rightObs);
        }

        left->subscribe(leftObs);
        if (!isDisposed()) {
            right->subscribe(rightObs);
        }
    }

    void dispose() override
    {
        std::vector<DisposablePtr> disposables;
        {
            GLockerGuard lock(mGate);
            if (mCancelled.exchange(true)) {
                return;
            }
            collectDisposables(disposables);
        }
        disposeAll(disposables);
    }

    bool isDisposed() const override
    {
        return mCancelled.load(std::memory_order_acquire);
    }

    void innerError(const GAnyException &e)
    {
        ObserverPtr downstream;
        std::vector<DisposablePtr> disposables;
        {
            GLockerGuard lock(mGate);
            if (mCancelled.exchange(true)) {
                return;
            }
            downstream = mDownstream;
            collectDisposables(disposables);
        }
        disposeAll(disposables);
        if (downstream) {
            downstream->onError(e);
        }
    }

    void innerComplete(bool /*isLeft*/)
    {
        if (mActiveCount.fetch_sub(1) == 1) {
            ObserverPtr downstream;
            std::vector<DisposablePtr> disposables;
            {
                GLockerGuard lock(mGate);
                if (mCancelled.exchange(true)) {
                    return;
                }
                downstream = mDownstream;
                collectDisposables(disposables);
            }
            disposeAll(disposables);
            if (downstream) {
                downstream->onComplete();
            }
        }
    }

    void innerValue(bool isLeft, const GAny &value)
    {
        ObservableSourcePtr durationObservable;
        try {
            if (isLeft) {
                durationObservable = mLeftDurationSelector(value);
            } else {
                durationObservable = mRightDurationSelector(value);
            }
        } catch (...) {
            innerError(ExceptionHelper::fromCurrentException("Join: Duration selector failed"));
            return;
        }

        if (!durationObservable) {
            innerError(GAnyException("Join: Duration Selector returned null"));
            return;
        }

        uint64_t id;
        {
            GLockerGuard lock(mGate);
            if (mCancelled.load(std::memory_order_acquire)) {
                return;
            }
            id = mIdGenerator++;
        }
        const auto durationObserver = std::make_shared<JoinDurationObserver>(shared_from_this(), id, isLeft);
        std::vector<GAny> values;
        {
            GLockerGuard lock(mGate);
            if (mCancelled.load(std::memory_order_acquire)) {
                return;
            }
            if (isLeft) {
                mLefts[id] = value;
                mLeftDurations[id] = durationObserver;
                for (const auto &pair: mRights) {
                    values.push_back(pair.second);
                }
            } else {
                mRights[id] = value;
                mRightDurations[id] = durationObserver;
                for (const auto &pair: mLefts) {
                    values.push_back(pair.second);
                }
            }
        }
        durationObservable->subscribe(durationObserver);

        for (const auto &otherValue: values) {
            if (isLeft) {
                emitResult(value, otherValue);
            } else {
                emitResult(otherValue, value);
            }
            if (isDisposed()) {
                return;
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
        } catch (...) {
            innerError(ExceptionHelper::fromCurrentException("Join: Result selector failed"));
            return;
        }
        ObserverPtr downstream;
        {
            GLockerGuard lock(mGate);
            if (mCancelled.load(std::memory_order_acquire)) {
                return;
            }
            downstream = mDownstream;
        }
        if (downstream) {
            downstream->onNext(result);
        }
    }

    void collectDisposables(std::vector<DisposablePtr> &disposables)
    {
        disposables.insert(disposables.end(), mDisposables.begin(), mDisposables.end());
        for (const auto &pair: mLeftDurations) {
            disposables.push_back(pair.second);
        }
        for (const auto &pair: mRightDurations) {
            disposables.push_back(pair.second);
        }
        mDisposables.clear();
        mLeftDurations.clear();
        mRightDurations.clear();
        mLefts.clear();
        mRights.clear();
        mDownstream = nullptr;
    }

    static void disposeAll(const std::vector<DisposablePtr> &disposables)
    {
        for (const auto &disposable: disposables) {
            if (disposable) {
                disposable->dispose();
            }
        }
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
