#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <iostream> // logging added by gpt
#include <thread>

template <typename T>
class mpmcQueue // this might be a lock based or atmic or normal
{
public:
    virtual ~mpmcQueue() {}
    virtual void push(T) = 0;
    virtual T pop() = 0;
    virtual void stop() noexcept {}
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
                std::cout << "[lockedMpmcQueue][Thread "
                          << std::this_thread::get_id()
                          << "] push ignored, queue stopped" << std::endl; // logging added by gpt
                return;
            }

            q_.push(std::move(obj));
            std::cout << "[lockedMpmcQueue][Thread "
                      << std::this_thread::get_id()
                      << "] push done, queue size=" << q_.size() << std::endl; // logging added by gpt
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
                std::cout << "[lockedMpmcQueue][Thread "
                          << std::this_thread::get_id()
                          << "] pop returned empty, queue stopped" << std::endl; // logging added by gpt
                return {};
            }

            curTask = std::move(q_.front());
            q_.pop();
            std::cout << "[lockedMpmcQueue][Thread "
                      << std::this_thread::get_id()
                      << "] pop done, queue size=" << q_.size() << std::endl; // logging added by gpt
        }
        return curTask;
    }

    void stop() noexcept override
    {
        stop_.store(true);
        notEmptyCv_.notify_all();
        std::cout << "[lockedMpmcQueue][Thread "
                  << std::this_thread::get_id()
                  << "] stop called, all waiting threads notified" << std::endl; // logging added by gpt
    }
};
