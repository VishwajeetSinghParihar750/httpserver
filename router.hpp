#pragma once
#include <memory>
#include <functional>
#include <iostream> // logging added by gpt

#include "mpmcQueue.hpp"
#include "request.hpp"
#include "response.hpp"
#include "task.hpp"

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
            std::cout << "[router] recieved GET request for URL: " << req->url_ << " " << getHandlers_.contains(req->url_) << std::endl; // logging added by gpt

            if (getHandlers_.contains(req->url_))
            {
                std::cout << "[router] Processing GET request for URL: " << req->url_ << std::endl; // logging added by gpt

                callableTask curTask = [req = std::move(req), this]() mutable
                {
                    std::cout << "[TASK] Entered task " << std::endl;

                    std::string url = req->url_;
                    std::cout << "[TASK] Executing handler for URL: " << url << std::endl;

                    auto res = std::make_unique<httpResponse>(*req);
                    auto result = getHandlers_[url](std::move(req), std::move(res));

                    std::cout << "[TASK] Handler execution finished, pushing response to responseQ" << std::endl;
                    httpResponseQ_->push(std::move(result));
                };

                httpTasksQ_->push(std::make_unique<decltype(curTask)>(std::move(curTask)));
                std::cout << "[router] Task created and pushed to taskQ" << std::endl; // logging added by gpt
            }

            else
            {
                std::cout << "[router] Unsupported get request : " << req->method_ << std::endl; // logging added by gpt
                // dont deal rn
            }
        }
        else
        {
            std::cout << "[router] Unsupported method: " << req->method_ << std::endl; // logging added by gpt
                                                                                       // dont deal rn
        }
    }

public:
    router(std::shared_ptr<mpmcRequestQ> reqQ, std::shared_ptr<mpmcTaskQ> taskQ, std::shared_ptr<mpmcResponseQ> resQ)
        : httpRequestsQ_(reqQ), httpTasksQ_(taskQ), httpResponseQ_(resQ)
    {
        std::cout << "[router] Router initialized" << std::endl; // logging added by gpt
    }

    bool addGetHandler(std::string url, handlerFn fn)
    {
        bool inserted = getHandlers_.try_emplace(url, fn).second;
        std::cout << "[router] addGetHandler for URL: " << url << (inserted ? " (added)" : " (already exists)") << std::endl; // logging added by gpt
        return inserted;
    }

    void eventLoop(std::stop_token stoken)
    {
        std::cout << "[router] Event loop started" << std::endl; // logging added by gpt
        while (!stoken.stop_requested())
        {
            auto req = httpRequestsQ_->pop();
            std::cout << "[router] Request popped from requestQ" << std::endl; // logging added by gpt
            processRequest(std::move(req));
        }
        std::cout << "[router] Event loop stopped" << std::endl; // logging added by gpt
    }
};
