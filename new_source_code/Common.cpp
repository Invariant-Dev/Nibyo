// nibyo v1.0 beta - natural english programming language
// common.cpp - global state implementations
#include "Common.h"
#include "Value.h"

// global state definitions
std::mutex output_mutex;
std::mutex task_registry_mutex;
std::unordered_map<std::string, bool> task_registry;
std::unordered_map<std::string, std::shared_ptr<Mailbox>> channel_registry;
std::mutex channel_registry_mutex;

// mailbox implementations
void Mailbox::send(std::shared_ptr<Value> msg) {
    {
        std::lock_guard<std::mutex> guard(lock);
        messages.push(msg);
    }
    cv.notify_one();
}

std::shared_ptr<Value> Mailbox::receive() {
    std::lock_guard<std::mutex> guard(lock);
    if (messages.empty()) return nullptr;
    auto msg = messages.front();
    messages.pop();
    return msg;
}

std::shared_ptr<Value> Mailbox::receiveBlocking(double timeoutSeconds) {
    std::unique_lock<std::mutex> ulock(lock);
    
    auto predicate = [this]{ return !messages.empty(); };
    
    if (timeoutSeconds < 0) {
        cv.wait(ulock, predicate);
    } else {
        auto duration = std::chrono::milliseconds(static_cast<int>(timeoutSeconds * 1000));
        if (!cv.wait_for(ulock, duration, predicate)) {
            return nullptr;
        }
    }
    
    if (messages.empty()) return nullptr;
    auto msg = messages.front();
    messages.pop();
    return msg;
}

// utility function implementations
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::vector<std::string> split(const std::string& s, const std::string& delim) {
    std::vector<std::string> result;
    if (delim.empty()) {
        for (char c : s) result.push_back(std::string(1, c));
        return result;
    }
    size_t start = 0;
    size_t end = s.find(delim);
    
    while (end != std::string::npos) {
        result.push_back(s.substr(start, end - start));
        start = end + delim.length();
        end = s.find(delim, start);
    }
    result.push_back(s.substr(start));
    return result;
}

std::string join(const std::vector<std::string>& v, const std::string& delim) {
    std::string result;
    for (size_t i = 0; i < v.size(); i++) {
        if (i > 0) result += delim;
        result += v[i];
    }
    return result;
}

std::string toLower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string toUpper(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

std::string replaceAll(const std::string& source, const std::string& from, const std::string& to) {
    if (from.empty()) return source;
    std::string result = source;
    size_t pos = 0;
    while ((pos = result.find(from, pos)) != std::string::npos) {
        result.replace(pos, from.length(), to);
        pos += to.length();
    }
    return result;
}
