//
// Created by Gxin on 2026/1/5.
//

#ifndef RX_CANCELLABLE_H
#define RX_CANCELLABLE_H

namespace rx
{
struct Cancellable
{
    virtual ~Cancellable() = default;

    virtual void cancel() = 0;
};
} // rx

#endif //RX_CANCELLABLE_H