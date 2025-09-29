#pragma once

#include "requestParser.hpp"
#include "fdRaii.hpp"
//

class client
{

    fdRaii cfd_; // client file des, client owns this
    httpRequestParser httpRequestParser_;

public:
    client(int cfd) : cfd_(cfd), httpRequestParser_(cfd) {}

    friend class clients;
};

class clients
{
    std::unordered_map<int, client> clientsInfo_;
    std::mutex mtx_; // meh... but fine for now

public:
    bool addClient(int cfd)
    {
        std::unique_lock lock(mtx_);
        clientsInfo_.try_emplace(cfd, cfd).second;
    }
    void removeClient(int cfd)
    {
        std::unique_lock lock(mtx_);
        clientsInfo_.erase(cfd);
    }

    httpRequestParser &getClientsHttpRequestParser(int cfd)
    {
        std::unique_lock lock(mtx_);
        return clientsInfo_.at(cfd).httpRequestParser_; // can throw
    }
};