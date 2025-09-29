#pragma once
#include <stop_token>
#include <sstream>
#include <deque>
#include <memory>
#include "mpmcQueue.hpp"
#include "fdRaii.hpp"
#include "response.hpp"
#include "epollUtils.hpp"
#include "logger.hpp"
#include "connection.hpp"

class responder
{

    using mpmcResponseQ = mpmcQueue<std::unique_ptr<httpResponse>>; //

    std::shared_ptr<mpmcResponseQ> httpResponseQ_;

    std::shared_ptr<connectionManager> connectionManager_;
    fdRaii efd_ = -1; // epolll fd

public:
    responder(std::shared_ptr<mpmcResponseQ> httpResponseQ, std::shared_ptr<connectionManager> connectionManager) : httpResponseQ_(httpResponseQ), connectionManager_(connectionManager)
    {
        if ((efd_ = epoll_create1(0)) == -1)
            perror("responser efd"), exit(-1);

        logger::getInstance().logInfo("[responder] Responder initialized");
    }

    // void epollEventLoop(std::stop_token stoken)
    // {
    //     const int MAX_EPOLL_EVENTS = 1024;
    //     epoll_event ev, events[MAX_EPOLL_EVENTS];
    //     while (!stoken.stop_requested())
    //     {
    //         int nfds = epoll_wait(efd_, events, MAX_EPOLL_EVENTS, -1);
    //         if (nfds == -1)
    //             perror("responder epoll event loop "), exit(-1);

    //         for (int i = 0; i < nfds; i++)
    //         {
    //             writeBackResponse(events[i].data.fd);
    //         }
    //     }
    // }

    void eventLoop(std::stop_token stoken) // ℹ️ℹ️ provide all event loops noexcept excpetion gaurantee [ do it actually, or add try catch if cant ]
    {

        // std::jthread epollListener([&](std::stop_token stoken) mutable
        //                            { epollEventLoop(stoken); });

        logger::getInstance().logInfo("[responder] Event loop started");
        while (!stoken.stop_requested())
        {
            std::unique_ptr<httpResponse> res = httpResponseQ_->pop();

            auto conid = res->getConnectionId();

            if (connectionManager_->writeResponse(conid, *res) == false)
            {

                // for now just disconnect
                connectionManager_->removeConnection(conid);
            }
            else
            {
            }
        }
        logger::getInstance().logInfo("[responder] Event loop stopped");
    }
};
