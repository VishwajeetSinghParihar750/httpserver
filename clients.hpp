#pragma once
#include <shared_mutex>
#include "requestParser.hpp"
#include "responseCreator.hpp"
#include "fdRaii.hpp"
#include "types.hpp"
//
// TODO: Fix FD reuse race - see issue #1 github
class client
{

    fdRaii cfd_; // client file des, client owns this - getting this from epoll
    clientId_t clientId_;
    std::shared_ptr<httpRequestParser> httpRequestParser_;
    std::shared_ptr<httpResponseCreator> httpResponseCreator_;

public:
    client(int cfd, int id) : cfd_(cfd), clientId_(id), httpRequestParser_(std::make_shared<httpRequestParser>(cfd)), httpResponseCreator_(std::make_shared<httpResponseCreator>(cfd)) {}

    friend class clients;
};

class clients
{
    clientId_t nextId_{};

    std::unordered_map<clientId_t, client> clientsInfo_; // client id to client info
    std::shared_mutex mtx_;                              // meh... but fine for now
    std::unordered_map<int, clientId_t> clientConnFdToId_;

public:
    bool addClient(int cfd)
    {
        std::unique_lock lock(mtx_);
        if (!clientConnFdToId_.contains(cfd))
        {
            bool res = clientsInfo_.try_emplace(nextId_, cfd, nextId_).second;
            if (res)
            {
                nextId_++;
                clientConnFdToId_[cfd] = nextId_;
                return true;
            }
        }
        return false;
    }
    bool removeClient(int cfd)
    {
        std::unique_lock lock(mtx_);
        if (clientConnFdToId_.contains(cfd))
            return clientsInfo_.erase(clientConnFdToId_.at(cfd)) && clientConnFdToId_.erase(cfd);
        return true;
    }

    std::shared_ptr<httpRequestParser> getClientsHttpRequestParser(int fd, clientId_t clientId)
    {
        std::shared_lock lock(mtx_);
        if (clientsInfo_.contains(clientId))
            return clientsInfo_.at(clientId).httpRequestParser_;

        return nullptr;
    }

    std::shared_ptr<httpResponseCreator> getClientsHttpResponseCreator(int fd, clientId_t clientId)
    {
        std::shared_lock lock(mtx_);
        if (clientsInfo_.contains(clientId))
            return clientsInfo_.at(clientId).httpResponseCreator_;

        return nullptr;
    }

    long long getClientIdFromConnFd(int cfd)
    {
        std::shared_lock lock(mtx_);
        if (clientConnFdToId_.contains(cfd))
            return clientConnFdToId_.at(cfd);
        return -1;
    }
};