#pragma once
#include <unordered_map>
#include <string>

class httpServer;

class httpRequest
{
    int receivedFrom_ = -1, sendTo_ = -1;

public:
    std::string method_, url_, version_;

    std::unordered_map<std::string, std::string> headers_;

    std::string body_;

    int getReceivedFrom() const noexcept { return receivedFrom_; }
    int getSendTo() const noexcept { return sendTo_; }

    friend class httpServer;
};
