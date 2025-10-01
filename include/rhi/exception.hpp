#pragma once

#include <cstdint>
#include <format>
#include <stdexcept>
#include <string>

namespace RHI {
    class Exception : public std::runtime_error {
    public:
        explicit Exception(const std::string& msg)
            : std::runtime_error("[RHI]\t" + msg) {}
    };

    class InstanceException final : public Exception {
    public:
        enum ErrorCode : std::uint8_t {
            MISSING_REQUIRED_EXTENSION,
            MISSING_REQUIRED_LAYER,
            MISSING_COMPATIBLE_DEVICE,
            MISSING_SURFACE_WINDOW,
          };

        InstanceException(const ErrorCode code)
            : Exception(std::format("[Instance]\t{}", getMessage(code))),
              code(code) {};

        InstanceException(const ErrorCode code, const std::string& what)
            : Exception(
                  std::format("[Instance]\t{}:\t`{}`", getMessage(code), what)),
              code(code) {};

    private:
        ErrorCode code;

        static auto getMessage(const ErrorCode code) -> std::string {
            switch (code) {
            case ErrorCode::MISSING_REQUIRED_EXTENSION:
                return "Missing Extension";
            case ErrorCode::MISSING_REQUIRED_LAYER:
                return "Missing Layer";
            case ErrorCode::MISSING_COMPATIBLE_DEVICE:
                return "Missing Compatible Device";
            case ErrorCode::MISSING_SURFACE_WINDOW:
                return "Missing Surface Window";
            default:
                return "Unknown";
            }
        }
    };

    class WindowException final : public Exception {
    public:
        enum ErrorCode : std::uint8_t {
            SURFACE_CREATION_ERROR,
          };

        WindowException(const ErrorCode code)
            : Exception(std::format("[Window]\t{}", getMessage(code))),
              code(code) {};

        WindowException(const ErrorCode code, const std::string& what)
            : Exception(
                  std::format("[Window]\t{}:\t`{}`", getMessage(code), what)),
              code(code) {};

    private:
        ErrorCode code;

        static auto getMessage(const ErrorCode code) -> std::string {
            switch (code) {
            case ErrorCode::SURFACE_CREATION_ERROR:
                return "Unable to create Surface";
            default:
                return "Unknown";
            }
        }
    };
}
