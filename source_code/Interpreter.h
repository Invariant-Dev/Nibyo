// nibyo v1.0 beta - natural english programming language
// interpreter.h - interpreter declaration
#pragma once

#include "Common.h"
#include "Value.h"
#include "AST.h"
#include <memory>
#include <vector>
#include <string>

// control flow exceptions
class ReturnException : public std::exception {
public:
    std::shared_ptr<Value> value;
    explicit ReturnException(std::shared_ptr<Value> v) : value(v) {}
};

class BreakException : public std::exception {};
class ContinueException : public std::exception {};
class ExitException : public std::exception {};
class TaskStoppedException : public std::exception {};

class Interpreter {
private:
    std::shared_ptr<Environment> env;
    std::string currentTaskName;
    std::vector<std::string> callStack;
    
    // check if current task should stop
    void checkTaskStatus();
    
    // core evaluation
    std::shared_ptr<Value> eval(std::shared_ptr<ASTNode> node);
    void execute(std::shared_ptr<ASTNode> node);
    
    // helper methods
    std::string httpGet(const std::string& url);
    std::string httpPost(const std::string& url, const std::string& body);
    std::shared_ptr<Value> parseJson(const std::string& json);
    std::shared_ptr<Value> parseJsonValue(const std::string& json, size_t& pos);
    std::string parseJsonString(const std::string& json, size_t& pos);
    void skipJsonWhitespace(const std::string& json, size_t& pos);
    std::string stringifyValue(std::shared_ptr<Value> val);
    void runScript(const std::string& content, std::shared_ptr<Environment> targetEnv);
    
public:
    Interpreter();
    void run(const std::vector<std::shared_ptr<ASTNode>>& program);
    
    // for spawned tasks
    void setEnvironment(std::shared_ptr<Environment> e) { env = e; }
    void setTaskName(const std::string& name) { currentTaskName = name; }
};
