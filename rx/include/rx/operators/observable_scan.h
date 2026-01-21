//
// Created by Gxin on 2026/1/13.
//

#ifndef RX_OBSERVABLE_SCAN_H
#define RX_OBSERVABLE_SCAN_H

#include "../observable.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"


namespace rx
{
class ScanObserver : public Observer, public Disposable, public std::enable_shared_from_this<ScanObserver>
{
public:
    explicit ScanObserver(const ObserverPtr &observer, const BiFunction &accumulator)
        : mDownstream(observer),
          mAccumulator(accumulator)
    {
        LeakObserver::make<ScanObserver>();
    }

    ~ScanObserver() override
    {
        LeakObserver::release<ScanObserver>();
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

    void onNext(const GAny &t) override
    {
        if (mDone.load(std::memory_order_acquire)) {
            return;
        }
        if (const auto a = mDownstream) {
            const GAny v = mValue;
            if (v == nullptr) {
                mValue = t;
                a->onNext(t);
            } else {
                GAny u;
                try {
                    u = mAccumulator(v, t);
                } catch (const std::exception &e) {
                    if (const auto up = mUpstream) {
                        up->dispose();
                    }
                    onError(GAnyException(e.what()));
                    return;
                }
                mValue = u;
                a->onNext(u);
            }
        }
    }

    void onError(const GAnyException &e) override
    {
        if (mDone.load(std::memory_order_acquire)) {
            return;
        }
        mDone.store(true, std::memory_order_release);
        if (const auto d = mDownstream) {
            d->onError(e);
        }

        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    void onComplete() override
    {
        if (mDone.load(std::memory_order_acquire)) {
            return;
        }
        mDone.store(true, std::memory_order_release);
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
    BiFunction mAccumulator;
    DisposablePtr mUpstream;
    std::atomic<bool> mDone = false;
    GAny mValue;
};

class ObservableScan : public Observable
{
public:
    explicit ObservableScan(ObservableSourcePtr source, const BiFunction &accumulator)
        : mSource(std::move(source)),
          mAccumulator(accumulator)
    {
        LeakObserver::make<ObservableScan>();
    }

    ~ObservableScan() override
    {
        LeakObserver::release<ObservableScan>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<ScanObserver>(observer, mAccumulator));
    }

private:
    ObservableSourcePtr mSource;
    BiFunction mAccumulator;
};
} // rx

#endif //RX_OBSERVABLE_SCAN_H
