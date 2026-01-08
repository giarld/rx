//
// Created by Gxin on 2026/1/4.
//

#include "rx/observable.h"
#include "rx/operators/observable_create.h"
#include "rx/operators/observable_just.h"
#include "rx/operators/observable_empty.h"
#include "rx/operators/observable_map.h"
#include "rx/operators/observable_subscribe_on.h"


namespace rx
{
std::shared_ptr<Observable> Observable::create(ObservableOnSubscribe source)
{
    return std::make_shared<ObservableCreate>(std::move(source));
}

std::shared_ptr<Observable> Observable::just(const GAny &value)
{
    return std::make_shared<ObservableJust>(value);
}

std::shared_ptr<Observable> Observable::empty()
{
    return std::make_shared<ObservableEmpty>();
}


std::shared_ptr<Observable> Observable::map(MapFunction function)
{
    return std::make_shared<ObservableMap>(this->shared_from_this(), std::move(function));
}


std::shared_ptr<Observable> Observable::subscribeOn(SchedulerPtr scheduler)
{
    return std::make_shared<ObservableSubscribeOn>(this->shared_from_this(), scheduler);
}


void Observable::subscribe(const ObserverPtr &observer)
{
    subscribeActual(observer);
}

DisposablePtr Observable::subscribe(const OnNextAction &next, const OnErrorAction &error, const OnCompleteAction &complete)
{
    auto observer = std::make_shared<LambdaObserver>(next, error, complete, nullptr);
    subscribe(observer);

    return observer;
}
} // rx