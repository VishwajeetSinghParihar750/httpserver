
#pragma once
#include <unordered_map>
#include <string>
#include "request.hpp"

class httpStatus // SOURCE : gpt [coz thats what thing thing si for ]
{
public:
    enum Code
    {
        // Success
        OK = 200,         // The request is successful.
        CREATED = 201,    // A new URL is created.
        ACCEPTED = 202,   // The request is accepted, but it is not immediately acted upon.
        NO_CONTENT = 204, // There is no content in the body.

        // Client Error
        BAD_REQUEST = 400,        // There is a syntax error in the request.
        UNAUTHORIZED = 401,       // The request lacks proper authorization.
        FORBIDDEN = 403,          // Service is denied.
        NOT_FOUND = 404,          // The document is not found.
        METHOD_NOT_ALLOWED = 405, // The method is not supported in this URL.
        NOT_ACCEPTABLE = 406,     // The format requested is not acceptable.

        // Server Error
        INTERNAL_SERVER_ERROR = 500, // There is an error, such as a crash, at the server site.
        NOT_IMPLEMENTED = 501,       // The action requested cannot be performed.
        SERVICE_UNAVAILABLE = 503    // The service is temporarily unavailable.
    };

    static std::string to_string(Code code)
    {
        static const std::unordered_map<Code, std::string> codeMap = {
            // Success
            {OK, "OK"},
            {CREATED, "Created"},
            {ACCEPTED, "Accepted"},
            {NO_CONTENT, "No Content"},

            // Client Error
            {BAD_REQUEST, "Bad Request"},
            {UNAUTHORIZED, "Unauthorized"},
            {FORBIDDEN, "Forbidden"},
            {NOT_FOUND, "Not Found"},
            {METHOD_NOT_ALLOWED, "Method Not Allowed"},
            {NOT_ACCEPTABLE, "Not Acceptable"},

            // Server Error
            {INTERNAL_SERVER_ERROR, "Internal Server Error"},
            {NOT_IMPLEMENTED, "Not Implemented"},
            {SERVICE_UNAVAILABLE, "Service Unavailable"}};

        if (codeMap.contains(code))
            return codeMap.at(code);

        return "Unknown Status";
    }
};

//
class httpResponse
{
    int receivedFrom_ = -1, sendTo_ = -1;

public:
    std::string version_;
    httpStatus::Code statusCode_ = httpStatus::Code::OK;
    std::string phrase_;

    std::unordered_map<std::string, std::string> headers_;

    std::string body_;

    int getReceivedFrom() const noexcept { return receivedFrom_; }
    int getSendTo() const noexcept { return sendTo_; }

    httpResponse(httpRequest req) : receivedFrom_(req.getReceivedFrom()), sendTo_(req.getSendTo()) {}
};