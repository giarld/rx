//
// Created by Gxin on 2026/1/22.
//

#ifndef RX_BLOCKING_FOR_EACH_OBSERVER_H
#define RX_BLOCKING_FOR_EACH_OBSERVER_H

#include "../observer.h"


namespace rx
{
class BlockingForEachObserver : public Observer
{
public:
    explicit BlockingForEachObserver(const OnNextAction &onNext)
        : mOnNext(onNext)
    {
        LeakObserver::make<BlockingForEachObserver>();
    }

    ~BlockingForEachObserver() override
    {
        LeakObserver::release<BlockingForEachObserver>();
    }

    void onSubscribe(const DisposablePtr &d) override
    {
        mUpstream = d;
    }

    void onNext(const GAny &value) override
    {
        GLocker locker(mLock);
        if (mDone) {
            return;
        }

        locker.unlock();

        try {
            mOnNext(value);
        } catch (const GAnyException &e) {
            locker.lock();
            if (!mDone) {
                mError = e;
                mDone = true;
                if (mUpstream)
                    mUpstream->dispose();
                mCv.notify_all();
            }
        } catch (const std::exception &e) {
            locker.lock();
            if (!mDone) {
                mError = GAnyException(e.what());
                mDone = true;
                if (mUpstream)
                    mUpstream->dispose();
                mCv.notify_all();
            }
        } catch (...) {
            locker.lock();
            if (!mDone) {
                mError = GAnyException("Unknown exception in blockingForEach action");
                mDone = true;
                if (mUpstream) {
                    mUpstream->dispose();
                }
                mCv.notify_all();
            }
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

    void blockingWait()
    {
        GLocker locker(mLock);
        mCv.wait(locker, [this] { return mDone; });

        if (mError.isException()) {
            throw *mError.as<GAnyException>();
        }
    }

private:
    OnNextAction mOnNext;
    DisposablePtr mUpstream;

    GMutex mLock;
    std::condition_variable mCv;

    GAny mError;
    bool mDone = false;
};
} // rx

#endif //RX_BLOCKING_FOR_EACH_OBSERVER_H
