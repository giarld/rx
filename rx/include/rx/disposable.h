//
// Created by Gxin on 2026/1/4.
//

#ifndef RX_DISPOSABLE_H
#define RX_DISPOSABLE_H

#include "gx/gglobal.h"
#include "memory"


namespace rx
{
struct Disposable
{
    virtual ~Disposable() = default;

    virtual void dispose() = 0;

    virtual bool isDisposed() const = 0;
};

using DisposablePtr = std::shared_ptr<Disposable>;
} // rx

#endif //RX_DISPOSABLE_H
