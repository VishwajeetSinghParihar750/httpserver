#pragma once

#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <iostream>
#include "logger.hpp"

namespace epollUtils
{
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
                logger::getInstance().logInfo("[getTcpServerSocket] Successfully bound to port " + PORT);
                break;
            }

            close(sfd);
        }

        freeaddrinfo(result);

        if (res == NULL)
            return -1;

        return sfd;
    }

    int setFdNonBlocking(int fd) noexcept
    {
        int flags = fcntl(fd, F_GETFL);
        if (flags == -1)
            return -1;

        flags |= O_NONBLOCK;
        if (fcntl(fd, F_SETFL, flags))
            return -1;

        logger::getInstance().logInfo("[setFdNonBlocking] FD " + std::to_string(fd) + " set to non-blocking mode");
        return 0;
    }

    int watchEpollEtFd(int efd, int fd, int flags, void *dataPtr = nullptr) noexcept
    {
        if (setFdNonBlocking(fd) == -1)
            return -1;

        epoll_event ev{};
        if (dataPtr == nullptr)
            ev.data.fd = fd;
        else
            ev.data.ptr = dataPtr;

        ev.events = EPOLLET | flags;

        if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev) == -1)
            return -1;

        logger::getInstance().logInfo("[addEpollEtEvent] FD " + std::to_string(fd) +
                                      " registered with epoll FD " + std::to_string(efd));
        return 0;
    }

        int unwatchEpollFd(int efd, int fd) noexcept
    {
        if (epoll_ctl(efd, EPOLL_CTL_DEL, fd, NULL) == -1)
        {
            if (errno != EBADF)
                return -1;
        }
        return 0;
    }
}
