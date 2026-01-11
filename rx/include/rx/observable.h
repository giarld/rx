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
using ObservableOnSubscribe = std::function<void(const ObservableEmitterPtr &emitter)>;
using MapFunction = std::function<GAny(const GAny &x)>;

class GX_API Observable : public ObservableSource, public std::enable_shared_from_this<Observable>
{
public:
    ~Observable() override = default;

public:
    static std::shared_ptr<Observable> create(ObservableOnSubscribe source);

    static std::shared_ptr<Observable> empty();

    static std::shared_ptr<Observable> just(const GAny &value);

    static std::shared_ptr<Observable> just(const GAny &item1, const GAny &item2);

    static std::shared_ptr<Observable> just(const GAny &item1, const GAny &item2, const GAny &item3);

    static std::shared_ptr<Observable> just(const GAny &item1, const GAny &item2, const GAny &item3, const GAny &item4);

    static std::shared_ptr<Observable> just(const GAny &item1, const GAny &item2, const GAny &item3, const GAny &item4, const GAny &item5);

    static std::shared_ptr<Observable> just(const GAny &item1, const GAny &item2, const GAny &item3, const GAny &item4, const GAny &item5,
                                            const GAny &item6);

    static std::shared_ptr<Observable> just(const GAny &item1, const GAny &item2, const GAny &item3, const GAny &item4, const GAny &item5,
                                            const GAny &item6, const GAny &item7);

    static std::shared_ptr<Observable> just(const GAny &item1, const GAny &item2, const GAny &item3, const GAny &item4, const GAny &item5,
                                            const GAny &item6, const GAny &item7, const GAny &item8);

    static std::shared_ptr<Observable> just(const GAny &item1, const GAny &item2, const GAny &item3, const GAny &item4, const GAny &item5,
                                            const GAny &item6, const GAny &item7, const GAny &item8, const GAny &item9);

    static std::shared_ptr<Observable> just(const GAny &item1, const GAny &item2, const GAny &item3, const GAny &item4, const GAny &item5,
                                            const GAny &item6, const GAny &item7, const GAny &item8, const GAny &item9, const GAny &item10);

    static std::shared_ptr<Observable> fromArray(const std::vector<GAny> &array);

    static std::shared_ptr<Observable> never();

    static std::shared_ptr<Observable> error(const GAnyException &e);

    static std::shared_ptr<Observable> defer(const ObservableSourcePtr &source);

    static std::shared_ptr<Observable> interval(uint64_t delay, uint64_t interval);

    static std::shared_ptr<Observable> timer(uint64_t delay);

    static std::shared_ptr<Observable> range(int64_t start, uint64_t count);


    std::shared_ptr<Observable> map(MapFunction function);

    std::shared_ptr<Observable> buffer(uint64_t count, uint64_t skip);

    std::shared_ptr<Observable> buffer(uint64_t count);


    std::shared_ptr<Observable> subscribeOn(SchedulerPtr scheduler);

    std::shared_ptr<Observable> observeOn(SchedulerPtr scheduler);

public:
    void subscribe(const ObserverPtr &observer) override;

    DisposablePtr subscribe(const OnNextAction &next, const OnErrorAction &error, const OnCompleteAction &complete);

    DisposablePtr subscribe(const OnNextAction &next)
    {
        return subscribe(next, nullptr, nullptr);
    }

protected:
    virtual void subscribeActual(const ObserverPtr &observer) = 0;
};
} // rx

#endif //RX_OBSERVABLE_H
