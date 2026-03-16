// nibyo v1.0 beta - natural english programming language
// interpreter.cpp - high-performance runtime execution engine
#include "Interpreter.h"
#include "Lexer.h"
#include "Parser.h"
#include <fstream>
#include <sstream>
#include <thread>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <random>

// fast node cast - uses static_cast since we verify via nodetype
template<typename T>
static inline T* as(const std::shared_ptr<ASTNode>& node) {
    return static_cast<T*>(node.get());
}

Interpreter::Interpreter() : env(std::make_shared<Environment>()) {
    static std::once_flag seedFlag;
    std::call_once(seedFlag, []() {
        srand(static_cast<unsigned>(time(nullptr)));
    });
}

void Interpreter::checkTaskStatus() {
    if (!currentTaskName.empty()) {
        std::lock_guard<std::mutex> lock(task_registry_mutex);
        auto it = task_registry.find(currentTaskName);
        if (it != task_registry.end() && !it->second) {
            throw TaskStoppedException();
        }
    }
}

void Interpreter::run(const std::vector<std::shared_ptr<ASTNode>>& program) {
    for (auto& stmt : program) {
        execute(stmt);
    }
}

void Interpreter::runScript(const std::string& content, std::shared_ptr<Environment> targetEnv) {
    Lexer lexer(content);
    Parser parser(lexer.tokenize());
    auto program = parser.parse();
    
    auto prevEnv = env;
    env = targetEnv;
    for (auto& stmt : program) {
        execute(stmt);
    }
    env = prevEnv;
}

std::string Interpreter::httpGet(const std::string& url) {
    std::string cmd;
    #ifdef _WIN32
    cmd = "powershell -Command \"(Invoke-WebRequest -Uri '" + url + "' -UseBasicParsing).Content\" 2>nul";
    #else
    cmd = "curl -s \"" + url + "\" 2>/dev/null";
    #endif
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    
    std::string result;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
    pclose(pipe);
    return result;
}

std::string Interpreter::httpPost(const std::string& url, const std::string& body) {
    std::string cmd;
    #ifdef _WIN32
    cmd = "powershell -Command \"(Invoke-WebRequest -Uri '" + url + "' -Method POST -Body '" + body + "' -ContentType 'application/json' -UseBasicParsing).Content\" 2>nul";
    #else
    cmd = "curl -s -X POST -H 'Content-Type: application/json' -d '" + body + "' \"" + url + "\" 2>/dev/null";
    #endif
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    
    std::string result;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
    pclose(pipe);
    return result;
}

// robust json parser
std::shared_ptr<Value> Interpreter::parseJson(const std::string& json) {
    size_t pos = 0;
    return parseJsonValue(json, pos);
}

std::shared_ptr<Value> Interpreter::parseJsonValue(const std::string& json, size_t& pos) {
    skipJsonWhitespace(json, pos);
    if (pos >= json.length()) {
        return std::make_shared<Value>();
    }
    
    char c = json[pos];
    
    // object
    if (c == '{') {
        pos++;
        auto result = std::make_shared<Value>();
        result->type = Value::MAP;
        skipJsonWhitespace(json, pos);
        
        if (pos < json.length() && json[pos] == '}') { pos++; return result; }
        
        while (pos < json.length()) {
            skipJsonWhitespace(json, pos);
            if (json[pos] != '"') break;
            std::string key = parseJsonString(json, pos);
            skipJsonWhitespace(json, pos);
            if (pos < json.length() && json[pos] == ':') pos++;
            result->mapData[key] = parseJsonValue(json, pos);
            skipJsonWhitespace(json, pos);
            if (pos < json.length() && json[pos] == ',') { pos++; continue; }
            break;
        }
        if (pos < json.length() && json[pos] == '}') pos++;
        return result;
    }
    
    // array
    if (c == '[') {
        pos++;
        auto result = std::make_shared<Value>();
        result->type = Value::ARRAY;
        skipJsonWhitespace(json, pos);
        
        if (pos < json.length() && json[pos] == ']') { pos++; return result; }
        
        while (pos < json.length()) {
            result->array.push_back(parseJsonValue(json, pos));
            skipJsonWhitespace(json, pos);
            if (pos < json.length() && json[pos] == ',') { pos++; continue; }
            break;
        }
        if (pos < json.length() && json[pos] == ']') pos++;
        return result;
    }
    
    // string
    if (c == '"') {
        return std::make_shared<Value>(parseJsonString(json, pos));
    }
    
    // boolean / null
    if (json.compare(pos, 4, "true") == 0) { pos += 4; return std::make_shared<Value>(true); }
    if (json.compare(pos, 5, "false") == 0) { pos += 5; return std::make_shared<Value>(false); }
    if (json.compare(pos, 4, "null") == 0) { pos += 4; return std::make_shared<Value>(); }
    
    // number
    size_t start = pos;
    if (pos < json.length() && (json[pos] == '-' || json[pos] == '+')) pos++;
    while (pos < json.length() && (std::isdigit(json[pos]) || json[pos] == '.' || json[pos] == 'e' || json[pos] == 'E' || json[pos] == '+' || json[pos] == '-')) pos++;
    if (pos > start) {
        try {
            return std::make_shared<Value>(std::stod(json.substr(start, pos - start)));
        } catch (...) {}
    }
    
    return std::make_shared<Value>();
}

std::string Interpreter::parseJsonString(const std::string& json, size_t& pos) {
    if (pos >= json.length() || json[pos] != '"') return "";
    pos++; // skip opening quote
    std::string result;
    while (pos < json.length() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.length()) {
            pos++;
            switch (json[pos]) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                default: result += json[pos]; break;
            }
            pos++;
        } else {
            result += json[pos++];
        }
    }
    if (pos < json.length()) pos++; // skip closing quote
    return result;
}

void Interpreter::skipJsonWhitespace(const std::string& json, size_t& pos) {
    while (pos < json.length() && std::isspace(json[pos])) pos++;
}

std::string Interpreter::stringifyValue(std::shared_ptr<Value> val) {
    if (!val || val->type == Value::NONE) return "null";
    if (val->type == Value::MAP) {
        std::string result = "{";
        bool first = true;
        for (auto& [k, v] : val->mapData) {
            if (!first) result += ",";
            result += "\"" + k + "\":" + stringifyValue(v);
            first = false;
        }
        return result + "}";
    }
    if (val->type == Value::ARRAY) {
        std::string result = "[";
        for (size_t i = 0; i < val->array.size(); i++) {
            if (i > 0) result += ",";
            result += stringifyValue(val->array[i]);
        }
        return result + "]";
    }
    if (val->type == Value::TEXT) return "\"" + val->text + "\"";
    if (val->type == Value::BOOLEAN) return val->boolean ? "true" : "false";
    if (val->type == Value::NUMBER) return val->toString();
    return "null";
}

// high-performance expression evaluation (switch-based dispatch)
std::shared_ptr<Value> Interpreter::eval(std::shared_ptr<ASTNode> node) {
    checkTaskStatus();
    
    if (!node) return std::make_shared<Value>();
    
    switch (node->nodeType) {
    
    // literals
    case NodeType::Number:
        return std::make_shared<Value>(as<NumberNode>(node)->value);
    
    case NodeType::Text:
        return std::make_shared<Value>(as<TextNode>(node)->value);
    
    case NodeType::Bool:
        return std::make_shared<Value>(as<BoolNode>(node)->value);
    
    case NodeType::None:
        return std::make_shared<Value>();
    
    case NodeType::Identifier:
        return env->get(as<IdentifierNode>(node)->name);
    
    // array literal
    case NodeType::Array: {
        auto n = as<ArrayNode>(node);
        auto arr = std::make_shared<Value>();
        arr->type = Value::ARRAY;
        arr->array.reserve(n->elements.size());
        for (auto& elem : n->elements) {
            arr->array.push_back(eval(elem));
        }
        return arr;
    }
    
    // binary operations
    case NodeType::BinaryOp: {
        auto n = as<BinaryOpNode>(node);
        auto left = eval(n->left);
        auto right = eval(n->right);
        const auto& op = n->op;
        
        if (op == "+") {
            if (left->type == Value::TEXT || right->type == Value::TEXT)
                return std::make_shared<Value>(left->toString() + right->toString());
            return std::make_shared<Value>(left->number + right->number);
        }
        if (op == "-") return std::make_shared<Value>(left->number - right->number);
        if (op == "*") return std::make_shared<Value>(left->number * right->number);
        if (op == "/") {
            if (right->number == 0) throw std::runtime_error("Cannot divide by zero");
            return std::make_shared<Value>(left->number / right->number);
        }
        if (op == "%") return std::make_shared<Value>(fmod(left->number, right->number));
        if (op == "==") return std::make_shared<Value>(left->toString() == right->toString());
        if (op == "!=") return std::make_shared<Value>(left->toString() != right->toString());
        if (op == ">") return std::make_shared<Value>(left->number > right->number);
        if (op == "<") return std::make_shared<Value>(left->number < right->number);
        if (op == ">=") return std::make_shared<Value>(left->number >= right->number);
        if (op == "<=") return std::make_shared<Value>(left->number <= right->number);
        if (op == "and") return std::make_shared<Value>(left->isTruthy() && right->isTruthy());
        if (op == "or") return std::make_shared<Value>(left->isTruthy() || right->isTruthy());
        return std::make_shared<Value>();
    }
    
    // unary operations
    case NodeType::UnaryOp: {
        auto n = as<UnaryOpNode>(node);
        auto operand = eval(n->operand);
        if (n->op == "not") return std::make_shared<Value>(!operand->isTruthy());
        if (n->op == "-") return std::make_shared<Value>(-operand->number);
        return std::make_shared<Value>();
    }
    
    // field access
    case NodeType::GetField: {
        auto n = as<GetFieldNode>(node);
        auto obj = eval(n->object);
        if (obj->type != Value::MAP) {
            throw std::runtime_error("Cannot access property '" + n->field + "' of non-object");
        }
        auto it = obj->mapData.find(n->field);
        if (it != obj->mapData.end()) return it->second;
        return std::make_shared<Value>();
    }
    
    // array operations
    case NodeType::GetFirst: {
        auto arr = env->get(as<GetFirstNode>(node)->arrayName);
        if (arr->array.empty()) throw std::runtime_error("Cannot get first item of empty list");
        return arr->array.front();
    }
    case NodeType::GetLast: {
        auto arr = env->get(as<GetLastNode>(node)->arrayName);
        if (arr->array.empty()) throw std::runtime_error("Cannot get last item of empty list");
        return arr->array.back();
    }
    case NodeType::GetValueAt: {
        auto n = as<GetValueAtNode>(node);
        auto arr = env->get(n->arrayName);
        int idx = static_cast<int>(eval(n->index)->number);
        if (idx < 0 || idx >= static_cast<int>(arr->array.size()))
            throw std::runtime_error("Index " + std::to_string(idx) + " out of bounds (list has " + std::to_string(arr->array.size()) + " items)");
        return arr->array[idx];
    }
    case NodeType::LengthOf: {
        auto val = eval(as<LengthOfNode>(node)->target);
        if (val->type == Value::ARRAY) return std::make_shared<Value>(static_cast<double>(val->array.size()));
        if (val->type == Value::TEXT) return std::make_shared<Value>(static_cast<double>(val->text.length()));
        if (val->type == Value::MAP) return std::make_shared<Value>(static_cast<double>(val->mapData.size()));
        return std::make_shared<Value>(0.0);
    }
    case NodeType::GetIndex: {
        auto n = as<GetIndexNode>(node);
        auto arr = env->get(n->arrayName);
        auto val = eval(n->value);
        std::string target = val->toString();
        for (size_t i = 0; i < arr->array.size(); i++) {
            if (arr->array[i]->toString() == target) return std::make_shared<Value>(static_cast<double>(i));
        }
        return std::make_shared<Value>(-1.0);
    }
    
    // math operations
    case NodeType::MathOp: {
        auto n = as<MathOpNode>(node);
        double a = eval(n->arg1)->number;
        const auto& op = n->op;
        if (op == "floor") return std::make_shared<Value>(floor(a));
        if (op == "ceiling") return std::make_shared<Value>(ceil(a));
        if (op == "round") return std::make_shared<Value>(round(a));
        if (op == "abs") return std::make_shared<Value>(fabs(a));
        if (op == "sqrt") return std::make_shared<Value>(sqrt(a));
        if (op == "sin") return std::make_shared<Value>(sin(a));
        if (op == "cos") return std::make_shared<Value>(cos(a));
        if (op == "tan") return std::make_shared<Value>(tan(a));
        if (op == "log") return std::make_shared<Value>(log(a));
        if (op == "log10") return std::make_shared<Value>(log10(a));
        if (op == "pow" && n->arg2) {
            return std::make_shared<Value>(pow(a, eval(n->arg2)->number));
        }
        return std::make_shared<Value>(a);
    }
    
    // random
    case NodeType::Random: {
        auto n = as<RandomNode>(node);
        int minVal = static_cast<int>(eval(n->min)->number);
        int maxVal = static_cast<int>(eval(n->max)->number);
        if (maxVal < minVal) std::swap(minVal, maxVal);
        return std::make_shared<Value>(static_cast<double>(minVal + rand() % (maxVal - minVal + 1)));
    }
    
    // time
    case NodeType::CurrentTime:
        return std::make_shared<Value>(static_cast<double>(time(nullptr)));
    
    case NodeType::Date: {
        auto n = as<DateNode>(node);
        time_t now = time(nullptr);
        tm* t = localtime(&now);
        if (n->part == "year") return std::make_shared<Value>(static_cast<double>(t->tm_year + 1900));
        if (n->part == "month") return std::make_shared<Value>(static_cast<double>(t->tm_mon + 1));
        if (n->part == "day") return std::make_shared<Value>(static_cast<double>(t->tm_mday));
        if (n->part == "hour") return std::make_shared<Value>(static_cast<double>(t->tm_hour));
        if (n->part == "minute") return std::make_shared<Value>(static_cast<double>(t->tm_min));
        if (n->part == "second") return std::make_shared<Value>(static_cast<double>(t->tm_sec));
        return std::make_shared<Value>(0.0);
    }
    
    // aggregate functions
    case NodeType::Max: {
        auto arr = env->get(as<MaxNode>(node)->arrayName);
        if (arr->array.empty()) throw std::runtime_error("Cannot get maximum of empty list");
        double mx = arr->array[0]->number;
        for (size_t i = 1; i < arr->array.size(); i++) if (arr->array[i]->number > mx) mx = arr->array[i]->number;
        return std::make_shared<Value>(mx);
    }
    case NodeType::Min: {
        auto arr = env->get(as<MinNode>(node)->arrayName);
        if (arr->array.empty()) throw std::runtime_error("Cannot get minimum of empty list");
        double mn = arr->array[0]->number;
        for (size_t i = 1; i < arr->array.size(); i++) if (arr->array[i]->number < mn) mn = arr->array[i]->number;
        return std::make_shared<Value>(mn);
    }
    case NodeType::Sum: {
        auto arr = env->get(as<SumNode>(node)->arrayName);
        double sum = 0;
        for (auto& v : arr->array) sum += v->number;
        return std::make_shared<Value>(sum);
    }
    case NodeType::Average: {
        auto arr = env->get(as<AverageNode>(node)->arrayName);
        if (arr->array.empty()) throw std::runtime_error("Cannot get average of empty list");
        double sum = 0;
        for (auto& v : arr->array) sum += v->number;
        return std::make_shared<Value>(sum / arr->array.size());
    }
    
    // keys/values
    case NodeType::Keys: {
        auto obj = env->get(as<KeysNode>(node)->mapName);
        auto result = std::make_shared<Value>();
        result->type = Value::ARRAY;
        for (auto& [k, v] : obj->mapData) {
            result->array.push_back(std::make_shared<Value>(k));
        }
        return result;
    }
    case NodeType::Values: {
        auto obj = env->get(as<ValuesNode>(node)->mapName);
        auto result = std::make_shared<Value>();
        result->type = Value::ARRAY;
        for (auto& [k, v] : obj->mapData) {
            result->array.push_back(v);
        }
        return result;
    }
    
    // string operations
    case NodeType::Uppercase:
        return std::make_shared<Value>(toUpper(eval(as<UppercaseNode>(node)->text)->text));
    case NodeType::Lowercase:
        return std::make_shared<Value>(toLower(eval(as<LowercaseNode>(node)->text)->text));
    case NodeType::Trim:
        return std::make_shared<Value>(trim(eval(as<TrimNode>(node)->text)->text));
    
    case NodeType::Split: {
        auto n = as<SplitNode>(node);
        std::string text = eval(n->text)->text;
        std::string delim = eval(n->delimiter)->text;
        auto result = std::make_shared<Value>();
        result->type = Value::ARRAY;
        for (auto& part : split(text, delim)) {
            result->array.push_back(std::make_shared<Value>(part));
        }
        return result;
    }
    case NodeType::Join: {
        auto n = as<JoinNode>(node);
        auto arr = env->get(n->arrayName);
        std::string delim = eval(n->delimiter)->text;
        std::string result;
        for (size_t i = 0; i < arr->array.size(); i++) {
            if (i > 0) result += delim;
            result += arr->array[i]->toString();
        }
        return std::make_shared<Value>(result);
    }
    case NodeType::JoinedWith: {
        auto n = as<JoinedWithNode>(node);
        return std::make_shared<Value>(eval(n->left)->toString() + eval(n->right)->toString());
    }
    
    case NodeType::Replace: {
        auto n = as<ReplaceNode>(node);
        std::string text = eval(n->text)->text;
        std::string oldStr = eval(n->oldStr)->text;
        std::string newStr = eval(n->newStr)->text;
        return std::make_shared<Value>(replaceAll(text, oldStr, newStr));
    }
    
    case NodeType::Substring: {
        auto n = as<SubstringNode>(node);
        std::string text = eval(n->text)->text;
        int start = static_cast<int>(eval(n->start)->number);
        int len = n->length ? static_cast<int>(eval(n->length)->number) : static_cast<int>(text.length()) - start;
        if (start < 0) start = 0;
        if (start >= static_cast<int>(text.length())) return std::make_shared<Value>(std::string(""));
        return std::make_shared<Value>(text.substr(start, len));
    }
    
    case NodeType::Contains: {
        auto n = as<ContainsNode>(node);
        std::string text = eval(n->text)->toString();
        std::string search = eval(n->search)->toString();
        return std::make_shared<Value>(text.find(search) != std::string::npos);
    }
    
    case NodeType::StartsWith: {
        auto n = as<StartsWithNode>(node);
        std::string text = eval(n->text)->text;
        std::string prefix = eval(n->prefix)->text;
        return std::make_shared<Value>(text.length() >= prefix.length() && text.compare(0, prefix.length(), prefix) == 0);
    }
    
    case NodeType::EndsWith: {
        auto n = as<EndsWithNode>(node);
        std::string text = eval(n->text)->text;
        std::string suffix = eval(n->suffix)->text;
        return std::make_shared<Value>(text.length() >= suffix.length() && text.compare(text.length() - suffix.length(), suffix.length(), suffix) == 0);
    }
    
    // file operations
    case NodeType::ReadFile: {
        auto n = as<ReadFileNode>(node);
        std::string path = eval(n->filepath)->text;
        std::ifstream file(path);
        if (!file) throw std::runtime_error("Cannot read file: " + path);
        
        if (n->asLines) {
            auto result = std::make_shared<Value>();
            result->type = Value::ARRAY;
            std::string line;
            while (std::getline(file, line)) {
                result->array.push_back(std::make_shared<Value>(line));
            }
            return result;
        } else {
            std::stringstream ss;
            ss << file.rdbuf();
            return std::make_shared<Value>(ss.str());
        }
    }
    case NodeType::FileExists: {
        std::string path = eval(as<FileExistsNode>(node)->filepath)->text;
        std::ifstream file(path);
        return std::make_shared<Value>(file.good());
    }
    
    // http
    case NodeType::HttpGet:
        return std::make_shared<Value>(httpGet(eval(as<HttpGetNode>(node)->url)->text));
    
    case NodeType::HttpPost: {
        auto n = as<HttpPostNode>(node);
        std::string url = eval(n->url)->text;
        std::string body = n->body ? eval(n->body)->toString() : "";
        return std::make_shared<Value>(httpPost(url, body));
    }
    
    // json
    case NodeType::Parse:
        return parseJson(eval(as<ParseNode>(node)->json)->text);
    
    case NodeType::Stringify:
        return std::make_shared<Value>(stringifyValue(eval(as<StringifyNode>(node)->object)));
    
    // environment variable
    case NodeType::GetEnvVar: {
        std::string varName = eval(as<GetEnvVarNode>(node)->varName)->text;
        const char* val = std::getenv(varName.c_str());
        return std::make_shared<Value>(val ? std::string(val) : std::string(""));
    }
    
    // function call
    case NodeType::Call: {
        auto n = as<CallNode>(node);
        auto func = env->get(n->name);
        if (func->type != Value::FUNCTION)
            throw std::runtime_error("'" + n->name + "' is not a function");
        
        auto funcEnv = std::make_shared<Environment>(func->func->closure);
        for (size_t i = 0; i < func->func->params.size(); i++) {
            auto argVal = i < n->args.size() ? eval(n->args[i]) : std::make_shared<Value>();
            funcEnv->define(func->func->params[i], argVal);
        }
        
        auto prevEnv = env;
        env = funcEnv;
        callStack.push_back(n->name);
        
        std::shared_ptr<Value> result;
        try {
            for (auto& stmt : func->func->body) {
                execute(stmt);
            }
            result = std::make_shared<Value>();
        } catch (ReturnException& e) {
            result = e.value;
        }
        
        callStack.pop_back();
        env = prevEnv;
        return result;
    }
    
    // lambda
    case NodeType::Lambda: {
        auto n = as<LambdaNode>(node);
        auto funcVal = std::make_shared<Value>();
        funcVal->type = Value::FUNCTION;
        funcVal->func = std::make_shared<FunctionValue>();
        funcVal->func->params = n->params;
        funcVal->func->body = n->body;
        funcVal->func->closure = env;
        return funcVal;
    }
    
    // call function value
    case NodeType::CallFunc: {
        auto n = as<CallFuncNode>(node);
        auto func = eval(n->func);
        if (func->type != Value::FUNCTION)
            throw std::runtime_error("Value is not callable");
        
        auto funcEnv = std::make_shared<Environment>(func->func->closure);
        for (size_t i = 0; i < func->func->params.size(); i++) {
            auto argVal = i < n->args.size() ? eval(n->args[i]) : std::make_shared<Value>();
            funcEnv->define(func->func->params[i], argVal);
        }
        
        auto prevEnv = env;
        env = funcEnv;
        
        std::shared_ptr<Value> result;
        try {
            for (auto& stmt : func->func->body) execute(stmt);
            result = std::make_shared<Value>();
        } catch (ReturnException& e) {
            result = e.value;
        }
        
        env = prevEnv;
        return result;
    }
    
    // await
    case NodeType::Await: {
        auto n = as<AwaitNode>(node);
        auto task = eval(n->task);
        if (task->type == Value::FUTURE && task->futureVal.valid()) {
            return task->futureVal.get();
        }
        return task;
    }
    
    default:
        return std::make_shared<Value>();
    }
}

// high-performance statement execution (switch-based dispatch)
void Interpreter::execute(std::shared_ptr<ASTNode> node) {
    checkTaskStatus();
    
    if (!node) return;
    
    switch (node->nodeType) {
    
    // display
    case NodeType::Display: {
        auto val = eval(as<DisplayNode>(node)->value);
        std::lock_guard<std::mutex> lock(output_mutex);
        std::cout << val->toString() << std::endl;
        return;
    }
    
    // set variable
    case NodeType::Set: {
        auto n = as<SetNode>(node);
        env->set(n->name, eval(n->value));
        return;
    }
    
    // set field
    case NodeType::SetField: {
        auto n = as<SetFieldNode>(node);
        auto obj = eval(n->object);
        if (obj->type != Value::MAP)
            throw std::runtime_error("Cannot set property on non-object");
        obj->mapData[n->field] = eval(n->value);
        return;
    }
    
    // user input
    case NodeType::UserInput: {
        auto n = as<UserInputNode>(node);
        {
            std::lock_guard<std::mutex> lock(output_mutex);
            std::cout << eval(n->prompt)->toString();
        }
        std::string input;
        std::getline(std::cin, input);
        
        std::shared_ptr<Value> val;
        if (n->targetType == "number" || n->targetType == "integer") {
            try { val = std::make_shared<Value>(std::stod(input)); }
            catch (...) { val = std::make_shared<Value>(0.0); }
        } else {
            val = std::make_shared<Value>(input);
        }
        env->set(n->targetVar, val);
        return;
    }
    
    // if statement
    case NodeType::If: {
        auto n = as<IfNode>(node);
        if (eval(n->condition)->isTruthy()) {
            for (auto& stmt : n->thenBlock) execute(stmt);
        } else {
            for (auto& stmt : n->elseBlock) execute(stmt);
        }
        return;
    }
    
    // while loop
    case NodeType::While: {
        auto n = as<WhileNode>(node);
        while (eval(n->condition)->isTruthy()) {
            try {
                for (auto& stmt : n->body) execute(stmt);
            } catch (BreakException&) { break; }
            catch (ContinueException&) { continue; }
        }
        return;
    }
    
    // repeat loop
    case NodeType::Repeat: {
        auto n = as<RepeatNode>(node);
        int count = static_cast<int>(eval(n->count)->number);
        for (int i = 0; i < count; i++) {
            try {
                for (auto& stmt : n->body) execute(stmt);
            } catch (BreakException&) { break; }
            catch (ContinueException&) { continue; }
        }
        return;
    }
    
    // iterate loop
    case NodeType::Iterate: {
        auto n = as<IterateNode>(node);
        int start = static_cast<int>(eval(n->start)->number);
        int end = static_cast<int>(eval(n->end)->number);
        for (int i = start; i <= end; i++) {
            env->set(n->variable, std::make_shared<Value>(static_cast<double>(i)));
            try {
                for (auto& stmt : n->body) execute(stmt);
            } catch (BreakException&) { break; }
            catch (ContinueException&) { continue; }
        }
        return;
    }
    
    // for each loop
    case NodeType::ForEach: {
        auto n = as<ForEachNode>(node);
        auto collection = eval(n->collection);
        if (collection->type == Value::ARRAY) {
            for (auto& item : collection->array) {
                env->set(n->variable, item);
                try {
                    for (auto& stmt : n->body) execute(stmt);
                } catch (BreakException&) { break; }
                catch (ContinueException&) { continue; }
            }
        } else if (collection->type == Value::MAP) {
            for (auto& [key, val] : collection->mapData) {
                env->set(n->variable, std::make_shared<Value>(key));
                try {
                    for (auto& stmt : n->body) execute(stmt);
                } catch (BreakException&) { break; }
                catch (ContinueException&) { continue; }
            }
        } else {
            throw std::runtime_error("For each requires a list or object");
        }
        return;
    }
    
    // function definition
    case NodeType::FunctionDef: {
        auto n = as<FunctionDefNode>(node);
        auto funcVal = std::make_shared<Value>();
        funcVal->type = Value::FUNCTION;
        funcVal->func = std::make_shared<FunctionValue>();
        funcVal->func->params = n->params;
        funcVal->func->body = n->body;
        funcVal->func->closure = env;
        env->set(n->name, funcVal);
        return;
    }
    
    // return
    case NodeType::Return: {
        auto n = as<ReturnNode>(node);
        throw ReturnException(n->value ? eval(n->value) : std::make_shared<Value>());
    }
    
    // break, continue, exit
    case NodeType::Break: throw BreakException();
    case NodeType::Continue: throw ContinueException();
    case NodeType::Exit: exit(0);
    
    // array operations
    case NodeType::AddToArray: {
        auto n = as<AddToArrayNode>(node);
        auto arr = env->get(n->arrayName);
        arr->array.push_back(eval(n->value));
        return;
    }
    case NodeType::RemoveFromArray: {
        auto n = as<RemoveFromArrayNode>(node);
        auto arr = env->get(n->arrayName);
        auto val = eval(n->value);
        std::string target = val->toString();
        arr->array.erase(
            std::remove_if(arr->array.begin(), arr->array.end(),
                [&](auto& v) { return v->toString() == target; }),
            arr->array.end()
        );
        return;
    }
    case NodeType::Sort: {
        auto arr = env->get(as<SortNode>(node)->arrayName);
        std::sort(arr->array.begin(), arr->array.end(),
            [](auto& a, auto& b) {
                if (a->type == Value::NUMBER && b->type == Value::NUMBER)
                    return a->number < b->number;
                return a->toString() < b->toString();
            });
        return;
    }
    case NodeType::Reverse: {
        auto arr = env->get(as<ReverseNode>(node)->arrayName);
        std::reverse(arr->array.begin(), arr->array.end());
        return;
    }
    case NodeType::Unique: {
        auto arr = env->get(as<UniqueNode>(node)->arrayName);
        std::unordered_set<std::string> seen;
        std::vector<std::shared_ptr<Value>> unique;
        for (auto& v : arr->array) {
            std::string key = v->toString();
            if (seen.find(key) == seen.end()) {
                seen.insert(key);
                unique.push_back(v);
            }
        }
        arr->array = std::move(unique);
        return;
    }
    
    // file operations
    case NodeType::WriteFile: {
        auto n = as<WriteFileNode>(node);
        std::string path = eval(n->filepath)->text;
        std::string content = eval(n->content)->toString();
        std::ofstream file(path);
        if (!file) throw std::runtime_error("Cannot write to file: " + path);
        file << content;
        return;
    }
    case NodeType::AppendFile: {
        auto n = as<AppendFileNode>(node);
        std::string path = eval(n->filepath)->text;
        std::string content = eval(n->content)->toString();
        std::ofstream file(path, std::ios::app);
        if (!file) throw std::runtime_error("Cannot append to file: " + path);
        file << content;
        return;
    }
    case NodeType::DeleteFile: {
        std::string path = eval(as<DeleteFileNode>(node)->filepath)->text;
        std::remove(path.c_str());
        return;
    }
    
    // wait
    case NodeType::Wait: {
        double secs = eval(as<WaitNode>(node)->seconds)->number;
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(secs * 1000)));
        return;
    }
    
    // try-catch
    case NodeType::TryCatch: {
        auto n = as<TryCatchNode>(node);
        try {
            for (auto& stmt : n->tryBlock) execute(stmt);
        } catch (ReturnException&) {
            throw;
        } catch (BreakException&) {
            throw;
        } catch (ContinueException&) {
            throw;
        } catch (std::exception& e) {
            env->set(n->errorVar, std::make_shared<Value>(std::string(e.what())));
            for (auto& stmt : n->catchBlock) execute(stmt);
        }
        return;
    }
    
    // channels
    case NodeType::CreateChannel: {
        std::string name = as<CreateChannelNode>(node)->channelName;
        std::lock_guard<std::mutex> lock(channel_registry_mutex);
        channel_registry[name] = std::make_shared<Mailbox>();
        return;
    }
    case NodeType::SendToChannel: {
        auto n = as<SendToChannelNode>(node);
        auto msg = eval(n->message);
        std::lock_guard<std::mutex> lock(channel_registry_mutex);
        auto it = channel_registry.find(n->channelName);
        if (it == channel_registry.end())
            throw std::runtime_error("Channel '" + n->channelName + "' not found");
        it->second->send(msg);
        return;
    }
    case NodeType::ReceiveFromChannel: {
        auto n = as<ReceiveFromChannelNode>(node);
        std::shared_ptr<Mailbox> ch;
        {
            std::lock_guard<std::mutex> lock(channel_registry_mutex);
            auto it = channel_registry.find(n->channelName);
            if (it == channel_registry.end())
                throw std::runtime_error("Channel '" + n->channelName + "' not found");
            ch = it->second;
        }
        double timeout = n->timeout ? eval(n->timeout)->number : -1;
        auto msg = ch->receiveBlocking(timeout);
        env->set(n->targetVar, msg ? msg : std::make_shared<Value>());
        return;
    }
    
    // background task
    case NodeType::Spawn: {
        auto n = as<SpawnNode>(node);
        auto threadEnv = std::make_shared<Environment>(*env);
        std::string name = n->taskName;
        
        {
            std::lock_guard<std::mutex> lock(task_registry_mutex);
            task_registry[name] = true;
        }
        
        auto body = n->body;
        std::thread([threadEnv, body, name]() {
            Interpreter interp;
            interp.setEnvironment(threadEnv);
            interp.setTaskName(name);
            try {
                for (auto& stmt : body) interp.execute(stmt);
            } catch (TaskStoppedException&) {
            } catch (...) {}
        }).detach();
        return;
    }
    case NodeType::StopTask: {
        std::lock_guard<std::mutex> lock(task_registry_mutex);
        task_registry[as<StopTaskNode>(node)->taskName] = false;
        return;
    }
    
    // object creation
    case NodeType::CreateObject: {
        auto n = as<CreateObjectNode>(node);
        auto obj = std::make_shared<Value>();
        obj->type = Value::MAP;
        for (auto& [key, valNode] : n->properties) {
            obj->mapData[key] = eval(valNode);
        }
        env->set(n->name, obj);
        return;
    }
    
    // import
    case NodeType::Import: {
        auto n = as<ImportNode>(node);
        std::string path = eval(n->filepath)->text;
        std::ifstream file(path);
        if (!file) throw std::runtime_error("Cannot import file: " + path);
        std::stringstream ss;
        ss << file.rdbuf();
        runScript(ss.str(), env);
        return;
    }
    
    // run command
    case NodeType::RunCommand: {
        auto n = as<RunCommandNode>(node);
        std::string cmd = eval(n->command)->text;
        
        if (n->captureOutput) {
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) throw std::runtime_error("Cannot run command: " + cmd);
            std::string result;
            char buffer[4096];
            while (fgets(buffer, sizeof(buffer), pipe)) result += buffer;
            pclose(pipe);
            if (!n->targetVar.empty()) {
                env->set(n->targetVar, std::make_shared<Value>(result));
            }
        } else {
            int ret = system(cmd.c_str());
            if (!n->targetVar.empty()) {
                env->set(n->targetVar, std::make_shared<Value>(static_cast<double>(ret)));
            }
        }
        return;
    }
    
    // trace
    case NodeType::Trace: {
        std::lock_guard<std::mutex> lock(output_mutex);
        std::cout << "[Trace] Call stack:" << std::endl;
        for (size_t i = 0; i < callStack.size(); i++) {
            std::cout << "  " << i << ": " << callStack[i] << std::endl;
        }
        return;
    }
    
    // gui window
    case NodeType::GUIWindow: {
        auto n = as<GUIWindowNode>(node);
        std::string title = eval(n->title)->toString();
        int w = static_cast<int>(eval(n->width)->number);
        int h = static_cast<int>(eval(n->height)->number);
        
        #ifdef NIBYO_GUI_SUPPORT
        InitWindow(w, h, title.c_str());
        SetTargetFPS(60);
        while (!WindowShouldClose()) {
            BeginDrawing();
            ClearBackground(RAYWHITE);
            for (auto& stmt : n->body) execute(stmt);
            EndDrawing();
        }
        CloseWindow();
        #else
        std::lock_guard<std::mutex> lock(output_mutex);
        std::cout << "[GUI] Opened Window: '" << title << "' (" << w << "x" << h << ")" << std::endl;
        for (auto& stmt : n->body) execute(stmt);
        std::cout << "[GUI] Closed Window: '" << title << "'" << std::endl;
        #endif
        return;
    }
    
    // gui draw calls (simulation fallback)
    case NodeType::GUIDrawText: {
        auto n = as<GUIDrawTextNode>(node);
        std::string text = eval(n->text)->toString();
        int x = static_cast<int>(eval(n->x)->number);
        int y = static_cast<int>(eval(n->y)->number);
        int size = n->fontSize ? static_cast<int>(eval(n->fontSize)->number) : 20;
        #ifdef NIBYO_GUI_SUPPORT
        DrawText(text.c_str(), x, y, size, BLACK);
        #else
        std::lock_guard<std::mutex> lock(output_mutex);
        std::cout << "[GUI] Text: '" << text << "' at (" << x << "," << y << ") size=" << size << " color=" << n->color << std::endl;
        #endif
        return;
    }
    case NodeType::GUIDrawRect: {
        auto n = as<GUIDrawRectNode>(node);
        int x = static_cast<int>(eval(n->x)->number);
        int y = static_cast<int>(eval(n->y)->number);
        int w = static_cast<int>(eval(n->width)->number);
        int h = static_cast<int>(eval(n->height)->number);
        #ifndef NIBYO_GUI_SUPPORT
        std::lock_guard<std::mutex> lock(output_mutex);
        std::cout << "[GUI] Rectangle: (" << x << "," << y << ") " << w << "x" << h << " color=" << n->color << std::endl;
        #endif
        return;
    }
    case NodeType::GUIDrawCircle: {
        auto n = as<GUIDrawCircleNode>(node);
        int x = static_cast<int>(eval(n->x)->number);
        int y = static_cast<int>(eval(n->y)->number);
        int r = static_cast<int>(eval(n->radius)->number);
        #ifndef NIBYO_GUI_SUPPORT
        std::lock_guard<std::mutex> lock(output_mutex);
        std::cout << "[GUI] Circle: (" << x << "," << y << ") r=" << r << " color=" << n->color << std::endl;
        #endif
        return;
    }
    case NodeType::GUIButton: {
        auto n = as<GUIButtonNode>(node);
        std::string label = eval(n->label)->toString();
        #ifndef NIBYO_GUI_SUPPORT
        std::lock_guard<std::mutex> lock(output_mutex);
        std::cout << "[GUI] Button '" << n->name << "': " << label << std::endl;
        #endif
        return;
    }
    case NodeType::GUITextInput: {
        auto n = as<GUITextInputNode>(node);
        #ifndef NIBYO_GUI_SUPPORT
        std::lock_guard<std::mutex> lock(output_mutex);
        std::cout << "[GUI] TextInput '" << n->name << "'" << std::endl;
        #endif
        return;
    }
    case NodeType::GUIClose:
        return;
    
    default:
        // expression statements - evaluate for side effects
        eval(node);
        return;
    }
}
