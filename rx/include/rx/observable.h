//
// Created by Gxin on 2026/1/4.
//

#ifndef RX_OBSERVABLE_H
#define RX_OBSERVABLE_H

#include "observer.h"
#include "observable_source.h"
#include "emitter.h"
#include "scheduler.h"


namespace rx
{
class Observable;

using ObservableOnSubscribe = std::function<void(const ObservableEmitterPtr &emitter)>;
using MapFunction = std::function<GAny(const GAny &x)>;
using FlatMapFunction = std::function<std::shared_ptr<Observable>(const GAny &v)>;
using FilterFunction = std::function<bool(const GAny &v)>;
using Callable = std::function<GAny()>;
using BiFunction = std::function<GAny(const GAny &last, const GAny &item)>;
using CombineLatestFunction = std::function<GAny(const std::vector<GAny> &values)>;

class GX_API Observable : public ObservableSource, public std::enable_shared_from_this<Observable>
{
public:
    ~Observable() override = default;

public:
    static std::shared_ptr<Observable> create(ObservableOnSubscribe source);

    static std::shared_ptr<Observable> empty();

    static std::shared_ptr<Observable> fromArray(const std::vector<GAny> &array);

    template<typename... Args>
    static std::shared_ptr<Observable> just(Args &&... sources)
    {
        if constexpr (sizeof...(Args) == 1) {
            return justOne(std::forward<Args>(sources)...);
        }
        return fromArray({std::forward<Args>(sources)...});
    }

    static std::shared_ptr<Observable> never();

    static std::shared_ptr<Observable> error(const GAnyException &e);

    static std::shared_ptr<Observable> defer(const ObservableSourcePtr &source);

    static std::shared_ptr<Observable> interval(uint64_t delay, uint64_t interval);

    static std::shared_ptr<Observable> timer(uint64_t delay);

    static std::shared_ptr<Observable> range(int64_t start, uint64_t count);

    static std::shared_ptr<Observable> combineLatestArray(const std::vector<std::shared_ptr<Observable> > &sources,
                                                          const CombineLatestFunction &combiner);

    static std::shared_ptr<Observable> combineLatest(const std::shared_ptr<Observable> &source1, const std::shared_ptr<Observable> &source2,
                                                     const BiFunction &combiner);

    static std::shared_ptr<Observable> fromCallable(const Callable &callable);

    static std::shared_ptr<Observable> merge(const std::shared_ptr<Observable> &source);

    static std::shared_ptr<Observable> mergeArray(const std::vector<std::shared_ptr<Observable> > &sources);

    template<typename... Args>
    static std::shared_ptr<Observable> merge(Args &&... sources)
    {
        return mergeArray({std::forward<Args>(sources)...});
    }

    static std::shared_ptr<Observable> zipArray(const std::vector<std::shared_ptr<Observable> > &sources,
                                                const CombineLatestFunction &zipper);

    static std::shared_ptr<Observable> zip(const std::shared_ptr<Observable> &source1,
                                           const std::shared_ptr<Observable> &source2,
                                           const BiFunction &zipper);


    std::shared_ptr<Observable> map(const MapFunction &function);

    std::shared_ptr<Observable> flatMap(const FlatMapFunction &function);

    std::shared_ptr<Observable> buffer(uint64_t count, uint64_t skip);

    std::shared_ptr<Observable> buffer(uint64_t count);

    std::shared_ptr<Observable> repeat(uint64_t times);

    std::shared_ptr<Observable> scan(const BiFunction &accumulator);


    std::shared_ptr<Observable> filter(const FilterFunction &filter);

    std::shared_ptr<Observable> elementAt(uint64_t index);

    std::shared_ptr<Observable> elementAt(uint64_t index, const GAny &defaultValue);

    std::shared_ptr<Observable> first();

    std::shared_ptr<Observable> first(const GAny &defaultValue);

    std::shared_ptr<Observable> last();

    std::shared_ptr<Observable> last(const GAny &defaultValue);

    std::shared_ptr<Observable> ignoreElements();

    std::shared_ptr<Observable> skip(uint64_t count);

    std::shared_ptr<Observable> skipLast(uint64_t count);

    std::shared_ptr<Observable> take(uint64_t count);

    std::shared_ptr<Observable> takeLast(uint64_t count);

    std::shared_ptr<Observable> delay(uint64_t delay, SchedulerPtr scheduler = nullptr);

    std::shared_ptr<Observable> debounce(uint64_t delay, SchedulerPtr scheduler = nullptr);

    std::shared_ptr<Observable> join(const std::shared_ptr<Observable> &other,
                                     const FlatMapFunction &leftDurationSelector,
                                     const FlatMapFunction &rightDurationSelector,
                                     const BiFunction &resultSelector);

    std::shared_ptr<Observable> startWith(const GAny &item);

    std::shared_ptr<Observable> startWithArray(const std::vector<GAny> &items);

    template<typename... Args>
    std::shared_ptr<Observable> startWith(Args &&... items)
    {
        return startWithArray({std::forward<Args>(items)...});
    }


    std::shared_ptr<Observable> subscribeOn(SchedulerPtr scheduler);

    std::shared_ptr<Observable> observeOn(SchedulerPtr scheduler);


    GAny blockingFirst();

    GAny blockingFirst(const GAny &defaultValue);

    GAny blockingLast();

    GAny blockingLast(const GAny &defaultValue);

    void blockingForEach(const OnNextAction& onNext);

public:
    void subscribe(const ObserverPtr &observer) override;

    DisposablePtr subscribe(const OnNextAction &next, const OnErrorAction &error, const OnCompleteAction &complete);

    DisposablePtr subscribe(const OnNextAction &next)
    {
        return subscribe(next, nullptr, nullptr);
    }

private:
    static std::shared_ptr<Observable> justOne(const GAny &value);

protected:
    virtual void subscribeActual(const ObserverPtr &observer) = 0;
};
} // rx

#endif //RX_OBSERVABLE_H
