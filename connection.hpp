#pragma once

#include <shared_mutex>
#include <unordered_map>
#include <atomic>
#include "requestParser.hpp"
#include "responseCreator.hpp"
#include "fdRaii.hpp"
#include "types.hpp"

class connection
{
    connectionId_t connection_id_;
    fdRaii cfd_; // client file descriptor
    std::shared_ptr<httpRequestParser> httpRequestParser_;
    std::shared_ptr<httpResponseCreator> httpResponseCreator_;
    std::shared_mutex read_mutex_;
    std::shared_mutex write_mutex_;

public:
    connection(connectionId_t id, int cfd)
        : connection_id_(id),
          cfd_(cfd),
          httpRequestParser_(std::make_shared<httpRequestParser>(cfd)),
          httpResponseCreator_(std::make_shared<httpResponseCreator>()) {}

    ~connection()
    {
        // Acquire both locks to ensure no ongoing operations
        std::unique_lock read_lock(read_mutex_);
        std::unique_lock write_lock(write_mutex_);
        // Safe destruction - no races
    }

    connectionId_t getId() const noexcept { return connection_id_; }
    int getFd() const noexcept { return cfd_; }

    // Read operation - requires read lock
    std::pair<bool, std::vector<std::unique_ptr<httpRequest>>> readRequests()
    {
        std::shared_lock lock(read_mutex_);
        std::pair<bool, std::vector<std::unique_ptr<httpRequest>>> res = httpRequestParser_->parseRequest();

        bool success = res.first;

        if (res.first)
        {
            // Set connection ID in all requests for responder
            for (auto &req : res.second)
            {
                req->connectionId_ = connection_id_;
            }
            return std::make_pair<bool, std::vector<std::unique_ptr<httpRequest>>>(true, std::move(res.second));
        }
        return std::make_pair<bool, std::vector<std::unique_ptr<httpRequest>>>(false, std::vector<std::unique_ptr<httpRequest>>{});
    }

    //     // Write operation - requires write lock
    bool writeResponse(const httpResponse &response)
    {
        std::shared_lock lock(write_mutex_);
        httpResponseCreator_->addResponse(response);
        return httpResponseCreator_->write(cfd_);
    }

    friend class ConnectionManager;
};

class connectionManager
{
private:
    std::unordered_map<connectionId_t, std::shared_ptr<connection>> connections_by_id_;
    std::unordered_map<int, connectionId_t> fd_to_id_; // Map FD to connection ID
    std::shared_mutex global_mutex_;
    std::atomic<connectionId_t> next_connection_id_{1};

public:
    // Add connection and return the connection ID
    connectionId_t addConnection(int cfd)
    {
        std::unique_lock lock(global_mutex_);
        connectionId_t id = next_connection_id_++;
        auto conn = std::make_shared<connection>(id, cfd);

        connections_by_id_.try_emplace(id, conn);
        fd_to_id_.try_emplace(cfd, id);

        return id;
    }

    // Remove connection by ID - acquires both connection locks internally
    bool removeConnection(connectionId_t connection_id)
    {
        std::unique_lock global_lock(global_mutex_);
        auto id_it = connections_by_id_.find(connection_id);
        if (id_it == connections_by_id_.end())
        {
            return false;
        }

        // Remove from FD mapping first
        int fd = id_it->second->getFd();
        fd_to_id_.erase(fd);

        // Connection destructor will acquire both read/write locks
        connections_by_id_.erase(id_it);

        return true;
    }

    // Remove connection by FD
    bool removeConnectionByFd(int cfd)
    {
        std::unique_lock global_lock(global_mutex_);
        auto fd_it = fd_to_id_.find(cfd);
        if (fd_it == fd_to_id_.end())
        {
            return false;
        }

        connectionId_t id = fd_it->second;
        fd_to_id_.erase(fd_it);
        connections_by_id_.erase(id);

        return true;
    }

    // Server API: Read requests using connection ID
    std::pair<bool, std::vector<std::unique_ptr<httpRequest>>> readRequests(connectionId_t connection_id)
    {
        std::shared_lock lock(global_mutex_);
        if (auto it = connections_by_id_.find(connection_id); it != connections_by_id_.end())
        {
            return it->second->readRequests();
        }
        return {false, std::vector<std::unique_ptr<httpRequest>>()};
    }

    // Responder API: Write response using connection ID
    bool writeResponse(connectionId_t connection_id, const httpResponse &response)
    {
        std::shared_lock lock(global_mutex_);
        if (auto it = connections_by_id_.find(connection_id); it != connections_by_id_.end())
        {
            return it->second->writeResponse(response);
        }
        return false;
    }

    // Utility methods
    connectionId_t getConnectionId(int cfd)
    {
        std::shared_lock lock(global_mutex_);
        if (auto it = fd_to_id_.find(cfd); it != fd_to_id_.end())
        {
            return it->second;
        }
        return 0;
    }

    int getConnectionFd(connectionId_t connection_id)
    {
        std::shared_lock lock(global_mutex_);
        if (auto it = connections_by_id_.find(connection_id); it != connections_by_id_.end())
        {
            return it->second->getFd();
        }
        return -1;
    }

    bool isValidConnection(connectionId_t connection_id)
    {
        std::shared_lock lock(global_mutex_);
        return connections_by_id_.contains(connection_id);
    }

    bool isValidFd(int cfd)
    {
        std::shared_lock lock(global_mutex_);
        return fd_to_id_.contains(cfd);
    }
};