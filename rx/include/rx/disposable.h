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

    static bool validate(Disposable *const current, Disposable *const next)
    {
        if (next == nullptr) {
            return false;
        }
        if (current != nullptr) {
            next->dispose();
            return false;
        }
        return true;
    }
};

using DisposablePtr = std::shared_ptr<Disposable>;


class AtomicDisposable : public Disposable
{
public:
    ~AtomicDisposable() override = default;

    void dispose() override
    {
        mDisposed.store(true, std::memory_order_release);
    }

    bool isDisposed() const override
    {
        return mDisposed.load(std::memory_order_acquire);
    }

private:
    std::atomic<bool> mDisposed = false;
};
} // rx

#endif //RX_DISPOSABLE_H
