//
// Created by Gxin on 2026/1/21.
//

#ifndef RX_OBSERVABLE_ZIP_H
#define RX_OBSERVABLE_ZIP_H

#include "../observable.h"
#include "../leak_observer.h"
#include <vector>
#include <deque>


namespace rx
{
class ZipCoordinator;

class ZipInnerObserver : public Observer
{
public:
    ZipInnerObserver(std::shared_ptr<ZipCoordinator> parent, size_t index)
        : mParent(std::move(parent)), mIndex(index)
    {
    }

    void onSubscribe(const DisposablePtr &d) override;

    void onNext(const GAny &value) override;

    void onError(const GAnyException &e) override;

    void onComplete() override;

private:
    std::weak_ptr<ZipCoordinator> mParent;
    size_t mIndex;
};

class ZipCoordinator : public Disposable, public std::enable_shared_from_this<ZipCoordinator>
{
public:
    ZipCoordinator(const ObserverPtr &downstream, CombineLatestFunction zipper, size_t count)
        : mDownstream(downstream), mZipper(std::move(zipper)), mObservers(count)
    {
        LeakObserver::make<ZipCoordinator>();
        mRows.resize(count);
        for (size_t i = 0; i < count; ++i) {
            mObservers[i] = std::make_shared<ZipInnerObserver>(nullptr, i);
        }
    }

    ~ZipCoordinator() override
    {
        LeakObserver::release<ZipCoordinator>();
    }

    void subscribe(const std::vector<std::shared_ptr<Observable> > &sources)
    {
        for (size_t i = 0; i < sources.size(); ++i) {
            mObservers[i] = std::make_shared<ZipInnerObserver>(shared_from_this(), i);
        }

        for (size_t i = 0; i < sources.size(); ++i) {
            if (isDisposed()) {
                break;
            }
            sources[i]->subscribe(mObservers[i]);
        }
    }

    void onSubscribe(size_t /*index*/, const DisposablePtr &d)
    {
        GLockerGuard lock(mLock);
        mDisposables.push_back(d);
    }

    void onNext(size_t index, const GAny &value)
    {
        GLockerGuard lock(mLock);
        if (isDisposed())
            return;

        mRows[index].push_back(value);
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

    void onComplete()
    {
        GLockerGuard lock(mLock);
        if (mCancelled)
            return;

        mCompletedCount++;
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
            bool hasAll = true;
            for (const auto &row: mRows) {
                if (row.empty()) {
                    hasAll = false;
                    break;
                }
            }

            if (hasAll) {
                std::vector<GAny> args;
                args.reserve(mRows.size());
                for (auto &row: mRows) {
                    args.push_back(row.front());
                    row.pop_front();
                }

                try {
                    GAny result = mZipper(args);
                    mDownstream->onNext(result);
                } catch (const GAnyException &e) {
                    cancelAll();
                    mDownstream->onError(e);
                    return;
                }
            } else {
                break;
            }
        }
    }

    void cancelAll()
    {
        mCancelled = true;
        for (auto &d: mDisposables) {
            if (d) {
                d->dispose();
            }
        }
        mDisposables.clear();
        mRows.clear();
    }

private:
    ObserverPtr mDownstream;
    CombineLatestFunction mZipper;
    std::vector<std::shared_ptr<ZipInnerObserver> > mObservers;
    std::vector<DisposablePtr> mDisposables;
    std::vector<std::deque<GAny> > mRows;

    bool mCancelled = false;
    int mCompletedCount = 0;
    GMutex mLock;
};

inline void ZipInnerObserver::onSubscribe(const DisposablePtr &d)
{
    if (const auto p = mParent.lock()) {
        p->onSubscribe(mIndex, d);
    }
}

inline void ZipInnerObserver::onNext(const GAny &value)
{
    if (const auto p = mParent.lock()) {
        p->onNext(mIndex, value);
    }
}

inline void ZipInnerObserver::onError(const GAnyException &e)
{
    if (const auto p = mParent.lock()) {
        p->onError(e);
    }
}

inline void ZipInnerObserver::onComplete()
{
    if (const auto p = mParent.lock()) {
        p->onComplete();
    }
}

class ObservableZip : public Observable
{
public:
    ObservableZip(std::vector<std::shared_ptr<Observable> > sources, CombineLatestFunction zipper)
        : mSources(std::move(sources)), mZipper(std::move(zipper))
    {
        LeakObserver::make<ObservableZip>();
    }

    ~ObservableZip() override
    {
        LeakObserver::release<ObservableZip>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        if (mSources.empty()) {
            observer->onSubscribe(DisposableHelper::disposed());
            observer->onComplete();
            return;
        }
        const auto coordinator = std::make_shared<ZipCoordinator>(observer, mZipper, mSources.size());
        observer->onSubscribe(coordinator);
        coordinator->subscribe(mSources);
    }

private:
    std::vector<std::shared_ptr<Observable> > mSources;
    CombineLatestFunction mZipper;
};
} // rx

#endif //RX_OBSERVABLE_ZIP_H
