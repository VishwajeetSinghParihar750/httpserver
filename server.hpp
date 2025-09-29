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
#include "clients.hpp"
#include "mpmcQueue.hpp"
//

int getTcpServerSocket(std::string PORT)
{
    addrinfo hints{}, *result;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_addr = NULL, hints.ai_canonname = NULL, hints.ai_next = NULL;

    int ret = getaddrinfo(NULL, PORT.c_str(), &hints, &result);

    if (ret != 0)
        return -1;

    int sfd = -1;
    addrinfo *res;
    for (res = result; res != nullptr; res = res->ai_next)
    {

        sfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sfd == -1)
            continue;

        int optval = 1;

        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1)
            return -1;

        if (bind(sfd, res->ai_addr, res->ai_addrlen) == 0)
        {
            std::cout << "[getTcpServerSocket] Successfully bound to port " << PORT << "\n"; // logging added by gpt
            break;                                                                           // done
        }

        close(sfd);
    }

    freeaddrinfo(result);

    if (res == NULL)
        return -1;

    return sfd;
}

int setFdNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1)
        return -1;

    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags))
        return -1;

    std::cout << "[setFdNonBlocking] FD " << fd << " set to non-blocking mode\n"; // logging added by gpt
    return 0;
}

int addEpollEtEvent(int efd, int fd)
{
    if (setFdNonBlocking(fd) == -1)
        return -1;

    epoll_event ev{};
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLET;

    if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev) == -1)
        return -1;

    std::cout << "[addEpollEtEvent] FD " << fd << " registered with epoll FD " << efd << "\n"; // logging added by gpt
    return 0;
}

class httpServer
{

    using mpmcRequestQ = mpmcQueue<std::unique_ptr<httpRequest>>; //

    const size_t MAX_WAITING_CONNECTIONS_;
    const std::string PORT_;

    fdRaii sfd_; // socket file descriptor
    fdRaii efd_; // epoll file descirptor

    std::shared_ptr<clients> clients_;
    std::shared_ptr<mpmcRequestQ> httpRequestsQ_;

public:
    httpServer(std::shared_ptr<clients> clients, std::shared_ptr<mpmcRequestQ> httpRequestQ, std::string port = "8080", size_t maxWaitingConnections = 1024)
        : MAX_WAITING_CONNECTIONS_(maxWaitingConnections), PORT_(port), clients_(clients), httpRequestsQ_(httpRequestQ)
    {
        if ((sfd_ = getTcpServerSocket(PORT_)) == -1)
            perror("socket creation"), exit(-1); // ⚠️⚠️  this should be taken out its not this classes responsiblity to print errors

        if (listen(sfd_, MAX_WAITING_CONNECTIONS_) == -1)
            perror("socket listen "), exit(-1);

        if ((efd_ = epoll_create1(0)) == -1)
            perror("epoll creation "), exit(-1);

        if (addEpollEtEvent(efd_, sfd_) == -1)
            perror("add epoll ev, sfd_"), exit(-1);

        std::cout << "[httpServer] Server started on port " << PORT_
                  << " with max waiting connections " << MAX_WAITING_CONNECTIONS_ << "\n"; // logging added by gpt
    }

    void eventLoop()
    {
        size_t MAX_EVENTS = 1024;
        epoll_event events[MAX_EVENTS];

        std::cout << "[httpServer::eventLoop] Entering event loop\n"; // logging added by gpt

        while (true)
        {
            int nfds = epoll_wait(efd_, events, MAX_EVENTS, -1);
            if (nfds == -1)
                perror("epoll wait "), exit(-1);

            std::cout << "[httpServer::eventLoop] " << nfds << " events triggered\n"; // logging added by gpt

            for (int i = 0; i < nfds; i++)
            {
                if (events[i].data.fd == sfd_)
                {
                    int connfd = accept(sfd_, NULL, NULL);
                    if (connfd == -1)
                        perror("accepting conn  ");
                    else
                    {
                        if (clients_->addClient(connfd)) // simple rn
                        {
                            std::cout << "[httpServer::eventLoop] New client connected, FD " << connfd << "\n"; // logging added by gpt

                            addEpollEtEvent(efd_, connfd);
                        }
                    }
                }
                else
                {
                    // handle input for client

                    int cfd = events[i].data.fd;
                    std::cout << "[httpServer::eventLoop] Data available from client FD " << cfd << "\n"; // logging added by gpt

                    std::pair<bool, std::vector<std::unique_ptr<httpRequest>>> res = clients_->getClientsHttpRequestParser(cfd).parseRequest();

                    if (!res.first)
                    {
                                        }

                    std::cout << "[httpServer::eventLoop] Parsed " << res.second.size()
                              << " requests from client FD " << cfd << "\n"; // logging added by gpt

                    for (auto &i : res.second)
                    {
                        if (i->headers_.contains("Connection") && i->headers_["Connection"] == "close")
                            close(cfd);
                        else
                        {
                            i->receivedFrom_ = cfd, i->sendTo_ = cfd;
                            httpRequestsQ_->push(std::move(i));
                            std::cout << "[httpServer::eventLoop] Request pushed to queue from FD " << cfd << "\n"; // logging added by gpt}
                        }
                    }
                }
            }
        }
    }
};
