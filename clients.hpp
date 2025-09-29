#pragma once
#include <shared_mutex>

#include "requestParser.hpp"
#include "responseCreator.hpp"
#include "fdRaii.hpp"
//
// TODO: Fix FD reuse race - see issue #1 github
class client
{

    fdRaii cfd_; // client file des, client owns this
    std::shared_ptr<httpRequestParser> httpRequestParser_;
    std::shared_ptr<httpResponseCreator> httpResponseCreator_;

public:
    client(int cfd) : cfd_(cfd), httpRequestParser_(std::make_shared<httpRequestParser>(cfd)), httpResponseCreator_(std::make_shared<httpResponseCreator>(cfd)) {}

    friend class clients;
};

class clients
{
    std::unordered_map<int, client> clientsInfo_;
    std::shared_mutex mtx_; // meh... but fine for now

public:
    bool addClient(int cfd)
    {
        std::unique_lock lock(mtx_);
        return clientsInfo_.try_emplace(cfd, cfd).second;
    }
    bool removeClient(int cfd)
    {
        std::unique_lock lock(mtx_);
        if (clientsInfo_.contains(cfd))
            return clientsInfo_.erase(cfd);
        return true;
    }

    std::shared_ptr<httpRequestParser> getClientsHttpRequestParser(int cfd)
    {
        std::shared_lock lock(mtx_);
        if (clientsInfo_.contains(cfd))
            return clientsInfo_.at(cfd).httpRequestParser_;

        return nullptr;
    }

    std::shared_ptr<httpResponseCreator> getClientsHttpResponseCreator(int cfd)
    {
        std::shared_lock lock(mtx_);
        if (clientsInfo_.contains(cfd))
            return clientsInfo_.at(cfd).httpResponseCreator_;

        return nullptr;
    }
};