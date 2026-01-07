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
using MapFunction = std::function<GAny(const GAny &x)>;

class GX_API Observable : public ObservableSource, public std::enable_shared_from_this<Observable>
{
public:
    ~Observable() override = default;

public:
    static std::shared_ptr<Observable> create(ObservableOnSubscribe source);

    static std::shared_ptr<Observable> just(const GAny &value);

    static std::shared_ptr<Observable> empty();


    std::shared_ptr<Observable> map(MapFunction function);

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
