#pragma once
#include <string>
#include <iostream>
#include <ostream>

class logger
{ // logger singleton

    bool on = false;

    logger() = default;
    ~logger() = default;

public:
    logger(const logger &) = delete;
    logger &operator=(const logger &) = delete;
    logger(logger &&) = delete;
    logger &operator=(logger &&) = delete;

    static logger &getInstance() // this is thread safe in modern c++
    {
        static logger instance;
        return instance;
    }

    void logInfo(const std::string &msg) // msgs would be interleaved without lockign
    {
        if (on)
            std::cout << msg << '\n';
    }
    void logError(const std::string &msg)
    {
        if (on)
            std::cerr << msg << '\n';
    }
    void testLog(const std::string &msg)
    {

        std::cout << msg << '\n';
    }
};
//