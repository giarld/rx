//
// Created by Gxin on 2026/2/6.
//

#ifndef RX_OBSERVABLE_ON_ERROR_RESUME_NEXT_H
#define RX_OBSERVABLE_ON_ERROR_RESUME_NEXT_H

#include "../observable.h"
#include "../observer.h"
#include "../leak_observer.h"
#include "../disposables/sequential_disposable.h"


namespace rx
{
class OnErrorResumeNextObserver : public Observer, public std::enable_shared_from_this<OnErrorResumeNextObserver>
{
public:
    OnErrorResumeNextObserver(const ObserverPtr &downstream, const ResumeFunction &resumeFunction)
        : mDownstream(downstream), mResumeFunction(resumeFunction), mSerial(std::make_shared<SequentialDisposable>())
    {
        LeakObserver::make<OnErrorResumeNextObserver>();
    }

    ~OnErrorResumeNextObserver() override
    {
        LeakObserver::release<OnErrorResumeNextObserver>();
    }

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        mSerial->replace(d);
    }

    void onNext(const GAny &value) override
    {
        if (mDownstream) {
            mDownstream->onNext(value);
        }
    }

    void onError(const GAnyException &e) override
    {
        if (!mOnce) {
            mOnce = true;
            try {
                const auto next = mResumeFunction(e);
                if (next) {
                    next->subscribe(this->shared_from_this());
                } else {
                    if (mDownstream)
                        mDownstream->onError(GAnyException("OnErrorResumeNext: Function returned null Observable"));
                }
            } catch (const GAnyException &ex) {
                if (mDownstream)
                    mDownstream->onError(ex);
            } catch (...) {
                if (mDownstream)
                    mDownstream->onError(GAnyException("OnErrorResumeNext: Unknown error in resume function"));
            }
        } else {
            if (mDownstream)
                mDownstream->onError(e);
        }
    }

    void onComplete() override
    {
        if (mDownstream) {
            mDownstream->onComplete();
        }
    }

    DisposablePtr getDisposable() const
    {
        return mSerial;
    }

private:
    ObserverPtr mDownstream;
    ResumeFunction mResumeFunction;
    SequentialDisposablePtr mSerial;
    bool mOnce = false;
};

class ObservableOnErrorResumeNext : public Observable
{
public:
    ObservableOnErrorResumeNext(std::shared_ptr<Observable> source, ResumeFunction resumeFunction)
        : mSource(std::move(source)), mResumeFunction(std::move(resumeFunction))
    {
        LeakObserver::make<ObservableOnErrorResumeNext>();
    }

    ~ObservableOnErrorResumeNext() override
    {
        LeakObserver::release<ObservableOnErrorResumeNext>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        const auto parent = std::make_shared<OnErrorResumeNextObserver>(observer, mResumeFunction);
        observer->onSubscribe(parent->getDisposable());
        mSource->subscribe(parent);
    }

private:
    std::shared_ptr<Observable> mSource;
    ResumeFunction mResumeFunction;
};
} // rx

#endif //RX_OBSERVABLE_ON_ERROR_RESUME_NEXT_H
