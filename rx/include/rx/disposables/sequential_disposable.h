//
// Created by Gxin on 2026/1/12.
//

#ifndef RX_SEQUENTIAL_DISPOSABLE_H
#define RX_SEQUENTIAL_DISPOSABLE_H

#include "../disposables/atomic_disposable.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"
#include <memory>


namespace rx
{
class SequentialDisposable : public Disposable
{
public:
    explicit SequentialDisposable()
        : mDisposable(std::make_shared<AtomicDisposable>())
    {
        LeakObserver::make<SequentialDisposable>();
    }

    explicit SequentialDisposable(const DisposablePtr &initial)
        : mDisposable(initial)
    {
        LeakObserver::make<SequentialDisposable>();
    }

    ~SequentialDisposable() override
    {
        LeakObserver::release<SequentialDisposable>();
    }

public:
    bool update(const DisposablePtr &next)
    {
        return DisposableHelper::set(mDisposable, next, mLock);
    }

    bool replace(const DisposablePtr &next)
    {
        return DisposableHelper::replace(mDisposable, next, mLock);
    }

    void dispose() override
    {
        DisposableHelper::dispose(mDisposable, mLock);
    }

    bool isDisposed() const override
    {
        return DisposableHelper::isDisposed(mDisposable);
    }

private:
    DisposablePtr mDisposable;
    GMutex mLock;
};

using SequentialDisposablePtr = std::shared_ptr<SequentialDisposable>;
} // rx

#endif //RX_SEQUENTIAL_DISPOSABLE_H
