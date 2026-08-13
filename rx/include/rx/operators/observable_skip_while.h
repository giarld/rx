//
// Created by Gxin on 2026/2/6.
//

#ifndef RX_OBSERVABLE_SKIP_WHILE_H
#define RX_OBSERVABLE_SKIP_WHILE_H

#include "../observable.h"
#include "../exception_helper.h"
#include "../leak_observer.h"


namespace rx
{
class SkipWhileObserver : public Observer
{
public:
    SkipWhileObserver(ObserverPtr downstream, FilterFunction predicate)
        : mDownstream(std::move(downstream)), mPredicate(std::move(predicate))
    {
        LeakObserver::make<SkipWhileObserver>();
    }

    ~SkipWhileObserver() override
    {
        LeakObserver::release<SkipWhileObserver>();
    }

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        mUpstream = d;
        mDownstream->onSubscribe(d);
    }

    void onNext(const GAny &value) override
    {
        if (mDone) {
            return;
        }
        if (mSkipping) {
            bool result = false;
            try {
                result = mPredicate(value);
            } catch (...) {
                mDone = true;
                mUpstream->dispose();
                mDownstream->onError(ExceptionHelper::fromCurrentException("SkipWhile: Predicate failed"));
                return;
            }

            if (!result) {
                mSkipping = false;
                mDownstream->onNext(value);
            }
        } else {
            mDownstream->onNext(value);
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
    bool mSkipping = true;
    bool mDone = false;
};

class ObservableSkipWhile : public Observable
{
public:
    ObservableSkipWhile(std::shared_ptr<Observable> source, FilterFunction predicate)
        : mSource(std::move(source)), mPredicate(std::move(predicate))
    {
        LeakObserver::make<ObservableSkipWhile>();
    }

    ~ObservableSkipWhile() override
    {
        LeakObserver::release<ObservableSkipWhile>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<SkipWhileObserver>(observer, mPredicate));
    }

private:
    std::shared_ptr<Observable> mSource;
    FilterFunction mPredicate;
};
} // rx

#endif //RX_OBSERVABLE_SKIP_WHILE_H
