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
        if (mDown) {
            return;
        }
        GAny r;
        try {
            r = mFunction(value);
        } catch (GAnyException e) {
            onError(e);
            return;
        }
        mDownstream->onNext(r);
    }

    void onError(const GAnyException &e) override
    {
        if (mDown) {
            return;
        }
        mDown = true;
        mDownstream->onError(e);

        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    void onComplete() override
    {
        if (mDown) {
            return;
        }
        mDown = true;
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
        if (mUpstream) {
            return mUpstream->isDisposed();
        }
        return false;
    }

private:
    ObserverPtr mDownstream;
    MapFunction mFunction;
    DisposablePtr mUpstream;
    bool mDown = false;
};

class ObservableMap : public Observable
{
public:
    explicit ObservableMap(ObservableSourcePtr source, MapFunction function)
        : mSource(std::move(source)), mMapFunction(std::move(function))
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
