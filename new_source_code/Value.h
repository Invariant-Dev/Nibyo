// nexo v1.0 beta - natural english programming language
// value.h - core value types and environment (optimized)
#pragma once

#include "Common.h"
#include <unordered_map>
#include <unordered_set>

// string interning pool for memory efficiency
class StringPool {
    std::unordered_set<std::string> pool;
    std::mutex pool_mutex;
public:
    static StringPool& instance() {
        static StringPool inst;
        return inst;
    }
    
    const std::string& intern(const std::string& s) {
        std::lock_guard<std::mutex> lock(pool_mutex);
        return *pool.insert(s).first;
    }
};

// forward declaration
struct FunctionValue;

// core value type - represents any nexo value
struct Value {
    enum Type { 
        NONE, 
        NUMBER, 
        TEXT, 
        BOOLEAN, 
        ARRAY, 
        MAP, 
        FUNCTION, 
        FUTURE, 
        CHANNEL 
    } type = NONE;
    
    double number = 0;
    std::string text;
    bool boolean = false;
    std::vector<std::shared_ptr<Value>> array;
    std::unordered_map<std::string, std::shared_ptr<Value>> mapData;
    std::shared_ptr<FunctionValue> func;
    std::shared_future<std::shared_ptr<Value>> futureVal;
    std::shared_ptr<Mailbox> channel;
    
    // constructors
    Value() = default;
    explicit Value(double n) : type(NUMBER), number(n) {}
    explicit Value(const std::string& s) : type(TEXT), text(StringPool::instance().intern(s)) {}
    explicit Value(bool b) : type(BOOLEAN), boolean(b) {}
    
    // convert value to string representation
    std::string toString() const {
        switch (type) {
            case NUMBER: {
                if (number == static_cast<long long>(number)) {
                    return std::to_string(static_cast<long long>(number));
                }
                char buf[64];
                snprintf(buf, sizeof(buf), "%.10g", number);
                return std::string(buf);
            }
            case TEXT: 
                return text;
            case BOOLEAN: 
                return boolean ? "true" : "false";
            case ARRAY: {
                std::string r = "[";
                for (size_t i = 0; i < array.size(); i++) {
                    if (i > 0) r += ", ";
                    r += array[i]->toString();
                }
                return r + "]";
            }
            case MAP: {
                std::string r = "{";
                bool first = true;
                for (auto& [k, v] : mapData) {
                    if (!first) r += ", ";
                    r += k + ": " + v->toString();
                    first = false;
                }
                return r + "}";
            }
            case FUNCTION: 
                return "[Function]";
            case FUTURE: 
                return "[Background Task]";
            case CHANNEL: 
                return "[Channel]";
            case NONE:
            default: 
                return "none";
        }
    }
    
    // check if value is truthy
    bool isTruthy() const {
        switch (type) {
            case BOOLEAN: return boolean;
            case NUMBER: return number != 0;
            case TEXT: return !text.empty();
            case ARRAY: return !array.empty();
            case MAP: return !mapData.empty();
            case NONE: return false;
            default: return true;
        }
    }
};

// function value with closure support
struct FunctionValue {
    std::vector<std::string> params;
    std::vector<std::shared_ptr<ASTNode>> body;
    std::shared_ptr<class Environment> closure;
    std::function<std::shared_ptr<Value>(const std::vector<std::shared_ptr<Value>>&)> nativeImpl;
};

// environment for variable scoping (optimized with unordered_map)
class Environment {
    std::mutex env_mutex;
    
public:
    std::unordered_map<std::string, std::shared_ptr<Value>> vars;
    std::shared_ptr<Environment> parent;
    
    explicit Environment(std::shared_ptr<Environment> p = nullptr) : parent(p) {}
    
    // copy constructor for thread spawning
    Environment(const Environment& other) : vars(other.vars), parent(other.parent) {}
    
    // set a variable (updates parent if exists there)
    void set(const std::string& name, std::shared_ptr<Value> val) {
        std::lock_guard<std::mutex> lock(env_mutex);
        
        // check if variable exists in parent scope
        if (parent) {
            std::shared_ptr<Environment> current = parent;
            while (current) {
                auto it = current->vars.find(name);
                if (it != current->vars.end()) {
                    it->second = val;
                    return;
                }
                current = current->parent;
            }
        }
        
        vars[name] = val;
    }
    
    // define a new variable in current scope only
    void define(const std::string& name, std::shared_ptr<Value> val) {
        std::lock_guard<std::mutex> lock(env_mutex);
        vars[name] = val;
    }
    
    // get a variable (searches parent scopes)
    std::shared_ptr<Value> get(const std::string& name) {
        std::lock_guard<std::mutex> lock(env_mutex);
        
        auto it = vars.find(name);
        if (it != vars.end()) {
            return it->second;
        }
        
        if (parent) {
            return parent->get(name);
        }
        
        throw std::runtime_error(
            "Variable '" + name + "' not found. " +
            "Tip: use 'set the " + name + " to ...' to create it."
        );
    }
    
    // check if variable exists
    bool has(const std::string& name) {
        std::lock_guard<std::mutex> lock(env_mutex);
        
        if (vars.find(name) != vars.end()) return true;
        if (parent) return parent->has(name);
        return false;
    }
};
