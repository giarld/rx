//
// Created by Gxin on 2026/1/15.
//

#ifndef RX_OBSERVABLE_SKIP_H
#define RX_OBSERVABLE_SKIP_H

#include "../observable.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"


namespace rx
{
class SkipObserver : public Observer, public Disposable, public std::enable_shared_from_this<SkipObserver>
{
public:
    explicit SkipObserver(const ObserverPtr &observer, uint64_t count)
        : mDownstream(observer), mSkipCount(count), mRemaining(count)
    {
        LeakObserver::make<SkipObserver>();
    }

    ~SkipObserver() override
    {
        LeakObserver::release<SkipObserver>();
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
        if (mRemaining > 0) {
            mRemaining--;
        } else {
            mDownstream->onNext(value);
        }
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
    uint64_t mSkipCount;
    uint64_t mRemaining;
};

class ObservableSkip : public Observable
{
public:
    explicit ObservableSkip(ObservableSourcePtr source, uint64_t count)
        : mSource(std::move(source)), mCount(count)
    {
        LeakObserver::make<ObservableSkip>();
    }

    ~ObservableSkip() override
    {
        LeakObserver::release<ObservableSkip>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<SkipObserver>(observer, mCount));
    }

private:
    ObservableSourcePtr mSource;
    uint64_t mCount;
};
} // rx

#endif //RX_OBSERVABLE_SKIP_H
