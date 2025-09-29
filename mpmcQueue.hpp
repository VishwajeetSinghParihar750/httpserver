#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include "logger.hpp"

template <typename T>
class mpmcQueue // this might be a lock based or atmic or normal
{
public:
    virtual ~mpmcQueue() {}
    virtual void push(T) = 0;
    virtual T pop() = 0;
    virtual void stop() noexcept {}
    virtual size_t size() { return 0; }
};

//
template <typename T>
class lockedMpmcQueue : public mpmcQueue<T> //  not bounded, here might have to check  ℹ️ move , forwarding diff
{
    std::atomic<bool> stop_ = false;
    std::queue<T> q_;
    std::mutex mtx_;
    std::condition_variable notEmptyCv_;

public:
    void push(T obj) override
    {
        {
            std::unique_lock lck(mtx_);

            if (stop_.load())
            {
                logger::getInstance().logInfo("[lockedMpmcQueue][Thread " + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + "] push ignored, queue stopped"); // logging added by gpt
                return;
            }

            q_.push(std::move(obj));
            logger::getInstance().logInfo("[lockedMpmcQueue][Thread " + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + "] push done, queue size=" + std::to_string(q_.size())); // logging added by gpt
        }
        notEmptyCv_.notify_one();
    }

    T pop() override
    {
        T curTask;

        {
            std::unique_lock lck(mtx_);
            notEmptyCv_.wait(lck, [&]()
                             { return !q_.empty() || stop_.load(); });

            if (stop_.load())
            {
                logger::getInstance().logInfo("[lockedMpmcQueue][Thread " + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + "] pop returned empty, queue stopped"); // logging added by gpt
                return {};
            }

            curTask = std::move(q_.front());
            q_.pop();
            logger::getInstance().logInfo("[lockedMpmcQueue][Thread " + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + "] pop done, queue size=" + std::to_string(q_.size())); // logging added by gpt
        }
        return curTask;
    }

    size_t size() const noexcept
    {
        std::unique_lock lock(mtx_);
        return q_.size();
    }
    void stop() noexcept override
    {
        stop_.store(true);
        notEmptyCv_.notify_all();
        logger::getInstance().logInfo("[lockedMpmcQueue][Thread " + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + "] stop called, all waiting threads notified"); // logging added by gpt
    }
};
