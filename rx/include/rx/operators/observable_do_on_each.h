//
// Created by Gxin on 2026/2/4.
//

#ifndef RX_OBSERVABLE_DO_ON_EACH_H
#define RX_OBSERVABLE_DO_ON_EACH_H

#include "../observable.h"
#include "../exception_helper.h"
#include "../leak_observer.h"


namespace rx
{
class DoOnEachObserver : public Observer, public Disposable, public std::enable_shared_from_this<DoOnEachObserver>
{
public:
    DoOnEachObserver(const ObserverPtr &downstream,
                     OnNextAction onNext,
                     OnErrorAction onError,
                     OnCompleteAction onComplete,
                     OnSubscribeAction onSubscribe,
                     OnCompleteAction onFinally)
        : mDownstream(downstream),
          mOnNext(std::move(onNext)),
          mOnError(std::move(onError)),
          mOnComplete(std::move(onComplete)),
          mOnSubscribe(std::move(onSubscribe)),
          mOnFinally(std::move(onFinally))
    {
        LeakObserver::make<DoOnEachObserver>();
    }

    ~DoOnEachObserver() override
    {
        LeakObserver::release<DoOnEachObserver>();
    }

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        mUpstream = d;
        if (const auto downstream = mDownstream) {
            downstream->onSubscribe(shared_from_this());
        }
        if (mOnSubscribe) {
            try {
                mOnSubscribe(d);
            } catch (...) {
                const auto error = ExceptionHelper::fromCurrentException("DoOnEach: onSubscribe failed");
                onError(error);
                d->dispose();
                return;
            }
        }
    }

    void onNext(const GAny &value) override
    {
        if (isDisposed())
            return;

        if (mOnNext) {
            try {
                mOnNext(value);
            } catch (...) {
                const auto error = ExceptionHelper::fromCurrentException("DoOnEach: onNext failed");
                const auto upstream = mUpstream;
                onError(error);
                if (upstream) {
                    upstream->dispose();
                }
                return;
            }
        }

        if (const auto ds = mDownstream) {
            ds->onNext(value);
        }
    }

    void onError(const GAnyException &e) override
    {
        if (isDisposed())
            return;

        GAnyException downstreamError = e;
        try {
            if (mOnError) {
                mOnError(e);
            }
        } catch (...) {
            downstreamError = ExceptionHelper::fromCurrentException("DoOnEach: onError failed");
        }

        if (const auto ds = mDownstream) {
            ds->onError(downstreamError);
        }

        mUpstream = nullptr; // Break upstream reference after downstream call
        runFinally();
    }

    void onComplete() override
    {
        if (isDisposed()) {
            return;
        }

        try {
            if (mOnComplete) {
                mOnComplete();
            }
        } catch (...) {
            if (const auto ds = mDownstream) {
                ds->onError(ExceptionHelper::fromCurrentException("DoOnEach: onComplete failed"));
            }
            mUpstream = nullptr;
            runFinally();
            return;
        }

        if (const auto ds = mDownstream) {
            ds->onComplete();
        }

        mUpstream = nullptr; // Break upstream reference after downstream call
        runFinally();
    }

    // Disposable implementation
    void dispose() override
    {
        if (const auto d = mUpstream) {
            d->dispose();
        }
        // mUpstream = nullptr;
        runFinally();
    }

    bool isDisposed() const override
    {
        return mUpstream ? mUpstream->isDisposed() : true;
    }

private:
    void runFinally()
    {
        if (mFinallyRun.exchange(true)) {
            return;
        }

        if (mOnFinally) {
            try {
                mOnFinally();
            } catch (...) {
                // Exceptions in finally are usually swallowed or routed to global error handler
            }
        }
    }

private:
    ObserverPtr mDownstream;
    OnNextAction mOnNext;
    OnErrorAction mOnError;
    OnCompleteAction mOnComplete;
    OnSubscribeAction mOnSubscribe;
    OnCompleteAction mOnFinally; // Reusing OnCompleteAction signature for void()

    DisposablePtr mUpstream;
    std::atomic<bool> mFinallyRun{false};
};

class ObservableDoOnEach : public Observable
{
public:
    ObservableDoOnEach(ObservableSourcePtr source,
                       OnNextAction onNext,
                       OnErrorAction onError,
                       OnCompleteAction onComplete,
                       OnSubscribeAction onSubscribe,
                       OnCompleteAction onFinally)
        : mSource(std::move(source)),
          mOnNext(std::move(onNext)),
          mOnError(std::move(onError)),
          mOnComplete(std::move(onComplete)),
          mOnSubscribe(std::move(onSubscribe)),
          mOnFinally(std::move(onFinally))
    {
        LeakObserver::make<ObservableDoOnEach>();
    }

    ~ObservableDoOnEach() override
    {
        LeakObserver::release<ObservableDoOnEach>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        const auto doObserver = std::make_shared<DoOnEachObserver>(
            observer, mOnNext, mOnError, mOnComplete, mOnSubscribe, mOnFinally);
        mSource->subscribe(doObserver);
    }

private:
    ObservableSourcePtr mSource;
    OnNextAction mOnNext;
    OnErrorAction mOnError;
    OnCompleteAction mOnComplete;
    OnSubscribeAction mOnSubscribe;
    OnCompleteAction mOnFinally;
};
} // rx

#endif //RX_OBSERVABLE_DO_ON_EACH_H
