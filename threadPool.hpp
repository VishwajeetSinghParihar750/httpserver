#pragma once
#include <memory>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <stop_token>
#include "mpmcQueue.hpp"
#include "task.hpp"
#include "logger.hpp"

class threadPool
{
    using taskQ = mpmcQueue<std::unique_ptr<task>>;
    using lockedTaskQ = lockedMpmcQueue<std::unique_ptr<task>>;

    std::shared_ptr<taskQ> q_;

    void eventloop(std::stop_token stoken)
    {
        logger::getInstance().logInfo("[threadPool] Worker thread started");
        while (!stoken.stop_requested())
        {
            auto curTask = q_->pop();
            if (curTask)
            {
                logger::getInstance().logInfo("[threadPool] Executing a task");
                curTask->execute();
                logger::getInstance().logInfo("[threadPool] Task execution finished");
            }
        }
        logger::getInstance().logInfo("[threadPool] Worker thread stopping");
    }

    std::vector<std::jthread> workers_;

public:
    threadPool(std::shared_ptr<taskQ> q, size_t nthreads = std::max(1u, std::thread::hardware_concurrency())) : q_(std::move(q))
    {
        logger::getInstance().logInfo("[threadPool] Initializing with " + std::to_string(nthreads) + " threads");
        for (int i = 0; i < nthreads; i++)
        {
            workers_.emplace_back([this](std::stop_token stoken)
                                  { eventloop(stoken); });
            logger::getInstance().logInfo("[threadPool] Worker thread " + std::to_string(i) + " created");
        }
    }

    threadPool() : threadPool(std::make_shared<lockedTaskQ>()) {}

    void addTask(std::unique_ptr<task> work)
    {
        logger::getInstance().logInfo("[threadPool] Adding a new task to the queue");
        q_->push(std::move(work));
    }

    threadPool(threadPool &&) = default;
    threadPool &operator=(threadPool &&) = default;

    ~threadPool()
    {
        logger::getInstance().logInfo("[threadPool] Stopping thread pool, signaling queue to stop");
        q_->stop();
    }
};
