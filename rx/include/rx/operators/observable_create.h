//
// Created by Gxin on 2026/1/5.
//

#ifndef RX_OBSERVABLE_CREATE_H
#define RX_OBSERVABLE_CREATE_H

#include "../observable.h"
#include "../exception_helper.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"


namespace rx
{
class CreateEmitter : public ObservableEmitter, public Disposable
{
public:
    explicit CreateEmitter(const ObserverPtr &observer)
        : mDownstream(observer)
    {
        LeakObserver::make<CreateEmitter>();
    }

    ~CreateEmitter() override
    {
        LeakObserver::release<CreateEmitter>();
    }

public:
    void onNext(const GAny &value) override
    {
        // if (!value) {
        //     onError(GAnyException("onNext called with a null value."));
        //     return;
        // }
        if (!isDisposed()) {
            try {
                if (const auto o = mDownstream) {
                    o->onNext(value);
                }
            } catch (...) {
                onError(ExceptionHelper::fromCurrentException("CreateEmitter: Downstream onNext failed"));
            }
        }
    }

    void onError(const GAnyException &e) override
    {
        if (!isDisposed()) {
            try {
                if (const auto o = mDownstream) {
                    o->onError(e);
                }
            } catch (...) {
            }
            dispose();
        }
    }

    void onComplete() override
    {
        if (!isDisposed()) {
            try {
                if (const auto o = mDownstream) {
                    o->onComplete();
                }
            } catch (...) {
            }
            dispose();
        }
    }

    void dispose() override
    {
        DisposableHelper::dispose(mDisposable, mLock);
        mDownstream = nullptr;
    }

    bool isDisposed() const override
    {
        return DisposableHelper::isDisposed(mDisposable);
    }

    void setDisposable(const DisposablePtr &d) override
    {
        DisposableHelper::set(mDisposable, d, mLock);
    }

private:
    ObserverPtr mDownstream;
    DisposablePtr mDisposable = nullptr;
    GMutex mLock;
};


class ObservableCreate : public Observable
{
public:
    ~ObservableCreate() override
    {
        LeakObserver::release<ObservableCreate>();
    }

public:
    explicit ObservableCreate(ObservableOnSubscribe source)
        : mSource(std::move(source))
    {
        LeakObserver::make<ObservableCreate>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        const auto parent = std::make_shared<CreateEmitter>(observer);
        observer->onSubscribe(parent);

        try {
            mSource(parent);
        } catch (...) {
            parent->onError(ExceptionHelper::fromCurrentException("ObservableCreate: Source failed"));
        }
    }

private:
    ObservableOnSubscribe mSource;
};
} // rx

#endif //RX_OBSERVABLE_CREATE_H
