//
// Created by Gxin on 2026/1/4.
//

#include "rx/observable.h"

#include "rx/operators/observable_buffer.h"
#include "rx/operators/observable_create.h"
#include "rx/operators/observable_defer.h"
#include "rx/operators/observable_delay.h"
#include "rx/operators/observable_element_at.h"
#include "rx/operators/observable_empty.h"
#include "rx/operators/observable_error.h"
#include "rx/operators/observable_filter.h"
#include "rx/operators/observable_flat_map.h"
#include "rx/operators/observable_from_array.h"
#include "rx/operators/observable_ignore_elements.h"
#include "rx/operators/observable_interval.h"
#include "rx/operators/observable_just.h"
#include "rx/operators/observable_last.h"
#include "rx/operators/observable_map.h"
#include "rx/operators/observable_never.h"
#include "rx/operators/observable_observe_on.h"
#include "rx/operators/observable_range.h"
#include "rx/operators/observable_repeat.h"
#include "rx/operators/observable_scan.h"
#include "rx/operators/observable_subscribe_on.h"
#include "rx/operators/observable_timer.h"
#include "rx/schedulers/main_thread_scheduler.h"


namespace rx
{
std::shared_ptr<Observable> Observable::create(ObservableOnSubscribe source)
{
    return std::make_shared<ObservableCreate>(std::move(source));
}

std::shared_ptr<Observable> Observable::empty()
{
    return std::make_shared<ObservableEmpty>();
}

std::shared_ptr<Observable> Observable::just(const GAny &value)
{
    return std::make_shared<ObservableJust>(value);
}

std::shared_ptr<Observable> Observable::just(const GAny &item1, const GAny &item2)
{
    return fromArray({item1, item2});
}

std::shared_ptr<Observable> Observable::just(const GAny &item1, const GAny &item2, const GAny &item3)
{
    return fromArray({item1, item2, item3});
}

std::shared_ptr<Observable> Observable::just(const GAny &item1, const GAny &item2, const GAny &item3, const GAny &item4)
{
    return fromArray({item1, item2, item3, item4});
}

std::shared_ptr<Observable> Observable::just(const GAny &item1, const GAny &item2, const GAny &item3, const GAny &item4, const GAny &item5)
{
    return fromArray({item1, item2, item3, item4, item5});
}

std::shared_ptr<Observable> Observable::just(const GAny &item1, const GAny &item2, const GAny &item3, const GAny &item4, const GAny &item5, const GAny &item6)
{
    return fromArray({item1, item2, item3, item4, item5, item6});
}

std::shared_ptr<Observable> Observable::just(const GAny &item1, const GAny &item2, const GAny &item3, const GAny &item4, const GAny &item5, const GAny &item6, const GAny &item7)
{
    return fromArray({item1, item2, item3, item4, item5, item6, item7});
}

std::shared_ptr<Observable> Observable::just(const GAny &item1, const GAny &item2, const GAny &item3, const GAny &item4, const GAny &item5,
                                             const GAny &item6, const GAny &item7, const GAny &item8)
{
    return fromArray({item1, item2, item3, item4, item5, item6, item7, item8});
}

std::shared_ptr<Observable> Observable::just(const GAny &item1, const GAny &item2, const GAny &item3, const GAny &item4, const GAny &item5, const GAny &item6, const GAny &item7,
                                             const GAny &item8, const GAny &item9)
{
    return fromArray({item1, item2, item3, item4, item5, item6, item7, item8, item9});
}

std::shared_ptr<Observable> Observable::just(const GAny &item1, const GAny &item2, const GAny &item3, const GAny &item4, const GAny &item5,
                                             const GAny &item6, const GAny &item7, const GAny &item8, const GAny &item9, const GAny &item10)
{
    return fromArray({item1, item2, item3, item4, item5, item6, item7, item8, item9, item10});
}

std::shared_ptr<Observable> Observable::fromArray(const std::vector<GAny> &array)
{
    return std::make_shared<ObservableFromArray>(array);
}

std::shared_ptr<Observable> Observable::never()
{
    return ObservableNever::instance();
}

std::shared_ptr<Observable> Observable::error(const GAnyException &e)
{
    return std::make_shared<ObservableError>(e);
}

std::shared_ptr<Observable> Observable::defer(const ObservableSourcePtr &source)
{
    return std::make_shared<ObservableDefer>(source);
}

std::shared_ptr<Observable> Observable::interval(uint64_t delay, uint64_t interval)
{
    return std::make_shared<ObservableInterval>(delay, interval);
}

std::shared_ptr<Observable> Observable::timer(uint64_t delay)
{
    return std::make_shared<ObservableTimer>(delay);
}

std::shared_ptr<Observable> Observable::range(int64_t start, uint64_t count)
{
    if (count == 0) {
        return empty();
    }

    if (count == 1) {
        return just(start);
    }

    if (start + (count - 1) > std::numeric_limits<int64_t>::max()) {
        throw GAnyException("Integer overflow");
    }

    return std::make_shared<ObservableRange>(start, count);
}

std::shared_ptr<Observable> Observable::fromCallable(const Callable &callable)
{
    return create([callable](const ObservableEmitterPtr &emitter) {
        GAny r;
        try {
            r = callable();
        } catch (const std::exception &e) {
            if (!emitter->isDisposed()) {
                emitter->onError(GAnyException(e.what()));
            }
        }
        if (!emitter->isDisposed()) {
            emitter->onNext(r);
            emitter->onComplete();
        }
    });
}


std::shared_ptr<Observable> Observable::map(const MapFunction &function)
{
    return std::make_shared<ObservableMap>(this->shared_from_this(), function);
}

std::shared_ptr<Observable> Observable::flatMap(const FlatMapFunction &function)
{
    return std::make_shared<ObservableFlatMap>(this->shared_from_this(), function);
}

std::shared_ptr<Observable> Observable::buffer(uint64_t count, uint64_t skip)
{
    return std::make_shared<ObservableBuffer>(this->shared_from_this(), count, skip);
}

std::shared_ptr<Observable> Observable::buffer(uint64_t count)
{
    return buffer(count, count);
}

std::shared_ptr<Observable> Observable::repeat(uint64_t times)
{
    if (times == 0) {
        return empty();
    }
    return std::make_shared<ObservableRepeat>(this->shared_from_this(), times);
}

std::shared_ptr<Observable> Observable::scan(const BiFunction &accumulator)
{
    return std::make_shared<ObservableScan>(this->shared_from_this(), accumulator);
}


std::shared_ptr<Observable> Observable::filter(const FilterFunction &filter)
{
    return std::make_shared<ObservableFilter>(this->shared_from_this(), filter);
}

std::shared_ptr<Observable> Observable::elementAt(uint64_t index)
{
    return std::make_shared<ObservableElementAt>(this->shared_from_this(), index, GAny(), false);
}

std::shared_ptr<Observable> Observable::elementAt(uint64_t index, const GAny &defaultValue)
{
    return std::make_shared<ObservableElementAt>(this->shared_from_this(), index, defaultValue, true);
}

std::shared_ptr<Observable> Observable::first()
{
    return elementAt(0);
}

std::shared_ptr<Observable> Observable::first(const GAny &defaultValue)
{
    return elementAt(0, defaultValue);
}

std::shared_ptr<Observable> Observable::last()
{
    return std::make_shared<ObservableLast>(this->shared_from_this(), GAny(), false);
}

std::shared_ptr<Observable> Observable::last(const GAny &defaultValue)
{
    return std::make_shared<ObservableLast>(this->shared_from_this(), defaultValue, true);
}

std::shared_ptr<Observable> Observable::ignoreElements()
{
    return std::make_shared<ObservableIgnoreElements>(this->shared_from_this());
}

std::shared_ptr<Observable> Observable::delay(uint64_t delay, SchedulerPtr scheduler)
{
    if (!scheduler) {
        scheduler = MainThreadScheduler::create();
    }
    return std::make_shared<ObservableDelay>(this->shared_from_this(), delay, scheduler);
}


std::shared_ptr<Observable> Observable::subscribeOn(SchedulerPtr scheduler)
{
    return std::make_shared<ObservableSubscribeOn>(this->shared_from_this(), scheduler);
}

std::shared_ptr<Observable> Observable::observeOn(SchedulerPtr scheduler)
{
    return std::make_shared<ObservableObserveOn>(this->shared_from_this(), scheduler);
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
