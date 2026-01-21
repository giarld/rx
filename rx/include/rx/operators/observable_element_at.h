//
// Created by Gxin on 2026/1/13.
//

#ifndef RX_OBSERVABLE_ELEMENT_AT_H
#define RX_OBSERVABLE_ELEMENT_AT_H

#include "../observable.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"


namespace rx
{
class ElementAtObserver : public Observer, public Disposable, public std::enable_shared_from_this<ElementAtObserver>
{
public:
    explicit ElementAtObserver(const ObserverPtr &observer, uint64_t index, const GAny &defaultValue, bool hasDefault)
        : mDownstream(observer), mIndex(index), mDefaultValue(defaultValue), mHasDefault(hasDefault), mCount(0)
    {
        LeakObserver::make<ElementAtObserver>();
    }

    ~ElementAtObserver() override
    {
        LeakObserver::release<ElementAtObserver>();
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

        const uint64_t currentCount = mCount;
        if (currentCount == mIndex) {
            mDone.store(true, std::memory_order_release);
            mUpstream->dispose();
            if (const auto d = mDownstream) {
                d->onNext(value);
                d->onComplete();
            }

            mDownstream = nullptr;
            mUpstream = nullptr;
        } else {
            mCount = currentCount + 1;
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
            if (mHasDefault) {
                d->onNext(mDefaultValue);
                d->onComplete();
            } else {
                d->onError(GAnyException("Index out of bounds"));
            }
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
    uint64_t mIndex;
    GAny mDefaultValue;
    bool mHasDefault;
    uint64_t mCount;
    std::atomic<bool> mDone = false;
};

class ObservableElementAt : public Observable
{
public:
    explicit ObservableElementAt(ObservableSourcePtr source, uint64_t index, const GAny &defaultValue, bool hasDefault)
        : mSource(std::move(source)), mIndex(index), mDefaultValue(defaultValue), mHasDefault(hasDefault)
    {
        LeakObserver::make<ObservableElementAt>();
    }

    ~ObservableElementAt() override
    {
        LeakObserver::release<ObservableElementAt>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<ElementAtObserver>(observer, mIndex, mDefaultValue, mHasDefault));
    }

private:
    ObservableSourcePtr mSource;
    uint64_t mIndex;
    GAny mDefaultValue;
    bool mHasDefault;
};
} // rx

#endif //RX_OBSERVABLE_ELEMENT_AT_H
