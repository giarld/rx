//
// Created by Gxin on 2026/1/22.
//

#ifndef RX_BLOCKING_FIRST_OBSERVER_H
#define RX_BLOCKING_FIRST_OBSERVER_H

#include "../observer.h"


namespace rx
{
class BlockingFirstObserver : public Observer
{
public:
    explicit BlockingFirstObserver()
    {
        LeakObserver::make<BlockingFirstObserver>();
    }

    ~BlockingFirstObserver() override
    {
        LeakObserver::release<BlockingFirstObserver>();
    }

    void onSubscribe(const DisposablePtr &d) override
    {
        mDisposable = d;
    }

    void onNext(const GAny &value) override
    {
        GLockerGuard locker(mLock);
        if (!mDone) {
            mValue = value;
            mHasValue = true;
            mDone = true;
            if (mDisposable) {
                mDisposable->dispose();
            }
            mCv.notify_all();
        }
    }

    void onError(const GAnyException &e) override
    {
        GLockerGuard locker(mLock);
        if (!mDone) {
            mError = e;
            mDone = true;
            mCv.notify_all();
        }
    }

    void onComplete() override
    {
        GLockerGuard locker(mLock);
        if (!mDone) {
            mDone = true;
            mCv.notify_all();
        }
    }

    GAny blockingGet()
    {
        GLocker locker(mLock);
        mCv.wait(locker, [this] { return mDone; });

        if (mError.isException()) {
            throw *mError.as<GAnyException>();
        }
        if (mHasValue) {
            return mValue;
        }
        throw GAnyException("Observable emitted no items");
    }

    GAny blockingGet(const GAny &defaultValue)
    {
        GLocker locker(mLock);
        mCv.wait(locker, [this] { return mDone; });

        if (mError.isException()) {
            throw *mError.as<GAnyException>();
        }
        if (mHasValue) {
            return mValue;
        }
        return defaultValue;
    }

private:
    GMutex mLock;
    std::condition_variable mCv;
    DisposablePtr mDisposable;

    GAny mValue;
    GAny mError;

    bool mHasValue = false;
    bool mDone = false;
};
} // rx

#endif //RX_BLOCKING_FIRST_OBSERVER_H
