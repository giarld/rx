//
// Created by Gxin on 2026/1/4.
//

#ifndef RX_OBSERVABLE_H
#define RX_OBSERVABLE_H

#include "observer.h"
#include "observable_source.h"
#include "emitter.h"


namespace rx
{
using ObservableOnSubscribe = std::function<void(const ObservableEmitterPtr &emitter)>;

class Observable : public ObservableSource
{
public:
    ~Observable() override = default;

public:
    virtual void subscribeActual(const ObserverPtr &observer) = 0;
};
} // rx

#endif //RX_OBSERVABLE_H
