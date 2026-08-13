//
// Created by Gxin on 2026/2/6.
//

#ifndef RX_OBSERVABLE_TAKE_WHILE_H
#define RX_OBSERVABLE_TAKE_WHILE_H

#include "../observable.h"
#include "../exception_helper.h"
#include "../leak_observer.h"


namespace rx
{
class TakeWhileObserver : public Observer
{
public:
    TakeWhileObserver(ObserverPtr downstream, FilterFunction predicate)
        : mDownstream(std::move(downstream)), mPredicate(std::move(predicate))
    {
    }

    void onSubscribe(const DisposablePtr &d) override
    {
        mDownstream->onSubscribe(d);
        mUpstream = d;
    }

    void onNext(const GAny &value) override
    {
        if (mDone) {
            return;
        }
        bool result = false;
        try {
            result = mPredicate(value);
        } catch (...) {
            mDone = true;
            mDownstream->onError(ExceptionHelper::fromCurrentException("TakeWhile: Predicate failed"));
            if (mUpstream)
                mUpstream->dispose();
            return;
        }

        if (result) {
            mDownstream->onNext(value);
        } else {
            mDone = true;
            mDownstream->onComplete();
            if (mUpstream)
                mUpstream->dispose();
        }
    }

    void onError(const GAnyException &e) override
    {
        if (mDone) {
            return;
        }
        mDone = true;
        mDownstream->onError(e);
    }

    void onComplete() override
    {
        if (mDone) {
            return;
        }
        mDone = true;
        mDownstream->onComplete();
    }

private:
    ObserverPtr mDownstream;
    FilterFunction mPredicate;
    DisposablePtr mUpstream;
    bool mDone = false;
};

class ObservableTakeWhile : public Observable
{
public:
    ObservableTakeWhile(std::shared_ptr<Observable> source, FilterFunction predicate)
        : mSource(std::move(source)), mPredicate(std::move(predicate))
    {
        LeakObserver::make<ObservableTakeWhile>();
    }

    ~ObservableTakeWhile() override
    {
        LeakObserver::release<ObservableTakeWhile>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<TakeWhileObserver>(observer, mPredicate));
    }

private:
    std::shared_ptr<Observable> mSource;
    FilterFunction mPredicate;
};
} // rx

#endif //RX_OBSERVABLE_TAKE_WHILE_H
