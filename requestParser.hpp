#pragma once

#include <unistd.h>
#include <errno.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

#include "request.hpp"
#include "logger.hpp"

class httpRequestParser // for HTTP 1.1
{
    enum class HTTP_REQUEST_PARSING_STATE
    {
        REQUEST_LINE,
        HEADER,
        BODY
    };

    int fd_;
    static size_t totalBytesRead_;

    HTTP_REQUEST_PARSING_STATE curRequestState_ = HTTP_REQUEST_PARSING_STATE::REQUEST_LINE;
    std::unique_ptr<httpRequest> curRequest_;
    std::vector<std::unique_ptr<httpRequest>> currentlyParsedRequests_;

    bool foundMethod_{false}, foundUrl_{false};
    std::string key_{}, value_{};
    bool foundColon_{false};

    std::string buffer_;  // global buffer to store all read bytes
    size_t parsePos_ = 0; // current parse position in buffer

    void makeNewRequest()
    {
        curRequest_ = std::make_unique<httpRequest>();
        curRequestState_ = HTTP_REQUEST_PARSING_STATE::REQUEST_LINE;
        foundMethod_ = foundUrl_ = false;
        key_.clear();
        value_.clear();
        foundColon_ = false;

        logger::getInstance().logInfo("[Parser] New httpRequest object created");
    }

    void resetCurrentRequest() noexcept
    {
        *curRequest_ = httpRequest{};
        curRequestState_ = HTTP_REQUEST_PARSING_STATE::REQUEST_LINE;
        foundMethod_ = foundUrl_ = false;
        key_.clear();
        value_.clear();
        foundColon_ = false;

        logger::getInstance().logInfo("[Parser] Current request reset");
    }

    bool parseRequestLine()
    {
        for (; parsePos_ < buffer_.size(); ++parsePos_)
        {
            char readChar = buffer_[parsePos_];

            if (readChar == ' ')
            {
                if (foundMethod_)
                    foundUrl_ = true;
                else
                    foundMethod_ = true;
                continue;
            }

            if (foundUrl_)
            {
                curRequest_->version_.push_back(readChar);

                if (readChar == '\n' && curRequest_->version_.size() > 1 &&
                    curRequest_->version_[curRequest_->version_.size() - 2] == '\r')
                {
                    curRequest_->version_.pop_back();
                    curRequest_->version_.pop_back();

                    curRequestState_ = HTTP_REQUEST_PARSING_STATE::HEADER;
                    foundMethod_ = false;
                    foundUrl_ = false;

                    logger::getInstance().logInfo("[Parser] Parsed request line: " + curRequest_->method_ + " " + curRequest_->url_ + " " + curRequest_->version_);

                    ++parsePos_; // move past '\n'
                    return true;
                }
            }
            else if (foundMethod_)
            {
                curRequest_->url_.push_back(readChar);
            }
            else
            {
                curRequest_->method_.push_back(readChar);
            }
        }
        return false; // need more data
    }

    bool parseHeader()
    {
        for (; parsePos_ < buffer_.size(); ++parsePos_)
        {
            char readChar = buffer_[parsePos_];

            if (readChar == ':')
            {
                foundColon_ = true;
                continue;
            }

            if (foundColon_)
            {
                if (readChar == ' ')
                    continue;
                value_.push_back(readChar);
                if (readChar == '\n' && value_.size() > 1 && value_[value_.size() - 2] == '\r')
                {
                    value_.pop_back();
                    value_.pop_back();

                    logger::getInstance().logInfo("[Parser] Parsed header: " + key_ + ": " + value_);

                    curRequest_->headers_[std::move(key_)] = std::move(value_);

                    foundColon_ = false;
                    value_.clear();
                    key_.clear();

                    ++parsePos_; // move past '\n'
                    return true;
                }
            }
            else
            {
                key_.push_back(readChar);

                if (key_ == "\r\n")
                {
                    logger::getInstance().logInfo("[Parser] End of headers detected");

                    foundColon_ = false;
                    value_.clear();
                    key_.clear();

                    curRequestState_ = HTTP_REQUEST_PARSING_STATE::BODY;
                    ++parsePos_; // move past '\n'
                    return true;
                }
            }
        }
        return false; // need more data
    }

    bool parseBody()
    {
        if (!curRequest_->headers_.contains("Content-Length"))
        {
            currentlyParsedRequests_.push_back(std::move(curRequest_));

            logger::getInstance().logInfo("[Parser] No body, request ready");

            makeNewRequest();

            return false;
        }

        size_t contentLength = std::stoull(curRequest_->headers_["Content-Length"]);

        while (parsePos_ < buffer_.size() && curRequest_->body_.size() < contentLength)
        {
            char readChar = buffer_[parsePos_++];
            curRequest_->body_.push_back(readChar);
        }

        if (curRequest_->body_.size() == contentLength)
        {
            currentlyParsedRequests_.push_back(std::move(curRequest_));
            logger::getInstance().logInfo("[Parser] Parsed full body, size: " + std::to_string(contentLength));
            makeNewRequest();
            return true;
        }

        return false; // need more data
    }

    bool parseBodyChunked()
    {
        // Not implemented yet
        return false;
    }

public:
    httpRequestParser(int fd) : fd_(fd)
    {
        makeNewRequest();
    }

    std::pair<bool, std::vector<std::unique_ptr<httpRequest>>> parseRequest()
    {
        // read all available data
        char buf[4096];
        ssize_t nread = 0;
        while ((nread = read(fd_, buf, sizeof(buf))) > 0)
        {
            totalBytesRead_ += nread;
            buffer_.append(buf, nread);

            logger::getInstance().logInfo("[Parser] Read " + std::to_string(nread) + " bytes from fd " + std::to_string(fd_));
        }

        if (nread == -1 && !(errno == EWOULDBLOCK || errno == EAGAIN))
        {

            resetCurrentRequest();
            return std::make_pair<bool, std::vector<std::unique_ptr<httpRequest>>>(false, std::vector<std::unique_ptr<httpRequest>>());
        }

        bool res = true;
        while (res)
        {
            switch (curRequestState_)
            {
            case HTTP_REQUEST_PARSING_STATE::REQUEST_LINE:
                res = parseRequestLine();
                break;
            case HTTP_REQUEST_PARSING_STATE::HEADER:
                res = parseHeader();
                break;
            case HTTP_REQUEST_PARSING_STATE::BODY:
                if (curRequest_->headers_.contains("Transfer-Encoding") &&
                    curRequest_->headers_["Transfer-Encoding"] == "chunked")
                    res = parseBodyChunked();
                else
                    res = parseBody();
                break;
            }
        }

        return std::make_pair<bool, std::vector<std::unique_ptr<httpRequest>>>(true, std::move(currentlyParsedRequests_));
    }
};

inline size_t httpRequestParser::totalBytesRead_ = 0;
