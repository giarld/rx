//
// Created by Gxin on 2026/1/15.
//

#ifndef RX_OBSERVABLE_TAKE_H
#define RX_OBSERVABLE_TAKE_H

#include "../observable.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"


namespace rx
{
class TakeObserver : public Observer, public Disposable, public std::enable_shared_from_this<TakeObserver>
{
public:
    explicit TakeObserver(const ObserverPtr &observer, uint64_t limit)
        : mDownstream(observer), mRemaining(limit)
    {
        LeakObserver::make<TakeObserver>();
    }

    ~TakeObserver() override
    {
        LeakObserver::release<TakeObserver>();
    }

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        if (DisposableHelper::validate(mUpstream, d)) {
            if (const auto ds = mDownstream) {
                mUpstream = d;
                if (mRemaining == 0) {
                    mDone = true;
                    d->dispose();
                    EmptyDisposable::complete(ds.get());

                    mDownstream = nullptr;
                    mUpstream = nullptr;
                } else {
                    ds->onSubscribe(this->shared_from_this());
                }
            }
        }
    }

    void onNext(const GAny &value) override
    {
        if (!mDone && mRemaining-- > 0) {
            const bool stop = mRemaining == 0;
            mDownstream->onNext(value);
            if (stop) {
                onComplete();
            }
        }
    }

    void onError(const GAnyException &e) override
    {
        if (mDone) {
            return;
        }
        
        mDone = true;
        mUpstream->dispose();
        mDownstream->onError(e);

        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    void onComplete() override
    {
        if (!mDone) {
            mDone = true;
            mUpstream->dispose();
            mDownstream->onComplete();

            mDownstream = nullptr;
            mUpstream = nullptr;
        }
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
    uint64_t mRemaining;
    bool mDone = false;
};

class ObservableTake : public Observable
{
public:
    explicit ObservableTake(ObservableSourcePtr source, uint64_t count)
        : mSource(std::move(source)), mCount(count)
    {
        LeakObserver::make<ObservableTake>();
    }

    ~ObservableTake() override
    {
        LeakObserver::release<ObservableTake>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<TakeObserver>(observer, mCount));
    }

private:
    ObservableSourcePtr mSource;
    uint64_t mCount;
};
} // rx

#endif //RX_OBSERVABLE_TAKE_H
