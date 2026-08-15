//
// Created by Gxin on 2026/2/6.
//

#ifndef RX_OBSERVABLE_WINDOW_H
#define RX_OBSERVABLE_WINDOW_H

#include "../observable.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"
#include <atomic>
#include <deque>
#include <vector>
#include <optional>


namespace rx
{
class WindowSubject
{
public:
    WindowSubject()
        : mState(std::make_shared<State>())
    {
        LeakObserver::make<WindowSubject>();

        const auto state = mState;
        mObservable = Observable::create([state](const ObservableEmitterPtr &emitter) {
            std::optional<GAnyException> error;
            bool completed = false;
            {
                GLockerGuard lock(state->lock);
                error = state->error;
                completed = state->completed;
                if (!error && !completed) {
                    state->emitters.push_back(emitter);
                    return;
                }
            }
            if (error) {
                emitter->onError(error.value());
            } else {
                emitter->onComplete();
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
        std::vector<ObservableEmitterPtr> emitters;
        {
            GLockerGuard lock(mState->lock);
            auto it = mState->emitters.begin();
            while (it != mState->emitters.end()) {
                if ((*it)->isDisposed()) {
                    it = mState->emitters.erase(it);
                } else {
                    emitters.push_back(*it);
                    ++it;
                }
            }
        }
        for (const auto &emitter: emitters) {
            emitter->onNext(value);
        }
    }

    void onError(const GAnyException &e)
    {
        std::vector<ObservableEmitterPtr> emitters;
        {
            GLockerGuard lock(mState->lock);
            mState->error = e;
            emitters = std::move(mState->emitters);
            mState->emitters.clear();
        }
        for (const auto &emitter: emitters) {
            if (!emitter->isDisposed()) {
                emitter->onError(e);
            }
        }
    }

    void onComplete()
    {
        std::vector<ObservableEmitterPtr> emitters;
        {
            GLockerGuard lock(mState->lock);
            mState->completed = true;
            emitters = std::move(mState->emitters);
            mState->emitters.clear();
        }
        for (const auto &emitter: emitters) {
            if (!emitter->isDisposed()) {
                emitter->onComplete();
            }
        }
    }

private:
    struct State
    {
        std::vector<ObservableEmitterPtr> emitters;
        std::optional<GAnyException> error;
        bool completed = false;
        GMutex lock;
    };

    std::shared_ptr<Observable> mObservable;
    std::shared_ptr<State> mState;
};

class ObservableWindow;

class WindowObserver : public Observer, public Disposable, public std::enable_shared_from_this<WindowObserver>
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
        if (DisposableHelper::validate(mUpstream, d)) {
            mUpstream = d;
            if (const auto downstream = mDownstream) {
                downstream->onSubscribe(shared_from_this());
            }
        }
    }

    void onNext(const GAny &value) override
    {
        if (mDone.load(std::memory_order_acquire)) {
            return;
        }

        // 1. Check if we need to start a new window
        if (mIndex % mSkip == 0) {
            const auto window = std::make_shared<WindowSubject>();
            mWindows.push_back({window, 0});
            mDownstream->onNext(window->getObservable());
            if (mDone.load(std::memory_order_acquire)) {
                return;
            }
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
        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        for (const auto &w: mWindows) {
            w.subject->onError(e);
        }
        mWindows.clear();
        if (mDownstream)
            mDownstream->onError(e);
        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    void onComplete() override
    {
        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        for (const auto &w: mWindows) {
            w.subject->onComplete();
        }
        mWindows.clear();
        if (mDownstream)
            mDownstream->onComplete();
        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    void dispose() override
    {
        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        if (mUpstream) {
            mUpstream->dispose();
        }
        for (const auto &w: mWindows) {
            w.subject->onComplete();
        }
        mWindows.clear();
        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    bool isDisposed() const override
    {
        return mDone.load(std::memory_order_acquire);
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
    std::atomic<bool> mDone{false};

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
