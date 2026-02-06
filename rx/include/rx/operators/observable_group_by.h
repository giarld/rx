//
// Created by Gxin on 2026/2/6.
//

#ifndef RX_OBSERVABLE_GROUP_BY_H
#define RX_OBSERVABLE_GROUP_BY_H

#include "../observable.h"
#include "../grouped_observable.h"
#include <map>

namespace rx
{
class GroupState;

class GroupByObserver : public Observer, public std::enable_shared_from_this<GroupByObserver>
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

private:
    ObserverPtr mDownstream;
    MapFunction mKeySelector;
    MapFunction mValueSelector;
    DisposablePtr mUpstream;

    struct GAnyCompare
    {
        bool operator()(const GAny &lhs, const GAny &rhs) const
        {
            return lhs.toString() < rhs.toString();
        }
    };

    std::map<GAny, std::shared_ptr<GroupState>, GAnyCompare> mGroups;
};

// Helper to act as the "Subject" for each group
class GroupState
{
public:
    GroupState(GAny key, std::shared_ptr<GroupByObserver> parent)
        : mKey(std::move(key)), mParent(std::move(parent))
    {
        mObservable = Observable::create([this](const ObservableEmitterPtr &emitter) {
            GLockerGuard lock(mLock);
            mEmitters.push_back(emitter);
        });
    }

    std::shared_ptr<Observable> getObservable() { return mObservable; }

    void onNext(const GAny &value)
    {
        GLockerGuard lock(mLock);
        auto it = mEmitters.begin();
        while (it != mEmitters.end()) {
            if ((*it)->isDisposed()) {
                it = mEmitters.erase(it);
            } else {
                (*it)->onNext(value);
                ++it;
            }
        }
    }

    void onError(const GAnyException &e)
    {
        GLockerGuard lock(mLock);
        for (const auto &emitter: mEmitters) {
            if (!emitter->isDisposed())
                emitter->onError(e);
        }
        mEmitters.clear();
    }

    void onComplete()
    {
        GLockerGuard lock(mLock);
        for (const auto &emitter: mEmitters) {
            if (!emitter->isDisposed())
                emitter->onComplete();
        }
        mEmitters.clear();
    }

private:
    GAny mKey;
    std::weak_ptr<GroupByObserver> mParent;
    std::shared_ptr<Observable> mObservable;
    std::vector<ObservableEmitterPtr> mEmitters;
    GMutex mLock;
};

// Implementation of GroupByObserver methods

inline void GroupByObserver::onSubscribe(const DisposablePtr &d)
{
    if (mDownstream)
        mDownstream->onSubscribe(d);
    mUpstream = d;
}

inline void GroupByObserver::onNext(const GAny &t)
{
    GAny key;
    try {
        key = mKeySelector(t);
    } catch (...) {
        onError(GAnyException("GroupBy: KeySelector threw exception"));
        if (mUpstream)
            mUpstream->dispose();
        return;
    }

    std::shared_ptr<GroupState> groupState;
    bool isNew = false; {
        const auto it = mGroups.find(key);
        if (it != mGroups.end()) {
            groupState = it->second;
        } else {
            isNew = true;
            groupState = std::make_shared<GroupState>(key, this->shared_from_this());
            mGroups[key] = groupState;
        }
    }

    if (isNew) {
        const auto groupedObservable = std::make_shared<GroupedObservable>(key, groupState->getObservable());
        mDownstream->onNext(groupedObservable);
    }

    GAny value = t;
    if (mValueSelector) {
        try {
            value = mValueSelector(t);
        } catch (...) {
            onError(GAnyException("GroupBy: ValueSelector threw exception"));
            if (mUpstream)
                mUpstream->dispose();
            return;
        }
    }

    groupState->onNext(value);
}

inline void GroupByObserver::onError(const GAnyException &e)
{
    for (const auto &pair: mGroups) {
        pair.second->onError(e);
    }
    mGroups.clear();
    if (mDownstream)
        mDownstream->onError(e);
}

inline void GroupByObserver::onComplete()
{
    for (const auto &pair: mGroups) {
        pair.second->onComplete();
    }
    mGroups.clear();
    if (mDownstream)
        mDownstream->onComplete();
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
