//
// Created by Gxin on 2026/1/21.
//

#ifndef RX_OBSERVABLE_ZIP_H
#define RX_OBSERVABLE_ZIP_H

#include "../observable.h"
#include "../exception_helper.h"
#include "../leak_observer.h"
#include <vector>
#include <deque>
#include <mutex>


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

public:
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
        : mDownstream(downstream), mZipper(std::move(zipper)), mObservers(count), mCompleted(count, false)
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

public:
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
        bool disposeNow = false;
        {
            GLockerGuard lock(mLock);
            if (mCancelled) {
                disposeNow = true;
            } else {
                mDisposables.push_back(d);
            }
        }
        if (disposeNow) {
            d->dispose();
        }
    }

    void onNext(size_t index, const GAny &value)
    {
        std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
        std::vector<GAny> args;
        ObserverPtr downstream;
        {
            GLockerGuard lock(mLock);
            if (mCancelled) {
                return;
            }
            mRows[index].push_back(value);
            if (!takeRow(args)) {
                return;
            }
            ++mInFlight;
            downstream = mDownstream;
        }
        if (!mCancelled) {
            emit(args, downstream);
        }
    }

    void onError(const GAnyException &e)
    {
        std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
        ObserverPtr downstream;
        std::vector<DisposablePtr> disposables;
        {
            GLockerGuard lock(mLock);
            if (mCancelled) {
                return;
            }
            downstream = mDownstream;
            cancelAll(disposables);
        }
        disposeAll(disposables);
        if (downstream) {
            downstream->onError(e);
        }
    }

    void onComplete(size_t index)
    {
        std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
        ObserverPtr downstream;
        std::vector<DisposablePtr> disposables;
        {
            GLockerGuard lock(mLock);
            if (mCancelled) {
                return;
            }
            mCompleted[index] = true;
            if (!hasCompleteEmptySource() || mInFlight != 0) {
                return;
            }
            downstream = mDownstream;
            cancelAll(disposables);
        }
        disposeAll(disposables);
        if (downstream) {
            downstream->onComplete();
        }
    }

    void dispose() override
    {
        std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
        std::vector<DisposablePtr> disposables;
        {
            GLockerGuard lock(mLock);
            if (mCancelled) {
                return;
            }
            cancelAll(disposables);
        }
        disposeAll(disposables);
    }

    bool isDisposed() const override
    {
        return mCancelled;
    }

private:
    bool takeRow(std::vector<GAny> &args)
    {
        for (const auto &row: mRows) {
            if (row.empty()) {
                return false;
            }
        }
        args.reserve(mRows.size());
        for (auto &row: mRows) {
            args.push_back(row.front());
            row.pop_front();
        }
        return true;
    }

    bool hasCompleteEmptySource() const
    {
        for (size_t i = 0; i < mRows.size(); ++i) {
            if (mCompleted[i] && mRows[i].empty()) {
                return true;
            }
        }
        return false;
    }

    void emit(const std::vector<GAny> &args, const ObserverPtr &downstream)
    {
        GAny result;
        try {
            result = mZipper(args);
        } catch (...) {
            onError(ExceptionHelper::fromCurrentException("Zip: Zipper failed"));
            return;
        }
        if (!isDisposed() && downstream) {
            downstream->onNext(result);
        }
        ObserverPtr completionDownstream;
        std::vector<DisposablePtr> disposables;
        {
            GLockerGuard lock(mLock);
            if (!mCancelled) {
                --mInFlight;
            }
            if (!mCancelled && mInFlight == 0 && hasCompleteEmptySource()) {
                completionDownstream = mDownstream;
                cancelAll(disposables);
            }
        }
        disposeAll(disposables);
        if (completionDownstream) {
            completionDownstream->onComplete();
        }
    }

    void cancelAll(std::vector<DisposablePtr> &disposables)
    {
        mCancelled = true;
        disposables = std::move(mDisposables);
        mDisposables.clear();
        mRows.clear();
        mObservers.clear();     // Clear observer references
        mDownstream = nullptr;   // Release downstream reference
    }

    static void disposeAll(const std::vector<DisposablePtr> &disposables)
    {
        for (const auto &disposable: disposables) {
            if (disposable) {
                disposable->dispose();
            }
        }
    }

private:
    ObserverPtr mDownstream;
    CombineLatestFunction mZipper;
    std::vector<std::shared_ptr<ZipInnerObserver> > mObservers;
    std::vector<DisposablePtr> mDisposables;
    std::vector<std::deque<GAny> > mRows;
    std::vector<bool> mCompleted;
    size_t mInFlight = 0;

    std::atomic<bool> mCancelled = false;
    GMutex mLock;
    std::recursive_mutex mSignalLock;
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
        p->onComplete(mIndex);
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
