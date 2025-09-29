#pragma once
#include <unordered_map>
#include <string>
#include "types.hpp"
#include "connection.hpp"

class httpRequest
{
    connectionId_t connectionId_{};

public:
    std::string method_, url_, version_;

    std::unordered_map<std::string, std::string> headers_;

    std::string body_;

    connectionId_t getConnectionId() const noexcept { return connectionId_; }

    friend class connection;
};
