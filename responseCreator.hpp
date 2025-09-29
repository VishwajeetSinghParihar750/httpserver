#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <mutex>
#include <unistd.h>
#include "request.hpp"
#include "response.hpp"
#include "logger.hpp"

class httpResponseCreator // this is therad safe
{
private:
    std::string buffer_;
    size_t writePos_ = 0; // Current position in buffer for writing
    int fd_;              // towrite to

    std::mutex mtx_; // multiple therads can come

public:
    httpResponseCreator(int fd) : fd_(fd) {}

    // Add httpResponse to internal buffer in HTTP format
    void addResponse(const httpResponse &response)
    {

        std::unique_lock lock(mtx_);

        logger::getInstance().logInfo("[ResponseCreator] Adding response to buffer for fd_=" + std::to_string(response.getSendTo()));

        auto oldSize = buffer_.size();

        buffer_ += response.version_;
        buffer_ += " ";
        buffer_ += std::to_string(static_cast<int>(response.statusCode_));
        buffer_ += " ";
        buffer_ += response.phrase_;
        buffer_ += "\r\n";

        for (const auto &[name, value] : response.headers_)
        {
            buffer_ += name;
            buffer_ += ": ";
            buffer_ += value;
            buffer_ += "\r\n";
        }

        buffer_ += "\r\n";

        buffer_ += response.body_;

        logger::getInstance().logInfo("[ResponseCreator] Response added to buffer. Size increased by: " +
                                      std::to_string(buffer_.size() - oldSize) + " bytes. Total buffer: " +
                                      std::to_string(buffer_.size()) + " bytes");
    }

    // Write to file descriptor - returns true if ALL data was written, false if partial/no write
    bool write()
    {

        std::unique_lock lock(mtx_);

        if (writePos_ >= buffer_.size())
        {
            logger::getInstance().logInfo("[ResponseCreator] No data to write");
            return true; // Nothing to write is considered "complete"
        }

        size_t remaining = buffer_.size() - writePos_;
        ssize_t bytesWritten = ::write(fd_, buffer_.data() + writePos_, remaining);

        if (bytesWritten > 0)
        {
            writePos_ += bytesWritten;
            logger::getInstance().logInfo("[ResponseCreator] Wrote " + std::to_string(bytesWritten) +
                                          " bytes to fd_ " + std::to_string(fd_) + ". Remaining: " +
                                          std::to_string(buffer_.size() - writePos_) + " bytes");

            // Check if ALL data was written
            if (writePos_ == buffer_.size())
            {
                logger::getInstance().logInfo("[ResponseCreator] All data successfully written to fd_ " + std::to_string(fd_));
                return true;
            }
            return false; // Partial write
        }
        else if (bytesWritten == 0)
        {
            logger::getInstance().logInfo("[ResponseCreator] Write returned 0 bytes");
            return false;
        }
        else // bytesWritten == -1
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                logger::getInstance().logInfo("[ResponseCreator] Write would block, fd_ " + std::to_string(fd_) + " not ready");
            }
            else
            {
                logger::getInstance().logError("[ResponseCreator] Write error on fd_ " + std::to_string(fd_) + ", errno: " + std::to_string(errno));
            }
            return false;
        }
    }
};
