//
// Created by Gxin on 2026/2/4.
//

#ifndef RX_OBSERVABLE_DO_ON_EACH_H
#define RX_OBSERVABLE_DO_ON_EACH_H

#include "../observable.h"
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
        if (mOnSubscribe) {
            try {
                mOnSubscribe(d);
            } catch (const GAnyException &e) {
                d->dispose();
                onError(e);
                return;
            } catch (...) {
                d->dispose();
                onError(GAnyException("DoOnEach: onSubscribe failed"));
                return;
            }
        }

        mUpstream = d;
        if (const auto ds = mDownstream) {
            ds->onSubscribe(shared_from_this()); // Pass ourselves as the disposable wrapper
        }
    }

    void onNext(const GAny &value) override
    {
        if (isDisposed())
            return;

        if (mOnNext) {
            try {
                mOnNext(value);
            } catch (const GAnyException &e) {
                onError(e);
                return;
            } catch (...) {
                onError(GAnyException("DoOnEach: onNext failed"));
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

        bool errorEmitted = false;
        try {
            if (mOnError) {
                mOnError(e);
            }
        } catch (...) {
            // Composite exception? For now just log or ignore double fault
        }

        if (const auto ds = mDownstream) {
            ds->onError(e);
            errorEmitted = true;
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
        } catch (const GAnyException &e) {
            if (const auto ds = mDownstream) {
                ds->onError(e);
            }
            mUpstream = nullptr;
            runFinally();
            return;
        } catch (...) {
            if (const auto ds = mDownstream) {
                ds->onError(GAnyException("DoOnEach: onComplete failed"));
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
