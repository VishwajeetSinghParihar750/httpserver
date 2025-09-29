#pragma once
#include <memory>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <stop_token>
#include <iostream> // logging added by gpt
#include "mpmcQueue.hpp"
#include "task.hpp"
//
class threadPool
{
    using taskQ = mpmcQueue<std::unique_ptr<task>>;
    using lockedTaskQ = lockedMpmcQueue<std::unique_ptr<task>>;

    std::shared_ptr<taskQ> q_;

    void eventloop(std::stop_token stoken)
    {
        std::cout << "[threadPool] Worker thread started" << std::endl; // logging added by gpt
        while (!stoken.stop_requested())
        {
            auto curTask = q_->pop();
            if (curTask)
            {
                std::cout << "[threadPool] Executing a task" << std::endl; // logging added by gpt
                curTask->execute();
                std::cout << "[threadPool] Task execution finished" << std::endl; // logging added by gpt
            }
        }
        std::cout << "[threadPool] Worker thread stopping" << std::endl; // logging added by gpt
    }

    std::vector<std::jthread> workers_;

public:
    //
    threadPool(std::shared_ptr<taskQ> q, size_t nthreads = std::max(1u, std::thread::hardware_concurrency())) : q_(std::move(q))
    {
        std::cout << "[threadPool] Initializing with " << nthreads << " threads" << std::endl; // logging added by gpt
        for (int i = 0; i < nthreads; i++)
        {
            workers_.emplace_back([this](std::stop_token stoken)
                                  { eventloop(stoken); });
            std::cout << "[threadPool] Worker thread " << i << " created" << std::endl; // logging added by gpt
        }
    }

    threadPool() : threadPool(std::make_shared<lockedTaskQ>()) {}

    void addTask(std::unique_ptr<task> work)
    {
        std::cout << "[threadPool] Adding a new task to the queue" << std::endl; // logging added by gpt
        q_->push(std::move(work));
    }

    //
    threadPool(threadPool &&) = default;
    threadPool &operator=(threadPool &&) = default;

    ~threadPool()
    {
        std::cout << "[threadPool] Stopping thread pool, signaling queue to stop" << std::endl; // logging added by gpt
        q_->stop();
    }
};
