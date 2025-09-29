#pragma once
#include <memory>
#include <functional>
#include "mpmcQueue.hpp"
#include "request.hpp"
#include "response.hpp"
#include "task.hpp"
#include "logger.hpp"

// can make crazy routing based on adding handlers for differt urls etc
class router
{
    using mpmcRequestQ = mpmcQueue<std::unique_ptr<httpRequest>>; //
    using mpmcTaskQ = mpmcQueue<std::unique_ptr<task>>;
    using mpmcResponseQ = mpmcQueue<std::unique_ptr<httpResponse>>; //
    using handlerFn = std::function<std::unique_ptr<httpResponse>(std::unique_ptr<httpRequest>, std::unique_ptr<httpResponse>)>;

    std::shared_ptr<mpmcRequestQ> httpRequestsQ_;
    std::shared_ptr<mpmcTaskQ> httpTasksQ_;
    std::shared_ptr<mpmcResponseQ> httpResponseQ_;

    std::unordered_map<std::string, handlerFn>
        getHandlers_; // takes req, empty response , returns response

    void processRequest(std::unique_ptr<httpRequest> req)
    {
        if (req->method_ == "GET")
        {
            logger::getInstance().logInfo("[router] recieved GET request for URL: " + req->url_ + " " + std::to_string(getHandlers_.contains(req->url_)));

            if (getHandlers_.contains(req->url_))
            {
                logger::getInstance().logInfo("[router] Processing GET request for URL: " + req->url_);

                callableTask curTask = [req = std::move(req), this]() mutable
                {
                    logger::getInstance().logInfo("[TASK] Entered task ");

                    std::string url = req->url_;
                    logger::getInstance().logInfo("[TASK] Executing handler for URL: " + url);

                    auto res = std::make_unique<httpResponse>(*req);
                    auto result = getHandlers_[url](std::move(req), std::move(res));

                    logger::getInstance().logInfo("[TASK] Handler execution finished, pushing response to responseQ");
                    httpResponseQ_->push(std::move(result));
                };

                httpTasksQ_->push(std::make_unique<decltype(curTask)>(std::move(curTask)));
                logger::getInstance().logInfo("[router] Task created and pushed to taskQ");
            }
            else
            {
                logger::getInstance().logInfo("[router] Unsupported get request : " + req->method_);
                // dont deal rn
            }
        }
        else
        {
            logger::getInstance().logInfo("[router] Unsupported method: " + req->method_);
            // dont deal rn
        }
    }

public:
    router(std::shared_ptr<mpmcRequestQ> reqQ, std::shared_ptr<mpmcTaskQ> taskQ, std::shared_ptr<mpmcResponseQ> resQ)
        : httpRequestsQ_(reqQ), httpTasksQ_(taskQ), httpResponseQ_(resQ)
    {
        logger::getInstance().logInfo("[router] Router initialized");
    }

    bool addGetHandler(std::string url, handlerFn fn)
    {
        bool inserted = getHandlers_.try_emplace(url, fn).second;
        logger::getInstance().logInfo("[router] addGetHandler for URL: " + url + (inserted ? " (added)" : " (already exists)"));
        return inserted;
    }

    void eventLoop(std::stop_token stoken)
    {

        logger::getInstance().logInfo("[router] Event loop started");
        while (!stoken.stop_requested())
        {
            auto req = httpRequestsQ_->pop();
            logger::getInstance().logInfo("[router] Request popped from requestQ");
            processRequest(std::move(req));
        }
        logger::getInstance().logInfo("[router] Event loop stopped");
    }
};
