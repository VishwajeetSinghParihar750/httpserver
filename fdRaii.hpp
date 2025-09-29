#pragma once
#include <unistd.h>
#include <exception>
#include <iostream>

class fdRaii
{
    int fd_ = -1;

    void acquire(int fd) noexcept
    {
        fd_ = fd;
    }
    void release() noexcept
    {
        if (fd_ >= 0)
        {
            if (close(fd_) == -1)
                std::cerr << "error in closing fd : " << fd_ << std::endl;
        }
    }

public:
    fdRaii() = default;
    fdRaii(int fd) noexcept : fd_(fd) {}

    fdRaii(const fdRaii &other) = delete;
    fdRaii &operator=(const fdRaii &other) = delete;

    fdRaii(fdRaii &&other) noexcept : fd_(other.fd_)
    {
        other.fd_ = -1;
    }
    fdRaii &operator=(fdRaii &&other) noexcept
    {
        release();
        acquire(other.fd_);
        other.fd_ = -1;
        return *this;
    }

    ~fdRaii() noexcept
    {
        release();
    }

    operator int() const noexcept { return fd_; }
};