//
// Created by Gxin on 2026/2/6.
//

#ifndef RX_OBSERVABLE_SKIP_WHILE_H
#define RX_OBSERVABLE_SKIP_WHILE_H

#include "../observable.h"
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
        mDownstream->onSubscribe(d);
    }

    void onNext(const GAny &value) override
    {
        if (mSkipping) {
            bool result = false;
            try {
                result = mPredicate(value);
            } catch (...) {
                mDownstream->onError(GAnyException("SkipWhile: Predicate threw exception"));
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
        mDownstream->onError(e);
    }

    void onComplete() override
    {
        mDownstream->onComplete();
    }

private:
    ObserverPtr mDownstream;
    FilterFunction mPredicate;
    bool mSkipping = true;
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
