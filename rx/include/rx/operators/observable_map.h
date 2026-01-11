//
// Created by Gxin on 2026/1/7.
//

#ifndef RX_OBSERVABLE_MAP_H
#define RX_OBSERVABLE_MAP_H

#include "../observable.h"


namespace rx
{
class MapObserver : public Observer, public Disposable, public std::enable_shared_from_this<MapObserver>
{
public:
    explicit MapObserver(const ObserverPtr &observer, const MapFunction &function)
        : mDownstream(observer), mFunction(function)
    {
    }

    ~MapObserver() override = default;

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        if (Disposable::validate(mUpstream.get(), d.get())) {
            mUpstream = d;
            mDownstream->onSubscribe(this->shared_from_this());
        }
    }

    void onNext(const GAny &value) override
    {
        if (mDown.load()) {
            return;
        }
        GAny r;
        try {
            r = mFunction(value);
        } catch (const GAnyException &e) {
            mUpstream->dispose();
            onError(e);
            return;
        }
        mDownstream->onNext(r);
    }

    void onError(const GAnyException &e) override
    {
        if (mDown.load()) {
            return;
        }
        mDown.store(true, std::memory_order_release);
        mDownstream->onError(e);

        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    void onComplete() override
    {
        if (mDown.load()) {
            return;
        }
        mDown.store(true, std::memory_order_release);
        mDownstream->onComplete();

        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    void dispose() override
    {
        if (mUpstream) {
            mUpstream->dispose();
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
    std::atomic<bool> mDown = false;
};

class ObservableMap : public Observable
{
public:
    explicit ObservableMap(ObservableSourcePtr source, const MapFunction &function)
        : mSource(std::move(source)), mMapFunction(function)
    {
    }

    ~ObservableMap() override = default;

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
