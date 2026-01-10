//
// Created by Gxin on 2026/1/5.
//

#ifndef RX_OBSERVABLE_CREATE_H
#define RX_OBSERVABLE_CREATE_H

#include "../observable.h"
#include <gx/gmutex.h>


namespace rx
{
class CreateEmitter : public ObservableEmitter, public Disposable
{
public:
    explicit CreateEmitter(const ObserverPtr &observer)
        : mObserver(observer), mDisposable(std::make_shared<AtomicDisposable>())
    {
    }

    ~CreateEmitter() override = default;

public:
    void onNext(const GAny &value) override
    {
        if (!value) {
            onError(GAnyException("onNext called with a null value."));
            return;
        }
        if (!isDisposed()) {
            if (const auto o = mObserver.lock()) {
                o->onNext(value);
            }
        }
    }

    void onError(const GAnyException &e) override
    {
        if (!isDisposed()) {
            try {
                if (const auto o = mObserver.lock()) {
                    o->onError(e);
                }
            } catch (GAnyException _e) {
            }
            dispose();
        }
    }

    void onComplete() override
    {
        if (!isDisposed()) {
            try {
                if (const auto o = mObserver.lock()) {
                    o->onComplete();
                }
            } catch (GAnyException _e) {
            }
            dispose();
        }
    }

    void dispose() override
    {
        if (const auto d = mDisposable) {
            d->dispose();
        }
    }

    bool isDisposed() const override
    {
        if (const auto d = mDisposable) {
            return d->isDisposed();
        }
        return false;
    }

    void setDisposable(const DisposablePtr &d) override
    {
        mDisposable = d;
    }

private:
    std::weak_ptr<Observer> mObserver;
    DisposablePtr mDisposable = nullptr;
};


class ObservableCreate : public Observable
{
public:
    ~ObservableCreate() override = default;

public:
    explicit ObservableCreate(ObservableOnSubscribe source)
        : mSource(std::move(source))
    {
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        const auto parent = std::make_shared<CreateEmitter>(observer);
        observer->onSubscribe(parent);

        try {
            mSource(parent);
        } catch (GAnyException e) {
            parent->onError(e);
        }
    }

private:
    ObservableOnSubscribe mSource;
};
} // rx

#endif //RX_OBSERVABLE_CREATE_H
