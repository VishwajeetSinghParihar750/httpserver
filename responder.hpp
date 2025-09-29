#pragma once
#include <stop_token>
#include <sstream>
#include <deque>
#include <memory>
#include <iostream> // logging added by gpt
#include "mpmcQueue.hpp"
#include "response.hpp"

void writeHttpResponseToDeque(std::deque<char> &buffer, const httpResponse &res)
{
    auto appendString = [&buffer](const std::string &str)
    {
        buffer.insert(buffer.end(), str.begin(), str.end());
    };

    std::cout << "[responder] Serializing response for fd=" << res.getSendTo() << std::endl; // logging added by gpt

    appendString(res.version_);
    appendString(" ");
    appendString(httpStatus::to_string(res.statusCode_));
    appendString(" ");
    appendString(res.phrase_);
    appendString("\r\n");

    for (auto &[name, value] : res.headers_)
    {
        appendString(name);
        appendString(": ");
        appendString(value);
        appendString("\r\n");
    }

    appendString("\r\n");

    appendString(res.body_);

    std::cout << "[responder] Response serialized, buffer size=" << buffer.size() << std::endl; // logging added by gpt
}

class responder
{
    using mpmcResponseQ = mpmcQueue<std::unique_ptr<httpResponse>>; //
    std::shared_ptr<mpmcResponseQ> httpResponseQ_;

    std::unordered_map<int, std::deque<char>> buffers_;

    bool checkHttpResponseValidity(const httpResponse &res) const noexcept
    {
        return res.getSendTo() != -1;
    }

    void writeBackResponse(int fd) // ⚡⚡💀 add epoll it will miss data
    {
        size_t totalWritten = 0, written = 0;
        auto &buf = buffers_[fd];
        if (buf.empty())
        {
            std::cout << "[responder] Nothing to write for fd=" << fd << std::endl; // logging added by gpt
            return;
        }

        std::string s(buf.begin(), buf.end());
        std::cout << "[responder] Writing response to fd=" << fd << " with " << buf.size() << " bytes buffered" << std::endl; // logging added by gpt

        while (!buf.empty() && (written = write(fd, s.c_str() + written, buf.size() - written)) > 0)
        {
            totalWritten += written;
            std::cout << "[responder] Wrote " << written << " bytes to fd=" << fd << std::endl; // logging added by gpt
        }

        buf.erase(buf.begin(), buf.begin() + totalWritten);

        if (written == -1 && (errno == EWOULDBLOCK || errno == EAGAIN))
        {
            std::cout << "[responder] Write would block for fd=" << fd << ", keeping buffer" << std::endl; // logging added by gpt
            return;
        }

        if (totalWritten == 0 || written == -1)
        {
            std::cerr << "[responder] Write failed or nothing written, clearing buffer for fd=" << fd << std::endl; // logging added by gpt
            buf.clear();
        }
    }

public:
    responder(std::shared_ptr<mpmcResponseQ> httpResponseQ) : httpResponseQ_(httpResponseQ)
    {
        std::cout << "[responder] Responder initialized" << std::endl; // logging added by gpt
    }

    void eventLoop(std::stop_token stoken) // ℹ️ℹ️ provide all event loops noexcept excpetion gaurantee [ do it actually, or add try catch if cant ]
    {
        std::cout << "[responder] Event loop started" << std::endl; // logging added by gpt
        while (!stoken.stop_requested())
        {
            auto res = httpResponseQ_->pop();
            std::cout << "[responder] Response popped from queue for fd=" << res->getSendTo() << std::endl; // logging added by gpt

            if (checkHttpResponseValidity(*res))
            {
                writeHttpResponseToDeque(buffers_[res->getSendTo()], *res);
                writeBackResponse(res->getSendTo());
            }
            else
            {
                std::cerr << "[responder] Invalid response received (fd=-1)" << std::endl; // logging added by gpt
            }
        }
        std::cout << "[responder] Event loop stopped" << std::endl; // logging added by gpt
    }
};
