//
// Created by Gxin on 2026/1/7.
//

#ifndef RX_OBSERVABLE_MAP_H
#define RX_OBSERVABLE_MAP_H

#include "../observable.h"


namespace rx
{
class MapObserver : public Observer, public AtomicDisposable
{
public:
    explicit MapObserver(const ObserverPtr &observer, const MapFunction &function)
        : mObserver(observer), mFunction(function)
    {
    }

    ~MapObserver() override = default;

public:
    void onSubscribe(const DisposablePtr &d) override
    {

    }

    void onNext(const GAny &value) override
    {
        if (!isDisposed()) {
            GAny r;
            try {
                r = mFunction(value);
            } catch (GAnyException e) {
                onError(e);
                return;
            }
            mObserver->onNext(r);
        }
    }

    void onError(const GAnyException &e) override
    {
        if (!isDisposed()) {
            try {
                mObserver->onError(e);
            } catch (GAnyException _e) {
            }
            dispose();
        }
    }

    void onComplete() override
    {
        if (!isDisposed()) {
            try {
                mObserver->onComplete();
            } catch (GAnyException _e) {
            }
            dispose();
        }
    }

private:
    ObserverPtr mObserver;
    MapFunction mFunction;
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
