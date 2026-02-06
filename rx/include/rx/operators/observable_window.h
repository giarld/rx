//
// Created by Gxin on 2026/2/6.
//

#ifndef RX_OBSERVABLE_WINDOW_H
#define RX_OBSERVABLE_WINDOW_H

#include "../observable.h"
#include "../leak_observer.h"
#include <deque>
#include <vector>
#include <optional>


namespace rx
{
class WindowSubject
{
public:
    WindowSubject()
    {
        LeakObserver::make<WindowSubject>();

        mObservable = Observable::create([this](const ObservableEmitterPtr &emitter) {
            GLockerGuard lock(mLock);
            if (mError) {
                emitter->onError(mError.value());
            } else if (mCompleted) {
                emitter->onComplete();
            } else {
                mEmitters.push_back(emitter);
            }
        });
    }

    ~WindowSubject()
    {
        LeakObserver::release<WindowSubject>();
    }

public:
    std::shared_ptr<Observable> getObservable() const { return mObservable; }

    void onNext(const GAny &value)
    {
        GLockerGuard lock(mLock);
        auto it = mEmitters.begin();
        while (it != mEmitters.end()) {
            if ((*it)->isDisposed()) {
                it = mEmitters.erase(it);
            } else {
                (*it)->onNext(value);
                ++it;
            }
        }
    }

    void onError(const GAnyException &e)
    {
        GLockerGuard lock(mLock);
        mError = e;
        for (const auto &emitter: mEmitters) {
            if (!emitter->isDisposed())
                emitter->onError(e);
        }
        mEmitters.clear();
    }

    void onComplete()
    {
        GLockerGuard lock(mLock);
        mCompleted = true;
        for (const auto &emitter: mEmitters) {
            if (!emitter->isDisposed())
                emitter->onComplete();
        }
        mEmitters.clear();
    }

private:
    std::shared_ptr<Observable> mObservable;
    std::vector<ObservableEmitterPtr> mEmitters;
    std::optional<GAnyException> mError;
    bool mCompleted = false;
    GMutex mLock;
};

class ObservableWindow;

class WindowObserver : public Observer, public std::enable_shared_from_this<WindowObserver>
{
public:
    WindowObserver(ObserverPtr downstream, int32_t count, int32_t skip)
        : mDownstream(std::move(downstream)), mCount(count), mSkip(skip)
    {
        LeakObserver::make<WindowObserver>();
    }

    ~WindowObserver() override
    {
        LeakObserver::release<WindowObserver>();
    }

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        if (mDownstream)
            mDownstream->onSubscribe(d);
        mUpstream = d;
    }

    void onNext(const GAny &value) override
    {
        // 1. Check if we need to start a new window
        if (mIndex % mSkip == 0) {
            const auto window = std::make_shared<WindowSubject>();
            mWindows.push_back({window, 0});
            mDownstream->onNext(window->getObservable());
        }

        // 2. Emit value to all active windows
        auto it = mWindows.begin();
        while (it != mWindows.end()) {
            it->subject->onNext(value);
            it->count++;

            if (it->count >= mCount) {
                it->subject->onComplete();
                it = mWindows.erase(it);
            } else {
                ++it;
            }
        }

        mIndex++;
    }

    void onError(const GAnyException &e) override
    {
        for (const auto &w: mWindows) {
            w.subject->onError(e);
        }
        mWindows.clear();
        if (mDownstream)
            mDownstream->onError(e);
    }

    void onComplete() override
    {
        for (const auto &w: mWindows) {
            w.subject->onComplete();
        }
        mWindows.clear();
        if (mDownstream)
            mDownstream->onComplete();
    }

private:
    struct ActiveWindow
    {
        std::shared_ptr<WindowSubject> subject;
        int32_t count = 0;
    };

    ObserverPtr mDownstream;
    int32_t mCount;
    int32_t mSkip;
    DisposablePtr mUpstream;

    int64_t mIndex = 0;
    std::deque<ActiveWindow> mWindows;
};

class ObservableWindow : public Observable
{
public:
    ObservableWindow(std::shared_ptr<Observable> source, int32_t count, int32_t skip)
        : mSource(std::move(source)), mCount(count), mSkip(skip)
    {
        LeakObserver::make<ObservableWindow>();
    }

    ~ObservableWindow() override
    {
        LeakObserver::release<ObservableWindow>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<WindowObserver>(observer, mCount, mSkip));
    }

private:
    std::shared_ptr<Observable> mSource;
    int32_t mCount;
    int32_t mSkip;
};
} // rx

#endif //RX_OBSERVABLE_WINDOW_H
