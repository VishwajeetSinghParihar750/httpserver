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

class responder
{

    using mpmcResponseQ = mpmcQueue<std::unique_ptr<httpResponse>>; //

    std::shared_ptr<mpmcResponseQ> httpResponseQ_;

    std::shared_ptr<clients> clients_;
    fdRaii efd_ = -1; // epolll fd

    void addResponse(std::shared_ptr<httpResponse> resp)
    {
        std::shared_ptr<httpResponseCreator> respCreatorPtr = clients_->getClientsHttpResponseCreator(resp->getSendTo(), resp->getClientId());

        if (respCreatorPtr == nullptr)
        {
            logger::getInstance().logError("[responder] client has been removed ");
        }
        else
        {
            respCreatorPtr->addResponse(*resp);
        }
    }

    void writeBackResponse(int sendTo, auto clientId)
    {

        std::shared_ptr<httpResponseCreator>
            respCreatorPtr = clients_->getClientsHttpResponseCreator(sendTo, clientId);

        if (respCreatorPtr == nullptr)
        {
            logger::getInstance().logError("[responder] client has been removed ");
        }
        else
        {
            if (!respCreatorPtr->write())
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    if (epollUtils::watchEpollEtFd(efd_, sendTo, EPOLLONESHOT | EPOLLOUT) == -1)
                        logger::getInstance().logError("epoll watch fd "), exit(-1);
            }
        }
    }

public:
    responder(std::shared_ptr<mpmcResponseQ> httpResponseQ, std::shared_ptr<clients> clients) : httpResponseQ_(httpResponseQ), clients_(clients)
    {
        if ((efd_ = epoll_create1(0)) == -1)
            perror("responser efd"), exit(-1);

        logger::getInstance().logInfo("[responder] Responder initialized");
    }

    void epollEventLoop(std::stop_token stoken)
    {
        const int MAX_EPOLL_EVENTS = 1024;
        epoll_event ev, events[MAX_EPOLL_EVENTS];
        while (!stoken.stop_requested())
        {
            int nfds = epoll_wait(efd_, events, MAX_EPOLL_EVENTS, -1);
            if (nfds == -1)
                logger::getInstance().logError("responder epoll event loop "), exit(-1);

            for (int i = 0; i < nfds; i++)
            {
                writeBackResponse(events[i].data.fd, clients_->getClientIdFromConnFd(events[i].data.fd)); // 💀💀💀  RACE CONDITION
            }
        }
    }

    void eventLoop(std::stop_token stoken) // ℹ️ℹ️ provide all event loops noexcept excpetion gaurantee [ do it actually, or add try catch if cant ]
    {

        std::jthread epollListener([&](std::stop_token stoken) mutable
                                   { epollEventLoop(stoken); });

        logger::getInstance().logInfo("[responder] Event loop started");
        while (!stoken.stop_requested())
        {
            auto res = httpResponseQ_->pop();
            logger::getInstance().logInfo("[responder] Response popped from queue for fd=" + std::to_string(res->getSendTo()));

            int sendTo = res->getSendTo();
            auto clientId = res->getClientId();
            addResponse(std::move(res));
            writeBackResponse(sendTo, clientId);
        }
        logger::getInstance().logInfo("[responder] Event loop stopped");
    }
};
