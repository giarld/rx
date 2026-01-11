//
// Created by Gxin on 2026/1/11.
//

#ifndef RX_OBSERVABLE_BUFFER_H
#define RX_OBSERVABLE_BUFFER_H

#include "../observable.h"


namespace rx
{
class BufferExactObserver : public Observer, public Disposable, public std::enable_shared_from_this<BufferExactObserver>
{
public:
    explicit BufferExactObserver(const ObserverPtr &observer, uint64_t count)
        : mDownstream(observer), mCount(count)
    {
    }

    ~BufferExactObserver() override = default;

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        if (Disposable::validate(mUpstream.get(), d.get())) {
            mUpstream = d;
            mDownstream->onSubscribe(this->shared_from_this());
        }
    }

    void onNext(const GAny &value) override
    {
        mBuffer.push_back(value);
        if (mBuffer.size() >= mCount) {
            mDownstream->onNext(mBuffer);
            mBuffer.clear();
        }
    }

    void onError(const GAnyException &e) override
    {
        mBuffer.clear();
        mDownstream->onError(e);

        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    void onComplete() override
    {
        if (!mBuffer.empty()) {
            mDownstream->onNext(mBuffer);
        }
        mBuffer.clear();
        mDownstream->onComplete();

        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    void dispose() override
    {
        if (mUpstream) {
            mUpstream->dispose();
            mUpstream = nullptr;
        }
        mDownstream = nullptr;
    }

    bool isDisposed() const override
    {
        if (const auto d = mUpstream) {
            return d->isDisposed();
        }
        return true;
    }

private:
    ObserverPtr mDownstream;
    DisposablePtr mUpstream;
    uint64_t mCount;
    std::vector<GAny> mBuffer;
};

class BufferSkipObserver : public Observer, public Disposable, public std::enable_shared_from_this<BufferSkipObserver>
{
public:
    explicit BufferSkipObserver(const ObserverPtr &observer, uint64_t count, uint64_t skip)
        : mDownstream(observer), mCount(count), mSkip(skip)
    {
    }

    ~BufferSkipObserver() override = default;

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        if (Disposable::validate(mUpstream.get(), d.get())) {
            mUpstream = d;
            mDownstream->onSubscribe(this->shared_from_this());
        }
    }

    void onNext(const GAny &value) override
    {
        if (mIndex++ % mSkip == 0) {
            mBuffers.push_back({});
        }

        for (auto it = mBuffers.begin(); it != mBuffers.end();) {
            auto &buffer = *it;
            buffer.push_back(value);
            if (mCount <= buffer.size()) {
                mDownstream->onNext(buffer);
                it = mBuffers.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    void onError(const GAnyException &e) override
    {
        mBuffers.clear();
        mDownstream->onError(e);

        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    void onComplete() override
    {
        for (auto it = mBuffers.begin(); it != mBuffers.end(); ++it) {
            mDownstream->onNext(*it);
        }
        mBuffers.clear();
        mDownstream->onComplete();

        mDownstream = nullptr;
        mUpstream = nullptr;
    }

    void dispose() override
    {
        if (mUpstream) {
            mUpstream->dispose();
            mUpstream = nullptr;
        }
        mDownstream = nullptr;
    }

    bool isDisposed() const override
    {
        if (const auto d = mUpstream) {
            return d->isDisposed();
        }
        return true;
    }

private:
    ObserverPtr mDownstream;
    DisposablePtr mUpstream;
    uint64_t mCount;
    uint64_t mSkip;
    uint64_t mIndex = 0;
    std::vector<std::vector<GAny>> mBuffers;
};

class ObservableBuffer : public Observable
{
public:
    explicit ObservableBuffer(ObservableSourcePtr source, uint64_t count, uint64_t skip)
        : mSource(source), mCount(count), mSkip(skip)
    {
    }

    ~ObservableBuffer() override = default;

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        if (mSkip == mCount) {
            mSource->subscribe(std::make_shared<BufferExactObserver>(observer, mCount));
        } else {
            mSource->subscribe(std::make_shared<BufferSkipObserver>(observer, mCount, mSkip));
        }
    }

private:
    ObservableSourcePtr mSource;
    uint64_t mCount;
    uint64_t mSkip;
};
} // rx

#endif //RX_OBSERVABLE_BUFFER_H
