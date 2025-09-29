#include <future>
#include <algorithm>
#include <iostream>
#include <type_traits>
#include "threadPool.hpp"
#include "clients.hpp"
#include "server.hpp"
#include "router.hpp"
#include "responder.hpp"
#include <memory>

void setupRoutingHandlers(router &currouter)
{
    currouter.addGetHandler("/",
                            [](std::unique_ptr<httpRequest> req, std::unique_ptr<httpResponse> res) -> std::unique_ptr<httpResponse>
                            {
                                std::cout << "[Router] Handling GET request for URL: " << req->url_ << std::endl;

                                // Status line
                                res->version_ = "HTTP/1.1";
                                res->phrase_ = "OK";

                                // Headers
                                res->headers_["Content-Type"] = "text/plain; charset=utf-8";
                                std::string body = "yeah we did it";
                                res->body_ = body;
                                res->headers_["Content-Length"] = std::to_string(body.size());
                                res->headers_["Connection"] = "close"; // or "keep-alive" depending on your server

                                return std::move(res);
                            });
}

int main()
{
    using mpmcHttpReqQ = mpmcQueue<std::unique_ptr<httpRequest>>;           //
    using lockMpmcHttpReqQ = lockedMpmcQueue<std::unique_ptr<httpRequest>>; //

    using mpmcTaskQ = mpmcQueue<std::unique_ptr<task>>;
    using lockMpmcTaskQ = lockedMpmcQueue<std::unique_ptr<task>>;

    using mpmcHttpResQ = mpmcQueue<std::unique_ptr<httpResponse>>;           //
    using lockMpmcHttpResQ = lockedMpmcQueue<std::unique_ptr<httpResponse>>; //

    std::shared_ptr<mpmcHttpReqQ> httpReqQPtr(std::make_shared<lockMpmcHttpReqQ>());
    std::shared_ptr<mpmcTaskQ> taskQPtr(std::make_shared<lockMpmcTaskQ>());
    std::shared_ptr<mpmcHttpResQ> httpResQPtr(std::make_shared<lockMpmcHttpResQ>());

    std::cout << "[Main] Queues initialized" << std::endl; // logging added by gpt

    responder curResponder(httpResQPtr);                  // sends back response
    std::cout << "[Main] Responder created" << std::endl; // logging added by gpt

    std::jthread curResponderThread([&curResponder](std::stop_token stoken) mutable
                                    { 
                                     std::cout << "[Responder Thread] Started event loop" << std::endl; // logging added by gpt
                                     curResponder.eventLoop(stoken); }); // router started

    threadPool curThreadPool(taskQPtr);                    // used by router / worker
    std::cout << "[Main] ThreadPool created" << std::endl; // logging added by gpt

    router curRouter(httpReqQPtr, taskQPtr, httpResQPtr);
    setupRoutingHandlers(curRouter);
    std::cout << "[Main] Router created and GET handlers set up" << std::endl; // logging added by gpt

    std::jthread curRouterThread([&curRouter](std::stop_token stoken) mutable
                                 { 
                                     std::cout << "[Router Thread] Started event loop" << std::endl; // logging added by gpt
                                     curRouter.eventLoop(stoken); }); // router started

    std::shared_ptr<clients> curClientsPtr(std::make_shared<clients>());
    httpServer curServer(curClientsPtr, httpReqQPtr);
    std::cout << "[Main] HTTP Server created" << std::endl; // logging added by gpt

    std::cout << "[Main] Starting server event loop (blocking)" << std::endl; // logging added by gpt
    curServer.eventLoop();                                                    // main thread will run server - server started

    return 0;
}
