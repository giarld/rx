//
// Created by Gxin on 2026/1/5.
//

#ifndef RX_OBSERVABLE_CREATE_H
#define RX_OBSERVABLE_CREATE_H

#include "../observable.h"


namespace rx
{
class CreateEmitter : public ObservableEmitter, public Disposable
{
public:
    explicit CreateEmitter(Observer *observer)
        : mObserver(observer)
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
            mObserver->onNext(value);
        }
    }

    void onError(const GAnyException &e) override
    {
        if (!isDisposed()) {
            try {
                mObserver->onError(e);
            } catch (GAnyException _e) {
            }
            dispose();
        }
    }

    void onCompleted() override
    {
        if (!isDisposed()) {
            try {
                mObserver->onComplete();
            } catch (GAnyException _e) {
            }
            dispose();
        }
    }

    void dispose() override
    {

    }

    void setDisposable(const DisposablePtr &d) override
    {

    }

    bool isDisposed() const override
    {
        return false;
    }

private:
    Observer *mObserver;
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

public:
    void subscribeActual(const ObserverPtr &observer) override
    {
        const auto parent = std::make_shared<CreateEmitter>(observer.get());
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
