#pragma once
#include <unordered_map>
#include <string>
#include "clients.hpp"
#include "types.hpp"

class httpServer;

class httpRequest
{
    int fdReadFrom_ = -1, fdSendTo_ = -1;
    clientId_t clientId_{};

public:
    std::string method_,
        url_,
        version_;

    std::unordered_map<std::string, std::string> headers_;

    std::string body_;

    int getReceivedFrom() const noexcept { return fdReadFrom_; }
    int getSendTo() const noexcept { return fdSendTo_; }
    auto getClientId() const noexcept { return clientId_; }

    friend class httpServer;
};
