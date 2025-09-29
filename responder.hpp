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

    bool checkHttpResponseValidity(const httpResponse &res) const noexcept
    {
        return res.getSendTo() != -1;
    }

    void addResponse(int fd, std::unique_ptr<httpResponse> resp)
    {
        std::shared_ptr<httpResponseCreator> respCreatorPtr = clients_->getClientsHttpResponseCreator(fd);
        if (respCreatorPtr == nullptr)
        {
            if (epollUtils::unwatchEpollFd(efd_, fd) == -1)
                logger::getInstance().logError("[responder] remove epoll fd error ");
        }
        else
        {
            respCreatorPtr->addResponse(*resp);
        }
    }
    void writeBackResponse(int fd)
    {
        std::shared_ptr<httpResponseCreator> respCreatorPtr = clients_->getClientsHttpResponseCreator(fd);
        if (respCreatorPtr == nullptr)
        {
            if (epollUtils::unwatchEpollFd(efd_, fd) == -1)
                logger::getInstance().logError("[responder] remove epoll fd error ");
        }
        else
        {
            if (!respCreatorPtr->write())
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    if (epollUtils::watchEpollEtFd(efd_, fd, EPOLLONESHOT | EPOLLOUT) == -1)
                        perror("epoll watch fd "), exit(-1);
            }
            else
            {
                // clients_->removeClient(fd); // 💀💀💀 check later, added fpr testomg

                // logger::getInstance().logError("[responder] closed fd _________________________________  " + std::to_string(fd));
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
                perror("responder epoll event loop "), exit(-1);

            for (int i = 0; i < nfds; i++)
            {
                writeBackResponse(events[i].data.fd);
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

            if (checkHttpResponseValidity(*res))
            {
                int sendto = res->getSendTo();
                addResponse(sendto, std::move(res));
                writeBackResponse(sendto);
            }
            else
            {
                logger::getInstance().logError("[responder] Invalid response received (fd=-1)");
            }
        }
        logger::getInstance().logInfo("[responder] Event loop stopped");
    }
};
