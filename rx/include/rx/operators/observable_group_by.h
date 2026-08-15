//
// Created by Gxin on 2026/2/6.
//

#ifndef RX_OBSERVABLE_GROUP_BY_H
#define RX_OBSERVABLE_GROUP_BY_H

#include "../observable.h"
#include "../disposables/disposable_helper.h"
#include "../exception_helper.h"
#include "../grouped_observable.h"
#include <optional>
#include <utility>
#include <vector>

namespace rx
{
class GroupState;

class GroupByObserver : public Observer, public Disposable, public std::enable_shared_from_this<GroupByObserver>
{
public:
    GroupByObserver(ObserverPtr downstream, MapFunction keySelector, MapFunction valueSelector)
        : mDownstream(std::move(downstream)), mKeySelector(std::move(keySelector)), mValueSelector(std::move(valueSelector))
    {
        LeakObserver::make<GroupByObserver>();
    }

    ~GroupByObserver() override
    {
        LeakObserver::release<GroupByObserver>();
    }

public:
    void onSubscribe(const DisposablePtr &d) override;

    void onNext(const GAny &t) override;

    void onError(const GAnyException &e) override;

    void onComplete() override;

    void dispose() override;

    bool isDisposed() const override;

private:
    ObserverPtr mDownstream;
    MapFunction mKeySelector;
    MapFunction mValueSelector;
    DisposablePtr mUpstream;
    std::atomic<bool> mDone{false};

    std::vector<std::pair<GAny, std::shared_ptr<GroupState> > > mGroups;
};

// Helper to act as the "Subject" for each group
class GroupState
{
public:
    GroupState(GAny key, std::shared_ptr<GroupByObserver> parent)
        : mKey(std::move(key)), mParent(std::move(parent)), mState(std::make_shared<State>())
    {
        const auto state = mState;
        mObservable = Observable::create([state](const ObservableEmitterPtr &emitter) {
            std::optional<GAnyException> error;
            bool completed = false;
            {
                GLockerGuard lock(state->lock);
                error = state->error;
                completed = state->completed;
                if (!error && !completed) {
                    state->emitters.push_back(emitter);
                    return;
                }
            }
            if (error) {
                emitter->onError(error.value());
            } else {
                emitter->onComplete();
            }
        });
    }

    std::shared_ptr<Observable> getObservable() { return mObservable; }

    void onNext(const GAny &value)
    {
        std::vector<ObservableEmitterPtr> emitters;
        {
            GLockerGuard lock(mState->lock);
            auto it = mState->emitters.begin();
            while (it != mState->emitters.end()) {
                if ((*it)->isDisposed()) {
                    it = mState->emitters.erase(it);
                } else {
                    emitters.push_back(*it);
                    ++it;
                }
            }
        }
        for (const auto &emitter: emitters) {
            emitter->onNext(value);
        }
    }

    void onError(const GAnyException &e)
    {
        std::vector<ObservableEmitterPtr> emitters;
        {
            GLockerGuard lock(mState->lock);
            mState->error = e;
            emitters = std::move(mState->emitters);
            mState->emitters.clear();
        }
        for (const auto &emitter: emitters) {
            if (!emitter->isDisposed()) {
                emitter->onError(e);
            }
        }
    }

    void onComplete()
    {
        std::vector<ObservableEmitterPtr> emitters;
        {
            GLockerGuard lock(mState->lock);
            mState->completed = true;
            emitters = std::move(mState->emitters);
            mState->emitters.clear();
        }
        for (const auto &emitter: emitters) {
            if (!emitter->isDisposed()) {
                emitter->onComplete();
            }
        }
    }

private:
    struct State
    {
        std::vector<ObservableEmitterPtr> emitters;
        std::optional<GAnyException> error;
        bool completed = false;
        GMutex lock;
    };

    GAny mKey;
    std::weak_ptr<GroupByObserver> mParent;
    std::shared_ptr<Observable> mObservable;
    std::shared_ptr<State> mState;
};

// Implementation of GroupByObserver methods

inline void GroupByObserver::onSubscribe(const DisposablePtr &d)
{
    if (DisposableHelper::validate(mUpstream, d)) {
        mUpstream = d;
        if (const auto downstream = mDownstream) {
            downstream->onSubscribe(shared_from_this());
        }
    }
}

inline void GroupByObserver::onNext(const GAny &t)
{
    if (mDone.load(std::memory_order_acquire)) {
        return;
    }

    GAny key;
    try {
        key = mKeySelector(t);
    } catch (...) {
        if (mUpstream)
            mUpstream->dispose();
        onError(ExceptionHelper::fromCurrentException("GroupBy: Key selector failed"));
        return;
    }

    std::shared_ptr<GroupState> groupState;
    bool isNew = false;
    try {
        for (const auto &group: mGroups) {
            if (group.first.typeInfo() == key.typeInfo() && group.first == key) {
                groupState = group.second;
                break;
            }
        }
        if (!groupState) {
            isNew = true;
            groupState = std::make_shared<GroupState>(key, this->shared_from_this());
            mGroups.emplace_back(key, groupState);
        }
    } catch (...) {
        if (mUpstream) {
            mUpstream->dispose();
        }
        onError(ExceptionHelper::fromCurrentException("GroupBy: Key comparison failed"));
        return;
    }

    if (isNew) {
        const auto groupedObservable = std::make_shared<GroupedObservable>(key, groupState->getObservable());
        mDownstream->onNext(groupedObservable);
        if (mDone.load(std::memory_order_acquire)) {
            return;
        }
    }

    GAny value = t;
    if (mValueSelector) {
        try {
            value = mValueSelector(t);
        } catch (...) {
            if (mUpstream)
                mUpstream->dispose();
            onError(ExceptionHelper::fromCurrentException("GroupBy: Value selector failed"));
            return;
        }
    }

    groupState->onNext(value);
}

inline void GroupByObserver::onError(const GAnyException &e)
{
    if (mDone.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    for (const auto &group: mGroups) {
        group.second->onError(e);
    }
    mGroups.clear();
    if (mDownstream)
        mDownstream->onError(e);
    mDownstream = nullptr;
    mUpstream = nullptr;
}

inline void GroupByObserver::onComplete()
{
    if (mDone.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    for (const auto &group: mGroups) {
        group.second->onComplete();
    }
    mGroups.clear();
    if (mDownstream)
        mDownstream->onComplete();
    mDownstream = nullptr;
    mUpstream = nullptr;
}

inline void GroupByObserver::dispose()
{
    if (mDone.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    if (mUpstream) {
        mUpstream->dispose();
    }
    for (const auto &group: mGroups) {
        group.second->onComplete();
    }
    mGroups.clear();
    mDownstream = nullptr;
    mUpstream = nullptr;
}

inline bool GroupByObserver::isDisposed() const
{
    return mDone.load(std::memory_order_acquire);
}

class ObservableGroupBy : public Observable
{
public:
    ObservableGroupBy(std::shared_ptr<Observable> source, MapFunction keySelector, MapFunction valueSelector = nullptr)
        : mSource(std::move(source)), mKeySelector(std::move(keySelector)), mValueSelector(std::move(valueSelector))
    {
        LeakObserver::make<ObservableGroupBy>();
    }

    ~ObservableGroupBy() override
    {
        LeakObserver::release<ObservableGroupBy>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<GroupByObserver>(observer, mKeySelector, mValueSelector));
    }

private:
    std::shared_ptr<Observable> mSource;
    MapFunction mKeySelector;
    MapFunction mValueSelector;
};
} // rx

#endif //RX_OBSERVABLE_GROUP_BY_H
