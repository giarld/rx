//
// Created by Gxin on 2026/1/4.
//

#ifndef RX_OBSERVABLE_SOURCE_H
#define RX_OBSERVABLE_SOURCE_H

#include "observer.h"


namespace rx
{
struct ObservableSource
{
    virtual ~ObservableSource() = default;

    virtual void subscribe(const ObserverPtr &observer) = 0;
};
} // rx

#endif //RX_OBSERVABLE_SOURCE_H