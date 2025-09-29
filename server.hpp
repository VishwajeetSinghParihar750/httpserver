#pragma once
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <exception>
#include <assert.h>
#include <stdexcept>
#include <iostream>
#include <unordered_map>
#include <queue>
#include <algorithm>
#include <fcntl.h>
#include "fdRaii.hpp"
#include "connection.hpp"
#include "mpmcQueue.hpp"
#include "epollUtils.hpp"
#include "logger.hpp"
//

inline bool shouldCloseConnection(const httpRequest &req)
{
    if (req.headers_.contains("Connection") && req.headers_.at("Connection") == "close")
    {
        return true;
    }
    return false;
}

class httpServer
{

    using mpmcRequestQ = mpmcQueue<std::unique_ptr<httpRequest>>; //

    const size_t MAX_WAITING_CONNECTIONS_;
    const std::string PORT_;

    fdRaii sfd_; // socket file descriptor
    fdRaii efd_; // epoll file descirptor

    std::shared_ptr<connectionManager> connManager_;
    std::shared_ptr<mpmcRequestQ> httpRequestsQ_;

public:
    httpServer(std::shared_ptr<connectionManager> clients, std::shared_ptr<mpmcRequestQ> httpRequestQ, std::string port = "8080", size_t maxWaitingConnections = 1024)
        : MAX_WAITING_CONNECTIONS_(maxWaitingConnections), PORT_(port), connManager_(clients), httpRequestsQ_(httpRequestQ)
    {
        if ((sfd_ = epollUtils::getTcpServerSocket(PORT_)) == -1)
            perror("socket creation"), exit(-1);

        if (listen(sfd_, MAX_WAITING_CONNECTIONS_) == -1)
            perror("socket listen "), exit(-1);

        if ((efd_ = epoll_create1(0)) == -1)
            perror("epoll creation "), exit(-1);

        if (epollUtils::watchEpollEtFd(efd_, sfd_, EPOLLIN) == -1)
            perror("add epoll ev, sfd_"), exit(-1);

        logger::getInstance().logInfo("[httpServer] Server started on port " + PORT_ +
                                      " with max waiting connections " + std::to_string(MAX_WAITING_CONNECTIONS_));
    }

    void eventLoop()
    {
        size_t MAX_EVENTS = 1024;
        epoll_event events[MAX_EVENTS];

        logger::getInstance().logInfo("[httpServer::eventLoop] Entering event loop");

        while (true)
        {
            int nfds = epoll_wait(efd_, events, MAX_EVENTS, -1);
            if (nfds == -1)
                perror("epoll wait "), exit(-1);

            logger::getInstance().logInfo("[httpServer::eventLoop] " + std::to_string(nfds) + " events triggered");

            for (int i = 0; i < nfds; i++)
            {

                if (events[i].data.fd == sfd_)
                {
                    int connfd = accept(sfd_, NULL, NULL);
                    if (connfd == -1)
                        perror("accepting conn  ");
                    else
                    {
                        connectionId_t connId = connManager_->addConnection(connfd);

                        if (connId != 0) // simple rn
                        {
                            logger::getInstance().logInfo("[httpServer::eventLoop] New client connected, FD " + std::to_string(connfd));

                            if (epollUtils::watchEpollEtFd(efd_, connfd, EPOLLIN, new connectionId_t(connId)) == -1)
                                perror("could not add fd to watch "), exit(-1);
                        }
                    }
                }
                else
                {
                    // handle input for client

                    connectionId_t *connIdPtr = (connectionId_t *)events[i].data.ptr;
                    logger::getInstance().logInfo("[httpServer::eventLoop] Data available from client FD ");

                    std::pair<bool, std::vector<std::unique_ptr<httpRequest>>> res = connManager_->readRequests(*connIdPtr);

                    if (!res.first ||
                        (res.first && !res.second.empty() && shouldCloseConnection(*res.second.back())))
                    {

                        if (!connManager_->removeConnection(*connIdPtr))
                            perror("could not rmeove client ");
                        else
                            delete connIdPtr;

                        break;
                    }

                    logger::getInstance().logInfo("[httpServer::eventLoop] Parsed " + std::to_string(res.second.size()) +
                                                  " requests from client FD ");

                    for (auto &i : res.second)
                    {
                        httpRequestsQ_->push(std::move(i));

                        logger::getInstance().logInfo("[httpServer::eventLoop] Request pushed to queue from FD ");
                    }
                }
            }
        }
    }
};
