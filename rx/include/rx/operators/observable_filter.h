//
// Created by Gxin on 2026/1/12.
//

#ifndef RX_OBSERVABLE_FILTER_H
#define RX_OBSERVABLE_FILTER_H

#include "../observable.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"


namespace rx
{
class FilterObserver : public Observer, public Disposable, public std::enable_shared_from_this<FilterObserver>
{
public:
    explicit FilterObserver(const ObserverPtr &observer, const FilterFunction &filter)
        : mFilter(filter), mDownstream(observer)
    {
        LeakObserver::make<FilterObserver>();
    }

    ~FilterObserver() override
    {
        LeakObserver::release<FilterObserver>();
    }

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        if (DisposableHelper::validate(mUpstream, d)) {
            mUpstream = d;
            if (const auto ds = mDownstream) {
                ds->onSubscribe(this->shared_from_this());
            }
        }
    }

    void onNext(const GAny &value) override
    {
        if (mDone.load(std::memory_order_acquire)) {
            return;
        }
        bool b;
        try {
            b = mFilter(value);
        } catch (const GAnyException &e) {
            mUpstream->dispose();
            onError(e);
            return;
        }
        if (b) {
            if (const auto d = mDownstream) {
                d->onNext(value);
            }
        }
    }

    void onError(const GAnyException &e) override
    {
        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        if (const auto d = mDownstream) {
            d->onError(e);
        }

        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    void onComplete() override
    {
        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        if (const auto d = mDownstream) {
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
    FilterFunction mFilter;
    ObserverPtr mDownstream;
    DisposablePtr mUpstream;
    std::atomic<bool> mDone = false;
    GMutex mLock;
};

class ObservableFilter : public Observable
{
public:
    explicit ObservableFilter(ObservableSourcePtr source, const FilterFunction &filter)
        : mSource(std::move(source)), mFilter(filter)
    {
        LeakObserver::make<ObservableFilter>();
    }

    ~ObservableFilter() override
    {
        LeakObserver::release<ObservableFilter>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<FilterObserver>(observer, mFilter));
    }

private:
    ObservableSourcePtr mSource;
    FilterFunction mFilter;
};
} // rx

#endif //RX_OBSERVABLE_FILTER_H
