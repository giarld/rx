//
// Created by Gxin on 2026/1/13.
//

#ifndef RX_OBSERVABLE_IGNORE_ELEMENTS_H
#define RX_OBSERVABLE_IGNORE_ELEMENTS_H

#include "../observable.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"


namespace rx
{
class IgnoreElementsObserver : public Observer, public Disposable, public std::enable_shared_from_this<IgnoreElementsObserver>
{
public:
    explicit IgnoreElementsObserver(const ObserverPtr &observer)
        : mDownstream(observer)
    {
        LeakObserver::make<IgnoreElementsObserver>();
    }

    ~IgnoreElementsObserver() override
    {
        LeakObserver::release<IgnoreElementsObserver>();
    }

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        if (DisposableHelper::validate(mUpstream, d)) {
            mUpstream = d;
            mDownstream->onSubscribe(this->shared_from_this());
        }
    }

    void onNext(const GAny &value) override
    {
        // deliberately ignored
    }

    void onError(const GAnyException &e) override
    {
        mDownstream->onError(e);

        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    void onComplete() override
    {
        mDownstream->onComplete();

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
};

class ObservableIgnoreElements : public Observable
{
public:
    explicit ObservableIgnoreElements(ObservableSourcePtr source)
        : mSource(std::move(source))
    {
        LeakObserver::make<ObservableIgnoreElements>();
    }

    ~ObservableIgnoreElements() override
    {
        LeakObserver::release<ObservableIgnoreElements>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<IgnoreElementsObserver>(observer));
    }

private:
    ObservableSourcePtr mSource;
};
} // rx

#endif //RX_OBSERVABLE_IGNORE_ELEMENTS_H
