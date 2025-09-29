#include <memory>
#include <algorithm>
#include <iostream>
#include <type_traits>
#include "threadPool.hpp"
#include "connection.hpp"
#include "server.hpp"
#include "router.hpp"
#include "responder.hpp"
#include "logger.hpp"

void setupRoutingHandlers(router &currouter)
{
    currouter.addGetHandler("/",
                            [](std::unique_ptr<httpRequest> req, std::unique_ptr<httpResponse> res) -> std::unique_ptr<httpResponse>
                            {
                                logger::getInstance().logInfo("[Router] Handling GET request for URL: " + req->url_);

                                // Status line
                                res->version_ = "HTTP/1.1";
                                res->phrase_ = "OK";

                                // Headers
                                res->headers_["Content-Type"] = "text/plain; charset=utf-8";
                                std::string body = "yeah we did it";
                                res->body_ = body;
                                res->headers_["Content-Length"] = std::to_string(body.size());
                                res->headers_["Connection"] = "keep-alive"; // or "keep-alive" depending on your server

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

    logger::getInstance().logInfo("[Main] Queues initialized"); // logging added by gpt

    std::shared_ptr<connectionManager> curConnectionManagerPtr(std::make_shared<connectionManager>());

    responder curResponder(httpResQPtr, curConnectionManagerPtr);
    logger::getInstance().logInfo("[Main] Responder created"); // logging added by gpt

    std::jthread curResponderThread([&curResponder](std::stop_token stoken) mutable
                                    {
                                     logger::getInstance().logInfo("[Responder Thread] Started event loop"); // logging added by gpt
                                     curResponder.eventLoop(stoken); }); // router started

    threadPool curThreadPool(taskQPtr, 4);
    logger::getInstance().logInfo("[Main] ThreadPool created"); // logging added by gpt

    router curRouter(httpReqQPtr, taskQPtr, httpResQPtr);
    setupRoutingHandlers(curRouter);
    logger::getInstance().logInfo("[Main] Router created and GET handlers set up"); // logging added by gpt

    std::jthread curRouterThread([&curRouter](std::stop_token stoken) mutable
                                 {
         logger::getInstance().logInfo("[Router Thread] Started event loop"); // logging added by gpt
        curRouter.eventLoop(stoken); }); // router started

    httpServer curServer(curConnectionManagerPtr, httpReqQPtr);
    logger::getInstance().logInfo("[Main] HTTP Server created"); // logging added by gpt

    logger::getInstance().logInfo("[Main] Starting server event loop (blocking)"); // logging added by gpt
    curServer.eventLoop();                                                         // main thread will run server - server started

    return 0;
}
