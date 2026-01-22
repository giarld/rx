//
// Created by Gxin on 2026/1/22.
//

#ifndef RX_BLOCKING_LAST_OBSERVER_H
#define RX_BLOCKING_LAST_OBSERVER_H

#include "../observer.h"


namespace rx
{
class BlockingLastObserver : public Observer
{
public:
    BlockingLastObserver() { LeakObserver::make<BlockingLastObserver>(); }
    ~BlockingLastObserver() override { LeakObserver::release<BlockingLastObserver>(); }

    void onSubscribe(const DisposablePtr &d) override { mDisposable = d; }

    void onNext(const GAny &value) override
    {
        GLockerGuard locker(mLock);
        mValue = value;
        mHasValue = true;
    }

    void onError(const GAnyException &e) override
    {
        GLockerGuard locker(mLock);
        mError = e;
        mDone = true;
        mCv.notify_all();
    }

    void onComplete() override
    {
        GLockerGuard locker(mLock);
        mDone = true;
        mCv.notify_all();
    }

    GAny blockingGet(const GAny &defaultValue, bool hasDefault)
    {
        GLocker locker(mLock);
        mCv.wait(locker, [this] { return mDone; });

        if (mError.isException()) {
            throw *mError.as<GAnyException>();
        }
        if (mHasValue) {
            return mValue;
        }
        if (hasDefault) {
            return defaultValue;
        }

        throw GAnyException("Observable emitted no items");
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

#endif //RX_BLOCKING_LAST_OBSERVER_H
