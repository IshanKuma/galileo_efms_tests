#ifndef LOGUTILS_HPP
#define LOGUTILS_HPP

#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>

inline std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

inline nlohmann::json createLogInfo(const nlohmann::json& additionalData = nlohmann::json()) {
    nlohmann::json info = additionalData;
    info["timestamp"] = getCurrentTimestamp();
    return info;
}

#endif // LOGUTILS_HPP
