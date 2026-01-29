#pragma once

#include <stdexcept>
#include <string>
#include <optional>

namespace muxi {

class MuxiException : public std::runtime_error {
public:
    MuxiException(const std::string& code, const std::string& message, int status, std::optional<int> retry_after = std::nullopt)
        : std::runtime_error(code.empty() ? message : code + ": " + message),
          error_code_(code), status_code_(status), retry_after_(retry_after) {}
    
    const std::string& error_code() const { return error_code_; }
    int status_code() const { return status_code_; }
    std::optional<int> retry_after() const { return retry_after_; }

protected:
    std::string error_code_;
    int status_code_;
    std::optional<int> retry_after_;
};

class AuthenticationException : public MuxiException {
public:
    AuthenticationException(const std::string& code, const std::string& message, int status)
        : MuxiException(code.empty() ? "UNAUTHORIZED" : code, message, status) {}
};

class AuthorizationException : public MuxiException {
public:
    AuthorizationException(const std::string& code, const std::string& message, int status)
        : MuxiException(code.empty() ? "FORBIDDEN" : code, message, status) {}
};

class NotFoundException : public MuxiException {
public:
    NotFoundException(const std::string& code, const std::string& message, int status)
        : MuxiException(code.empty() ? "NOT_FOUND" : code, message, status) {}
};

class ConflictException : public MuxiException {
public:
    ConflictException(const std::string& code, const std::string& message, int status)
        : MuxiException(code.empty() ? "CONFLICT" : code, message, status) {}
};

class ValidationException : public MuxiException {
public:
    ValidationException(const std::string& code, const std::string& message, int status)
        : MuxiException(code.empty() ? "VALIDATION_ERROR" : code, message, status) {}
};

class RateLimitException : public MuxiException {
public:
    RateLimitException(const std::string& message, int status, std::optional<int> retry_after)
        : MuxiException("RATE_LIMITED", message.empty() ? "Too Many Requests" : message, status, retry_after) {}
};

class ServerException : public MuxiException {
public:
    ServerException(const std::string& code, const std::string& message, int status)
        : MuxiException(code.empty() ? "SERVER_ERROR" : code, message, status) {}
};

class ConnectionException : public MuxiException {
public:
    ConnectionException(const std::string& message)
        : MuxiException("CONNECTION_ERROR", message, 0) {}
};

inline MuxiException map_error(int status, const std::string& code, const std::string& message, std::optional<int> retry_after = std::nullopt) {
    switch (status) {
        case 401: return AuthenticationException(code, message, status);
        case 403: return AuthorizationException(code, message, status);
        case 404: return NotFoundException(code, message, status);
        case 409: return ConflictException(code, message, status);
        case 422: return ValidationException(code, message, status);
        case 429: return RateLimitException(message, status, retry_after);
        case 500: case 501: case 502: case 503: case 504:
            return ServerException(code, message, status);
        default: return MuxiException(code, message, status);
    }
}

} // namespace muxi
