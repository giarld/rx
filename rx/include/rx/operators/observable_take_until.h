//
// Created by Gxin on 2026/2/4.
//

#ifndef RX_OBSERVABLE_TAKE_UNTIL_H
#define RX_OBSERVABLE_TAKE_UNTIL_H

#include "../observable.h"
#include "../disposables/disposable_helper.h"
#include "../disposables/sequential_disposable.h"
#include "../leak_observer.h"


namespace rx
{
class TakeUntilMainObserver;

class TakeUntilOtherObserver : public Observer
{
public:
    explicit TakeUntilOtherObserver(std::shared_ptr<TakeUntilMainObserver> parent);

    ~TakeUntilOtherObserver() override;

public:
    void onSubscribe(const DisposablePtr &d) override;

    void onNext(const GAny &value) override;

    void onError(const GAnyException &e) override;

    void onComplete() override;

private:
    std::weak_ptr<TakeUntilMainObserver> mParent;
};

class TakeUntilMainObserver : public Observer, public Disposable, public std::enable_shared_from_this<TakeUntilMainObserver>
{
public:
    explicit TakeUntilMainObserver(ObserverPtr downstream)
        : mDownstream(std::move(downstream)),
          mOtherDisposable(std::make_shared<SequentialDisposable>())
    {
        LeakObserver::make<TakeUntilMainObserver>();
    }

    ~TakeUntilMainObserver() override
    {
        LeakObserver::release<TakeUntilMainObserver>();
    }

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        if (DisposableHelper::validate(mUpstream, d)) {
            mUpstream = d;
            mDownstream->onSubscribe(shared_from_this());
        }
    }

    void onNext(const GAny &value) override
    {
        if (mDone) {
            return;
        }
        GLockerGuard lock(mLock);
        if (!mDone) {
            mDownstream->onNext(value);
        }
    }

    void onError(const GAnyException &e) override
    {
        if (mDone) {
            return;
        }
        GLockerGuard lock(mLock);
        if (!mDone) {
            mDone = true;
            mOtherDisposable->dispose();
            mDownstream->onError(e);
        }
    }

    void onComplete() override
    {
        if (mDone) {
            return;
        }
        GLockerGuard lock(mLock);
        if (!mDone) {
            mDone = true;
            mOtherDisposable->dispose();
            mDownstream->onComplete();
        }
    }

    void dispose() override
    {
        if (mDone) {
            return;
        }
        GLockerGuard lock(mLock);
        if (!mDone) {
            mDone = true;
            if (mUpstream) {
                mUpstream->dispose();
            }
            mOtherDisposable->dispose();
        }
    }

    bool isDisposed() const override
    {
        return mDone;
    }

    void setOtherDisposable(const DisposablePtr &d)
    {
        mOtherDisposable->update(d);
    }

    void otherError(const GAnyException &e)
    {
        if (mDone) {
            return;
        }
        GLockerGuard lock(mLock);
        if (!mDone) {
            mDone = true;
            if (mUpstream) {
                mUpstream->dispose();
            }
            mOtherDisposable->dispose(); // Dispose self just in case
            mDownstream->onError(e);
        }
    }

    void otherComplete()
    {
        if (mDone) {
            return;
        }
        GLockerGuard lock(mLock);
        if (!mDone) {
            mDone = true;
            if (mUpstream) {
                mUpstream->dispose();
            }
            mOtherDisposable->dispose();
            mDownstream->onComplete();
        }
    }

private:
    ObserverPtr mDownstream;
    DisposablePtr mUpstream;
    std::shared_ptr<SequentialDisposable> mOtherDisposable;
    
    std::atomic<bool> mDone{false};
    GSpinLock mLock;
};

inline TakeUntilOtherObserver::TakeUntilOtherObserver(std::shared_ptr<TakeUntilMainObserver> parent)
    : mParent(std::move(parent))
{
    LeakObserver::make<TakeUntilOtherObserver>();
}

inline TakeUntilOtherObserver::~TakeUntilOtherObserver()
{
    LeakObserver::release<TakeUntilOtherObserver>();
}

inline void TakeUntilOtherObserver::onSubscribe(const DisposablePtr &d)
{
    if (auto p = mParent.lock()) {
        p->setOtherDisposable(d);
    }
}

inline void TakeUntilOtherObserver::onNext(const GAny &value)
{
    if (auto p = mParent.lock()) {
        p->otherComplete();
    }
}

inline void TakeUntilOtherObserver::onError(const GAnyException &e)
{
    if (auto p = mParent.lock()) {
        p->otherError(e);
    }
}

inline void TakeUntilOtherObserver::onComplete()
{
    if (auto p = mParent.lock()) {
        p->otherComplete();
    }
}

class ObservableTakeUntil : public Observable
{
public:
    ObservableTakeUntil(ObservableSourcePtr source, ObservableSourcePtr other)
        : mSource(std::move(source)), mOther(std::move(other))
    {
        LeakObserver::make<ObservableTakeUntil>();
    }

    ~ObservableTakeUntil() override
    {
        LeakObserver::release<ObservableTakeUntil>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        auto mainObserver = std::make_shared<TakeUntilMainObserver>(observer);
        auto otherObserver = std::make_shared<TakeUntilOtherObserver>(mainObserver);

        mSource->subscribe(mainObserver);
        mOther->subscribe(otherObserver);
    }

private:
    ObservableSourcePtr mSource;
    ObservableSourcePtr mOther;
};
} // rx

#endif //RX_OBSERVABLE_TAKE_UNTIL_H
