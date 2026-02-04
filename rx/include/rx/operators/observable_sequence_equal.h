//
// Created by Gxin on 2026/2/4.
//

#ifndef RX_OBSERVABLE_SEQUENCE_EQUAL_H
#define RX_OBSERVABLE_SEQUENCE_EQUAL_H

#include "../observable.h"
#include "../observer.h"
#include "../leak_observer.h"
#include <deque>

namespace rx
{
class SequenceEqualCoordinator;

class SequenceEqualInnerObserver : public Observer
{
public:
    SequenceEqualInnerObserver(std::shared_ptr<SequenceEqualCoordinator> parent, int index);

    ~SequenceEqualInnerObserver() override;

    void onSubscribe(const DisposablePtr &d) override;

    void onNext(const GAny &value) override;

    void onError(const GAnyException &e) override;

    void onComplete() override;

private:
    std::weak_ptr<SequenceEqualCoordinator> mParent;
    int mIndex;
};

class SequenceEqualCoordinator : public Disposable, public std::enable_shared_from_this<SequenceEqualCoordinator>
{
public:
    SequenceEqualCoordinator(const ObserverPtr &downstream, BiFunction comparator, int bufferSize)
        : mDownstream(downstream), mComparator(std::move(comparator)), mBufferSize(bufferSize)
    {
        LeakObserver::make<SequenceEqualCoordinator>();
    }

    ~SequenceEqualCoordinator() override
    {
        LeakObserver::release<SequenceEqualCoordinator>();
    }

    void subscribe(const std::shared_ptr<Observable> &source1, const std::shared_ptr<Observable> &source2)
    {
        const auto inner1 = std::make_shared<SequenceEqualInnerObserver>(shared_from_this(), 0);
        const auto inner2 = std::make_shared<SequenceEqualInnerObserver>(shared_from_this(), 1);

        mResources[0] = inner1;

        mObservers[0] = inner1;
        mObservers[1] = inner2;

        source1->subscribe(inner1);
        source2->subscribe(inner2);
    }

    void onSubscribe(int index, const DisposablePtr &d)
    {
        if (DisposableHelper::setOnce(mDisposables[index], d, mLock)) {
            // ok
        }
    }

    void onNext(int index, const GAny &value)
    {
        GLockerGuard lock(mLock);
        if (mCancelled)
            return;

        if (index == 0)
            mBuffer1.push_back(value);
        else
            mBuffer2.push_back(value);

        drain();
    }

    void onError(const GAnyException &e)
    {
        GLockerGuard lock(mLock);
        if (mCancelled)
            return;

        cancelAll();
        mDownstream->onError(e);
    }

    void onComplete(int index)
    {
        GLockerGuard lock(mLock);
        if (mCancelled)
            return;

        if (index == 0)
            mDone1 = true;
        else
            mDone2 = true;

        drain();
    }

    void dispose() override
    {
        if (!mCancelled) {
            GLockerGuard lock(mLock);
            cancelAll();
        }
    }

    bool isDisposed() const override
    {
        return mCancelled;
    }

private:
    void drain()
    {
        while (true) {
            if (mCancelled)
                return;

            const bool has1 = !mBuffer1.empty();
            const bool has2 = !mBuffer2.empty();

            if (has1 && has2) {
                GAny v1 = mBuffer1.front();
                mBuffer1.pop_front();
                GAny v2 = mBuffer2.front();
                mBuffer2.pop_front();

                bool equal = false;
                try {
                    GAny res = mComparator(v1, v2);
                    if (res.is<bool>()) {
                        equal = res.toBool();
                    } else {
                        equal = (v1 == v2);
                    }
                } catch (...) {
                    cancelAll();
                    mDownstream->onError(GAnyException("Comparator failed"));
                    return;
                }

                if (!equal) {
                    cancelAll();
                    mDownstream->onNext(false);
                    mDownstream->onComplete();
                    return;
                }
            } else {
                if (mDone1 && !has1) {
                    if (has2) {
                        // v1 finished, v2 has more -> false
                        cancelAll();
                        mDownstream->onNext(false);
                        mDownstream->onComplete();
                        return;
                    }
                    if (mDone2 && !has2) {
                        // Both finished and empty -> true
                        cancelAll();
                        mDownstream->onNext(true);
                        mDownstream->onComplete();
                        return;
                    }
                }

                if (mDone2 && !has2) {
                    if (has1) {
                        // v2 finished, v1 has more -> false
                        cancelAll();
                        mDownstream->onNext(false);
                        mDownstream->onComplete();
                        return;
                    }
                }

                break; // Wait for more
            }
        }
    }

    void cancelAll()
    {
        mCancelled = true;
        if (mDisposables[0])
            mDisposables[0]->dispose();
        if (mDisposables[1])
            mDisposables[1]->dispose();
        mBuffer1.clear();
        mBuffer2.clear();
    }

    ObserverPtr mDownstream;
    BiFunction mComparator;
    int mBufferSize;

    DisposablePtr mDisposables[2];
    std::shared_ptr<SequenceEqualInnerObserver> mObservers[2];
    // Need to keep observers alive? In this design, source holds observers.
    // But we might need weak_ptr in inner observers.

    std::deque<GAny> mBuffer1;
    std::deque<GAny> mBuffer2;

    bool mDone1 = false;
    bool mDone2 = false;
    bool mCancelled = false;

    GMutex mLock;
    // Keep resources alive?
    // Usually inner observers are shared_ptr, passed to source.
    // We can store them to be safe or to break cycles.
    std::vector<std::shared_ptr<SequenceEqualInnerObserver> > mResources{2};
};

inline SequenceEqualInnerObserver::SequenceEqualInnerObserver(std::shared_ptr<SequenceEqualCoordinator> parent, int index)
    : mParent(std::move(parent)), mIndex(index)
{
    LeakObserver::make<SequenceEqualInnerObserver>();
}

inline SequenceEqualInnerObserver::~SequenceEqualInnerObserver()
{
    LeakObserver::release<SequenceEqualInnerObserver>();
}

inline void SequenceEqualInnerObserver::onSubscribe(const DisposablePtr &d)
{
    if (const auto p = mParent.lock()) {
        p->onSubscribe(mIndex, d);
    }
}

inline void SequenceEqualInnerObserver::onNext(const GAny &value)
{
    if (const auto p = mParent.lock()) {
        p->onNext(mIndex, value);
    }
}

inline void SequenceEqualInnerObserver::onError(const GAnyException &e)
{
    if (const auto p = mParent.lock()) {
        p->onError(e);
    }
}

inline void SequenceEqualInnerObserver::onComplete()
{
    if (const auto p = mParent.lock()) {
        p->onComplete(mIndex);
    }
}

class ObservableSequenceEqual : public Observable
{
public:
    ObservableSequenceEqual(std::shared_ptr<Observable> source1, std::shared_ptr<Observable> source2, BiFunction comparator, int bufferSize)
        : mSource1(std::move(source1)), mSource2(std::move(source2)), mComparator(std::move(comparator)), mBufferSize(bufferSize)
    {
        LeakObserver::make<ObservableSequenceEqual>();
    }

    ~ObservableSequenceEqual() override
    {
        LeakObserver::release<ObservableSequenceEqual>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        const auto coordinator = std::make_shared<SequenceEqualCoordinator>(observer, mComparator, mBufferSize);
        observer->onSubscribe(coordinator);
        coordinator->subscribe(mSource1, mSource2);
    }

private:
    std::shared_ptr<Observable> mSource1;
    std::shared_ptr<Observable> mSource2;
    BiFunction mComparator;
    int mBufferSize;
};
} // rx

#endif //RX_OBSERVABLE_SEQUENCE_EQUAL_H
