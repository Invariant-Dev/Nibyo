// nexo v1.0 beta - natural english programming language
// common.h - forward declarations, includes, and global state
#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <chrono>
#include <thread>
#include <future>
#include <mutex>
#include <atomic>
#include <cstdlib>
#include <queue>
#include <condition_variable>
#include <functional>
#include <regex>
#include <stdexcept>
#include <cstdio>
#include <numeric>

// forward declarations
struct ASTNode;
struct Value;
struct FunctionValue;
class Environment;
class Interpreter;

// global thread-safe state
extern std::mutex output_mutex;
extern std::mutex task_registry_mutex;
extern std::unordered_map<std::string, bool> task_registry;

// mailbox for channel communication
struct Mailbox {
    std::queue<std::shared_ptr<Value>> messages;
    std::mutex lock;
    std::condition_variable cv;
    
    void send(std::shared_ptr<Value> msg);
    std::shared_ptr<Value> receive();
    std::shared_ptr<Value> receiveBlocking(double timeoutSeconds = -1);
};

extern std::unordered_map<std::string, std::shared_ptr<Mailbox>> channel_registry;
extern std::mutex channel_registry_mutex;

// utility functions
std::string trim(const std::string& s);
std::vector<std::string> split(const std::string& s, const std::string& delim);
std::string join(const std::vector<std::string>& v, const std::string& delim);
std::string toLower(const std::string& s);
std::string toUpper(const std::string& s);
std::string replaceAll(const std::string& source, const std::string& from, const std::string& to);
