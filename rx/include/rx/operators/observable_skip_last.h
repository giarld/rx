//
// Created by Gxin on 2026/1/15.
//

#ifndef RX_OBSERVABLE_SKIP_LAST_H
#define RX_OBSERVABLE_SKIP_LAST_H

#include "../observable.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"

#include <deque>


namespace rx
{
class SkipLastObserver : public Observer, public Disposable, public std::enable_shared_from_this<SkipLastObserver>
{
public:
    explicit SkipLastObserver(const ObserverPtr &observer, uint64_t skip)
        : mDownstream(observer), mSkip(skip)
    {
        LeakObserver::make<SkipLastObserver>();
    }

    ~SkipLastObserver() override
    {
        LeakObserver::release<SkipLastObserver>();
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
        if (mSkip == mBuffer.size()) {
            mDownstream->onNext(mBuffer.front());
            mBuffer.pop_front();
        }
        mBuffer.push_back(value);
    }

    void onError(const GAnyException &e) override
    {
        mBuffer.clear();
        mDownstream->onError(e);

        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    void onComplete() override
    {
        mBuffer.clear();
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
        mBuffer.clear();
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
    uint64_t mSkip;
    std::deque<GAny> mBuffer;
};

class ObservableSkipLast : public Observable
{
public:
    explicit ObservableSkipLast(ObservableSourcePtr source, uint64_t skip)
        : mSource(std::move(source)), mSkip(skip)
    {
        LeakObserver::make<ObservableSkipLast>();
    }

    ~ObservableSkipLast() override
    {
        LeakObserver::release<ObservableSkipLast>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<SkipLastObserver>(observer, mSkip));
    }

private:
    ObservableSourcePtr mSource;
    uint64_t mSkip;
};
} // rx

#endif //RX_OBSERVABLE_SKIP_LAST_H
