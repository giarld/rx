//
// Created by Gxin on 2026/1/5.
//

#ifndef RX_EMITTER_H
#define RX_EMITTER_H

#include "disposable.h"
#include <gx/gany.h>


namespace rx
{
struct Emitter
{
    virtual ~Emitter() = default;

    virtual void onNext(const GAny &value) = 0;

    virtual void onError(const GAnyException &e) = 0;

    virtual void onCompleted() = 0;
};


struct ObservableEmitter : Emitter
{
    virtual void setDisposable(const DisposablePtr &d) = 0;

    virtual bool isDisposed() const = 0;
};

using ObservableEmitterPtr = std::shared_ptr<ObservableEmitter>;
} // rx

#endif //RX_EMITTER_H
