#pragma once

#include <shared_mutex>
#include <unordered_map>
#include <atomic>
#include "requestParser.hpp"

#include "responseCreator.hpp"
#include "fdRaii.hpp"
#include "types.hpp"
#include "logger.hpp"

class connection
{
    std::unique_ptr<connectionId_t> connectionIdPtr_;
    fdRaii cfd_;
    std::shared_ptr<httpRequestParser> httpRequestParser_;
    std::shared_ptr<httpResponseCreator> httpResponseCreator_;
    std::shared_mutex read_mutex_;
    std::shared_mutex write_mutex_;

public:
    connection(connectionId_t id, int cfd)
        : connectionIdPtr_(std::make_unique<connectionId_t>(id)),
          cfd_(cfd),
          httpRequestParser_(std::make_shared<httpRequestParser>(cfd)),
          httpResponseCreator_(std::make_shared<httpResponseCreator>())
    {
        logger::getInstance().logInfo("[connection] Created, id=" + std::to_string(*connectionIdPtr_) +
                                      ", fd=" + std::to_string(cfd_));
    }

    ~connection()
    {
        std::unique_lock read_lock(read_mutex_);
        std::unique_lock write_lock(write_mutex_);
        logger::getInstance().logInfo("[connection] Destroyed, id=" + std::to_string(*connectionIdPtr_) +
                                      ", fd=" + std::to_string(cfd_));
    }

    connectionId_t getId() const noexcept { return *connectionIdPtr_; }
    int getFd() const noexcept { return cfd_; }

    std::pair<bool, std::vector<std::unique_ptr<httpRequest>>> readRequests()
    {
        std::unique_lock lock(read_mutex_);
        auto res = httpRequestParser_->parseRequest();

        if (res.first)
        {
            for (auto &req : res.second)
            {
                req->connectionId_ = *connectionIdPtr_;
            }
            logger::getInstance().logInfo("[connection] Read " + std::to_string(res.second.size()) +
                                          " requests, id=" + std::to_string(*connectionIdPtr_));
            return {true, std::move(res.second)};
        }

        logger::getInstance().logError("[connection] Failed to read requests, id=" +
                                       std::to_string(*connectionIdPtr_));
        return {false, std::vector<std::unique_ptr<httpRequest>>()};
    }

    bool writeResponse(const httpResponse &response)
    {
        std::unique_lock lock(write_mutex_);
        httpResponseCreator_->addResponse(response);
        bool ok = httpResponseCreator_->write(cfd_);
        logger::getInstance().logInfo(std::string("[connection] Write response ") +
                                      (ok ? "success" : "failed") +
                                      ", id=" + std::to_string(*connectionIdPtr_));
        return ok;
    }

    friend class connectionManager;
};

class connectionManager
{
private:
    std::unordered_map<connectionId_t, std::shared_ptr<connection>> connectionById_;
    std::unordered_map<int, connectionId_t> fdToId_;
    std::shared_mutex globalMutex_;
    std::atomic<connectionId_t> nextConnId_{1};

public:
    connectionId_t *addConnection(int cfd)
    {
        std::unique_lock lock(globalMutex_);
        connectionId_t id = nextConnId_++;
        auto conn = std::make_shared<connection>(id, cfd);

        connectionById_.emplace(id, conn);
        fdToId_.emplace(cfd, id);

        logger::getInstance().logInfo("[connectionManager] Added connection id=" + std::to_string(id) +
                                      ", fd=" + std::to_string(cfd));
        return conn->connectionIdPtr_.get();
    }

    bool removeConnection(connectionId_t connection_id)
    {
        std::unique_lock global_lock(globalMutex_);
        auto id_it = connectionById_.find(connection_id);
        if (id_it == connectionById_.end())
        {
            logger::getInstance().logError("[connectionManager] Remove failed, id not found=" +
                                           std::to_string(connection_id));
            return false;
        }

        int fd = id_it->second->getFd();
        fdToId_.erase(fd);
        connectionById_.erase(id_it);

        logger::getInstance().logInfo("[connectionManager] Removed connection id=" +
                                      std::to_string(connection_id) + ", fd=" + std::to_string(fd));
        return true;
    }

    bool removeConnectionByFd(int cfd)
    {
        std::unique_lock global_lock(globalMutex_);
        auto fd_it = fdToId_.find(cfd);
        if (fd_it == fdToId_.end())
        {
            logger::getInstance().logError("[connectionManager] Remove by fd failed, fd not found=" +
                                           std::to_string(cfd));
            return false;
        }

        connectionId_t id = fd_it->second;
        fdToId_.erase(fd_it);
        connectionById_.erase(id);

        logger::getInstance().logInfo("[connectionManager] Removed connection by fd=" +
                                      std::to_string(cfd) + ", id=" + std::to_string(id));
        return true;
    }

    std::pair<bool, std::vector<std::unique_ptr<httpRequest>>> readRequests(connectionId_t connection_id)
    {
        std::unique_lock lock(globalMutex_);
        if (auto it = connectionById_.find(connection_id); it != connectionById_.end())
        {
            return it->second->readRequests();
        }
        logger::getInstance().logError("[connectionManager] Read requests failed, id not found=" +
                                       std::to_string(connection_id));

        return {false, std::vector<std::unique_ptr<httpRequest>>()};
    }

    bool writeResponse(connectionId_t connection_id, const httpResponse &response)
    {
        std::unique_lock lock(globalMutex_);
        if (auto it = connectionById_.find(connection_id); it != connectionById_.end())
        {
            return it->second->writeResponse(response);
        }
        logger::getInstance().logError("[connectionManager] Write response failed, id not found=" +
                                       std::to_string(connection_id));
        return false;
    }
};
