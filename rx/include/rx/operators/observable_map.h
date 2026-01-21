//
// Created by Gxin on 2026/1/7.
//

#ifndef RX_OBSERVABLE_MAP_H
#define RX_OBSERVABLE_MAP_H

#include "../observable.h"
#include "../leak_observer.h"


namespace rx
{
class MapObserver : public Observer, public Disposable, public std::enable_shared_from_this<MapObserver>
{
public:
    explicit MapObserver(const ObserverPtr &observer, const MapFunction &function)
        : mDownstream(observer), mFunction(function)
    {
        LeakObserver::make<MapObserver>();
    }

    ~MapObserver() override
    {
        LeakObserver::release<MapObserver>();
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
        if (mDone.load(std::memory_order_acquire)) {
            return;
        }
        if (const auto d = mDownstream) {
            GAny r;
            try {
                r = mFunction(value);
            } catch (const GAnyException &e) {
                if (const auto u = mUpstream) {
                    u->dispose();
                }
                onError(e);
                return;
            }
            d->onNext(r);
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
    ObserverPtr mDownstream;
    MapFunction mFunction;
    DisposablePtr mUpstream;
    std::atomic<bool> mDone = false;
};

class ObservableMap : public Observable
{
public:
    explicit ObservableMap(ObservableSourcePtr source, const MapFunction &function)
        : mSource(std::move(source)), mMapFunction(function)
    {
        LeakObserver::make<ObservableMap>();
    }

    ~ObservableMap() override
    {
        LeakObserver::release<ObservableMap>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<MapObserver>(observer, mMapFunction));
    }

private:
    ObservableSourcePtr mSource;
    MapFunction mMapFunction;
};
} // rx

#endif //RX_OBSERVABLE_MAP_H
