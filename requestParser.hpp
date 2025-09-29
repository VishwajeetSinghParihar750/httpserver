// #pragma once

// #include <string>
// #include <unordered_map>
// #include <unistd.h>
// #include <memory>
// #include <iostream> // logging added by gpt
// #include <errno.h>
// #include "request.hpp"

// class httpRequestParser // for  HTTP 1.1
// {
//     enum class HTTP_REQUEST_PARSING_STATE
//     {
//         REQUEST_LINE,
//         HEADER,
//         BLANK_LINE,
//         BODY
//     };

//     int fd_;                       // file desciptor to read from
//     static size_t totalBytesRead_; // 0

//     HTTP_REQUEST_PARSING_STATE curRequestState_ = HTTP_REQUEST_PARSING_STATE::REQUEST_LINE;
//     std::unique_ptr<httpRequest> curRequest_;
//     std::vector<std::unique_ptr<httpRequest>> currentlyParsedRequests_;

//     bool foundMethod_{false}, foundUrl_{false};
//     std::string key_{}, value_{};
//     bool foundColon_{false};

//     void makeNewRequest()
//     {
//         curRequest_ = std::make_unique<httpRequest>();
//         curRequestState_ = HTTP_REQUEST_PARSING_STATE::REQUEST_LINE;
//         foundMethod_ = foundUrl_ = false;
//         key_.clear();
//         value_.clear();
//         foundColon_ = false;

//         std::cout << "[Parser] New httpRequest object created" << std::endl; // logging added by gpt
//     }

//     void resetCurrentRequest() noexcept
//     {
//         *curRequest_ = httpRequest{};
//         curRequestState_ = HTTP_REQUEST_PARSING_STATE::REQUEST_LINE;
//         foundMethod_ = foundUrl_ = false;
//         key_.clear();
//         value_.clear();
//         foundColon_ = false;

//         std::cout << "[Parser] Current request reset" << std::endl; // logging added by gpt
//     }

//     bool parseRequestLine()
//     {
//         size_t nread = 0;
//         char buf[1024];
//         while ((nread = read(fd_, buf, sizeof(buf))) > 0)
//         {
//             totalBytesRead_ += nread;
//             std::cout << "[Parser] Read " << nread << " bytes from fd " << fd_ << std::endl; // logging added by gpt

//             const char *p = buf;
//             for (; p < buf + nread; p++)
//             {
//                 char readChar = *p;
//                 if (readChar == ' ')
//                 {
//                     if (foundMethod_)
//                         foundUrl_ = true;
//                     else
//                         foundMethod_ = true;
//                     continue;
//                 }

//                 if (foundUrl_)
//                 {
//                     curRequest_->version_.push_back(readChar);

//                     if (readChar == '\n' && curRequest_->version_.size() > 1 &&
//                         curRequest_->version_[curRequest_->version_.size() - 2] == '\r')
//                     {
//                         curRequest_->version_.pop_back();
//                         curRequest_->version_.pop_back();

//                         curRequestState_ = HTTP_REQUEST_PARSING_STATE::HEADER;
//                         foundMethod_ = false;
//                         foundUrl_ = false;

//                         std::cout << "[Parser] Parsed request line: "
//                                   << curRequest_->method_ << " "
//                                   << curRequest_->url_ << " "
//                                   << curRequest_->version_ << std::endl; // logging added by gpt

//                         return true;
//                     }
//                 }
//                 else if (foundMethod_)
//                 {
//                     curRequest_->url_.push_back(readChar);
//                 }
//                 else
//                 {
//                     curRequest_->method_.push_back(readChar);
//                 }
//             }
//         }

//         if (nread == -1 || totalBytesRead_ == 0)
//         {
//             if (nread == -1 && (errno == EWOULDBLOCK || errno == EAGAIN))
//                 return false;

//             foundMethod_ = false;
//             foundUrl_ = false;
//             resetCurrentRequest();
//         }

//         return false;
//     }

//     bool parseHeader()
//     {
//         size_t nread = 0;
//         char buf[1024];
//         while ((nread = read(fd_, buf, sizeof(buf))) > 0)
//         {
//             totalBytesRead_ += nread;
//             std::cout << "[Parser] Reading headers, bytes read: " << nread << std::endl; // logging added by gpt

//             const char *p = buf;
//             for (; p < buf + nread; p++)
//             {
//                 char readChar = *p;
//                 if (readChar == ':')
//                 {
//                     foundColon_ = true;
//                     continue;
//                 }

//                 if (foundColon_)
//                 {
//                     if (readChar == ' ')
//                         continue;
//                     value_.push_back(readChar);
//                     if (readChar == '\n' && value_.size() > 1 && value_[value_.size() - 2] == '\r')
//                     {
//                         value_.pop_back();
//                         value_.pop_back();
//                         curRequest_->headers_[std::move(key_)] = std::move(value_);

//                         foundColon_ = false;
//                         value_.clear();
//                         key_.clear();

//                         std::cout << "[Parser] Parsed header: " << key_ << std::endl; // logging added by gpt

//                         return true;
//                     }
//                 }
//                 else
//                 {
//                     key_.push_back(readChar);

//                     if (key_ == "\r\n")
//                     {
//                         foundColon_ = false;
//                         value_.clear();
//                         key_.clear();

//                         curRequestState_ = HTTP_REQUEST_PARSING_STATE::BODY;
//                         std::cout << "[Parser] End of headers, moving to BODY" << std::endl; // logging added by gpt
//                         return true;
//                     }
//                 }
//             }
//         }

//         if (nread == -1 || totalBytesRead_ == 0)
//         {
//             if (nread == -1 && (errno == EWOULDBLOCK || errno == EAGAIN))
//                 return false;

//             foundColon_ = false;
//             value_.clear();
//             key_.clear();
//             resetCurrentRequest();
//         }

//         return false;
//     }

//     bool parseBody()
//     {
//         size_t nread = 0;
//         size_t canRead = std::stoull(curRequest_->headers_["Content-Length"]);

//         char buf[1024];
//         while ((nread = read(fd_, buf, sizeof(buf))) > 0)
//         {
//             totalBytesRead_ += nread;
//             std::cout << "[Parser] Reading body, bytes read: " << nread << std::endl; // logging added by gpt

//             const char *p = buf;
//             for (; p < buf + nread; p++)
//             {
//                 char readChar = *p;
//                 curRequest_->body_.push_back(readChar);

//                 if (curRequest_->body_.size() == canRead)
//                 {
//                     currentlyParsedRequests_.push_back(std::move(curRequest_));
//                     std::cout << "[Parser] Parsed full body, size: " << canRead << std::endl; // logging added by gpt
//                     makeNewRequest();
//                     return true;
//                 }
//             }
//         }

//         if (nread == -1 || totalBytesRead_ == 0)
//         {
//             if (nread == -1 && (errno == EWOULDBLOCK || errno == EAGAIN))
//                 return false;
//             resetCurrentRequest();
//         }
//         return false;
//     }

//     bool parseBodyChunked()
//     {
//         // will handle later
//         return false;
//     }

// public:
//     httpRequestParser(int fd) : fd_(fd)
//     {
//         makeNewRequest();
//     }

//     auto parseRequest()
//     {
//         bool res = true;
//         totalBytesRead_ = 0;
//         currentlyParsedRequests_.clear();

//         while (res)
//         {
//             switch (curRequestState_)
//             {
//             case HTTP_REQUEST_PARSING_STATE::REQUEST_LINE:
//                 res = parseRequestLine();
//                 break;
//             case HTTP_REQUEST_PARSING_STATE::HEADER:
//                 res = parseHeader();
//                 break;
//             case HTTP_REQUEST_PARSING_STATE::BODY:

//                 if (curRequest_->headers_.contains("Transfer-Encoding") &&
//                     curRequest_->headers_["Transfer-Encoding"] == "chunked")
//                     res = parseBodyChunked();
//                 else if (curRequest_->headers_.contains("Content-Length"))
//                     res = parseBody();
//                 else
//                 {
//                     currentlyParsedRequests_.push_back(std::move(curRequest_));
//                     std::cout << "[Parser] No body, request ready" << std::endl; // logging added by gpt
//                     makeNewRequest();
//                     res = false;
//                 }
//                 break;

//             default:
//                 break;
//             }

//             if (totalBytesRead_ == 0)
//                 break;
//         }

//         return std::move(currentlyParsedRequests_);
//     }
// };

// inline size_t httpRequestParser::totalBytesRead_ = 0;

#pragma once

#include <string>
#include <unordered_map>
#include <unistd.h>
#include <memory>
#include <iostream>
#include <errno.h>
#include "request.hpp"

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

        std::cout << "[Parser] New httpRequest object created" << std::endl;
    }

    void resetCurrentRequest() noexcept
    {
        *curRequest_ = httpRequest{};
        curRequestState_ = HTTP_REQUEST_PARSING_STATE::REQUEST_LINE;
        foundMethod_ = foundUrl_ = false;
        key_.clear();
        value_.clear();
        foundColon_ = false;

        std::cout << "[Parser] Current request reset" << std::endl;
    }

    bool parseRequestLine()
    {
        for (; parsePos_ < buffer_.size(); ++parsePos_)
        {
            char readChar = buffer_[parsePos_];
            std::cout << "[Parser][REQUEST_LINE] char: '"
                      << (readChar == '\r' ? "\\r" : readChar == '\n' ? "\\n"
                                                                      : std::string(1, readChar))
                      << "'" << std::endl;

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

                    std::cout << "[Parser] Parsed request line: "
                              << curRequest_->method_ << " "
                              << curRequest_->url_ << " "
                              << curRequest_->version_ << std::endl;

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
            std::cout << "[Parser][HEADER] char: '"
                      << (readChar == '\r' ? "\\r" : readChar == '\n' ? "\\n"
                                                                      : std::string(1, readChar))
                      << "'" << std::endl;

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

                    std::cout << "[Parser] Parsed header: " << key_ << ": " << value_ << std::endl;

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
                    std::cout << "[Parser] End of headers detected" << std::endl;
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
            std::cout << "[Parser] No body, request ready" << std::endl;
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
            std::cout << "[Parser] Parsed full body, size: " << contentLength << std::endl;
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
            std::cout << "[Parser] Read " << nread << " bytes from fd " << fd_ << std::endl;
        }

        if (nread == -1 && !(errno == EWOULDBLOCK || errno == EAGAIN))
        {
            std::cerr << "[Parser] read() error, errno: " << errno << std::endl;
            resetCurrentRequest();
            return {false, std::vector<std::unique_ptr<httpRequest>>{}};
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

        return {true, std::move(currentlyParsedRequests_)};
    }
};

inline size_t httpRequestParser::totalBytesRead_ = 0;
