//
// Created by Gxin on 2026/2/5.
//

#ifndef RX_OBSERVABLE_TO_ARRAY_H
#define RX_OBSERVABLE_TO_ARRAY_H

#include "../observable.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"


namespace rx
{
class ToArrayObserver : public Observer, public Disposable, public std::enable_shared_from_this<ToArrayObserver>
{
public:
    explicit ToArrayObserver(const ObserverPtr &observer)
        : mDownstream(observer)
    {
        LeakObserver::make<ToArrayObserver>();
    }

    ~ToArrayObserver() override
    {
        LeakObserver::release<ToArrayObserver>();
    }

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        if (DisposableHelper::validate(mUpstream, d)) {
            if (const auto ds = mDownstream) {
                mUpstream = d;
                ds->onSubscribe(this->shared_from_this());
            }
        }
    }

    void onNext(const GAny &value) override
    {
        mList.push_back(value);
    }

    void onError(const GAnyException &e) override
    {
        mList.clear();
        if (const auto d = mDownstream) {
            d->onError(e);
        }

        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    void onComplete() override
    {
        if (const auto d = mDownstream) {
            d->onNext(mList);
            mList.clear();
            d->onComplete();
        }

        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    void dispose() override
    {
        if (const auto d = mUpstream) {
            d->dispose();
            mUpstream = nullptr;
        }
        mDownstream = nullptr;
    }

    bool isDisposed() const override
    {
        if (const auto d = mUpstream) {
            return d->isDisposed();
        }
        return true;
    }

private:
    ObserverPtr mDownstream;
    DisposablePtr mUpstream;
    std::vector<GAny> mList;
};

class ObservableToArray : public Observable
{
public:
    explicit ObservableToArray(ObservableSourcePtr source)
        : mSource(std::move(source))
    {
        LeakObserver::make<ObservableToArray>();
    }

    ~ObservableToArray() override
    {
        LeakObserver::release<ObservableToArray>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<ToArrayObserver>(observer));
    }

private:
    ObservableSourcePtr mSource;
};
} // rx

#endif //RX_OBSERVABLE_TO_ARRAY_H
