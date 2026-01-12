//
// Created by Gxin on 2026/1/12.
//

#ifndef RX_DISPOSABLE_HELPER_H
#define RX_DISPOSABLE_HELPER_H

#include "../disposable.h"
#include "gx/gmutex.h"


namespace rx
{
class DisposableHelper
{
public:
    static DisposablePtr disposed()
    {
        static auto instance = std::make_shared<DisposedState>();
        return instance;
    }

    static bool isDisposed(const DisposablePtr &d)
    {
        return d == disposed();
    }

    static bool set(DisposablePtr &field, const DisposablePtr &d, GMutex &lock)
    {
        DisposablePtr current;
        //
        {
            GLockerGuard g(lock);
            if (field == disposed()) {
            } else {
                current = field;
                field = d;
                goto SUCCESS;
            }
        }

        if (d) {
            d->dispose();
        }
        return false;

    SUCCESS:
        if (current) {
            current->dispose();
        }
        return true;
    }

    static bool setOnce(DisposablePtr &field, const DisposablePtr &d, GMutex &lock)
    {
        if (!d) {
            reportError("d is null in setOnce");
            return false;
        }
        //
        {
            GLockerGuard g(lock);
            if (field == nullptr) {
                field = d;
                return true;
            }
            if (field == disposed()) {
            } else {
                reportDisposableSet();
            }
        }

        d->dispose();
        return false;
    }

    static bool trySet(DisposablePtr &field, const DisposablePtr &d, GMutex &lock)
    {
        if (!d) { return false; }
        //
        {
            GLockerGuard g(lock);
            if (field == nullptr) {
                field = d;
                return true;
            }
            if (field == disposed()) {
                // disposed, destroy later
            }
        }

        d->dispose();
        return false;
    }

    static bool replace(DisposablePtr &field, const DisposablePtr &d, GMutex &lock)
    {
        //
        {
            GLockerGuard g(lock);
            if (field == disposed()) {
                // disposed, destroy later
            } else {
                field = d;
                return true;
            }
        }

        if (d) { d->dispose(); }
        return false;
    }

    static bool dispose(DisposablePtr &field, GMutex &lock)
    {
        DisposablePtr current;
        //
        {
            GLockerGuard g(lock);
            if (field == disposed()) {
                return false;
            }
            current = field;
            field = disposed();
        }

        if (current) {
            current->dispose();
        }
        return true;
    }

    static bool validate(const DisposablePtr &current, const DisposablePtr &next)
    {
        if (next == nullptr) {
            reportError("next is null in validate");
            return false;
        }
        if (current != nullptr) {
            next->dispose();
            reportDisposableSet();
            return false;
        }
        return true;
    }

    static void reportDisposableSet()
    {
        CHECK_CONDITION_S_V(false, "Disposable already set! (Protocol Violation)");
    }

private:
    static void reportError(const char *message)
    {
        CHECK_CONDITION_S_V(false, "Rx Error: {}", message);
    }

    class DisposedState : public Disposable
    {
    public:
        void dispose() override
        {
        }

        bool isDisposed() const override { return true; }
    };
};
} // rx

#endif //RX_DISPOSABLE_HELPER_H
