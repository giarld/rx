//
// Created by Gxin on 2026/1/12.
//

#ifndef RX_ATOMIC_DISPOSABLE_H
#define RX_ATOMIC_DISPOSABLE_H

#include "../disposable.h"
#include "../leak_observer.h"

#include <atomic>


namespace rx
{
class AtomicDisposable : public Disposable
{
public:
    explicit AtomicDisposable()
    {
        LeakObserver::make<AtomicDisposable>();
    }

    ~AtomicDisposable() override
    {
        LeakObserver::release<AtomicDisposable>();
    }

public:
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

#endif //RX_ATOMIC_DISPOSABLE_H