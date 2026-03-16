// project nibyo v4.0 -> full-featured esoteric language
// compile: g++ -std=c++17 -o3 nibyo.cpp -o nibyo.exe -static -static-libgcc -static-libstdc++ -pthread
// usage: ./nibyo.exe program.nb

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
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
#include <unordered_set>
#include <functional>

using namespace std;

// forward declarations
struct ASTNode;
struct Value;
class Environment;

struct Mailbox {
    queue<shared_ptr<Value>> messages;
    mutex lock;
    condition_variable cv;
    
    void send(shared_ptr<Value> msg) {
        {
            lock_guard<mutex> guard(lock);
            messages.push(msg);
        }
        cv.notify_one();
    }
    
    shared_ptr<Value> receive() {
        lock_guard<mutex> guard(lock);
        if(messages.empty()) return nullptr;
        auto msg = messages.front();
        messages.pop();
        return msg;
    }

    shared_ptr<Value> receiveBlocking(double timeoutSeconds = -1) {
        unique_lock<mutex> ulock(lock);
        if(timeoutSeconds < 0) {
            cv.wait(ulock, [&]{ return !messages.empty(); });
        } else {
            cv.wait_for(ulock, chrono::milliseconds((int)(timeoutSeconds * 1000)), [&]{ return !messages.empty(); });
        }
        if(messages.empty()) return nullptr;
        auto msg = messages.front();
        messages.pop();
        return msg;
    }
};

// global state
mutex output_mutex;
mutex task_registry_mutex;
map<string, bool> task_registry;
map<string, shared_ptr<Mailbox>> channel_registry;
mutex channel_registry_mutex;

struct FunctionValue {
    vector<string> params;
    vector<shared_ptr<ASTNode>> body;
    shared_ptr<Environment> closure;
    function<shared_ptr<Value>(const vector<shared_ptr<Value>>&)> nativeImpl;
};

// small string interning pool to save memory and speed up comparisons
class StringPool {
    unordered_set<string> pool;
public:
    static StringPool& instance() {
        static StringPool inst;
        return inst;
    }
    const string& intern(const string& s) {
        auto it = pool.insert(s).first;
        return *it;
    }
};

struct Value {
    enum Type { NUMBER, TEXT, BOOLEAN, ARRAY, MAP, FUNCTION, FUTURE, NONE, CHANNEL } type = NONE;
    double number = 0;
    string text;
    bool boolean = false;
    vector<shared_ptr<Value>> array;
    map<string, shared_ptr<Value>> mapData;
    shared_ptr<FunctionValue> func;
    shared_future<shared_ptr<Value>> futureVal;
    shared_ptr<Mailbox> channel;
    
    Value() = default;
    Value(double n) : type(NUMBER), number(n) {}
    Value(const string& s) : type(TEXT), text(StringPool::instance().intern(s)) {}
    Value(bool b) : type(BOOLEAN), boolean(b) {}
    
    string toString() const {
        if(type == NUMBER) {
            string s = to_string(number);
            s.erase(s.find_last_not_of('0') + 1);
            if(s.back() == '.') s.pop_back();
            return s;
        }
        if(type == TEXT) return text;
        if(type == BOOLEAN) return boolean ? "true" : "false";
        if(type == ARRAY) {
            string r = "[";
            for(size_t i = 0; i < array.size(); i++) {
                if(i > 0) r += ", ";
                r += array[i]->toString();
            }
            return r + "]";
        }
        if(type == MAP) return "[Object]";
        if(type == FUNCTION) return "[Function]";
        if(type == FUTURE) return "[Async Task]";
        if(type == CHANNEL) return "[Channel]";
        return "none";
    }
};

class Environment {
    mutex env_mutex; 
public:
    map<string, shared_ptr<Value>> vars;
    shared_ptr<Environment> parent;
    
    Environment(shared_ptr<Environment> p = nullptr) : parent(p) {}

    Environment(const Environment& other) : vars(other.vars), parent(other.parent) {
        // we manually copy vars and parent, but let the mutex be default-constructed (unlocked).
        // this allows creating a copy of the environment for new threads.
    }
    
    void set(const string& name, shared_ptr<Value> val) {
        lock_guard<mutex> lock(env_mutex);
        if(parent && parent->vars.count(name)) {
            parent->set(name, val);
        } else {
            vars[name] = val;
        }
    }
    
    shared_ptr<Value> get(const string& name) {
        lock_guard<mutex> lock(env_mutex);
        if(vars.count(name)) return vars[name];
        if(parent) return parent->get(name);
        throw runtime_error("Variable '" + name + "' not found. Tip: set it using 'set " + name + " to ...'");
    }
};

// ast nodes
struct ASTNode { virtual ~ASTNode() = default; };
struct NumberNode : ASTNode { double value; };
struct TextNode : ASTNode { string value; };
struct BoolNode : ASTNode { bool value; };
struct IdentifierNode : ASTNode { string name; };
struct BinaryOpNode : ASTNode { shared_ptr<ASTNode> left, right; string op; };
struct UnaryOpNode : ASTNode { shared_ptr<ASTNode> operand; string op; };
struct SetNode : ASTNode { string name; shared_ptr<ASTNode> value; };
struct SetFieldNode : ASTNode { shared_ptr<ASTNode> object; string field; shared_ptr<ASTNode> value; };
struct DisplayNode : ASTNode { shared_ptr<ASTNode> value; };
struct IfNode : ASTNode { shared_ptr<ASTNode> condition; vector<shared_ptr<ASTNode>> thenBlock, elseBlock; };
struct WhileNode : ASTNode { shared_ptr<ASTNode> condition; vector<shared_ptr<ASTNode>> body; };
struct IterateNode : ASTNode { string variable; shared_ptr<ASTNode> start, end; vector<shared_ptr<ASTNode>> body; };
struct ForEachNode : ASTNode { string variable; shared_ptr<ASTNode> array; vector<shared_ptr<ASTNode>> body; };
struct FunctionDefNode : ASTNode { string name; vector<string> params; vector<shared_ptr<ASTNode>> body; };
struct CallNode : ASTNode { string name; vector<shared_ptr<ASTNode>> args; };
struct CallFuncNode : ASTNode { shared_ptr<ASTNode> func; vector<shared_ptr<ASTNode>> args; };
struct ReturnNode : ASTNode { shared_ptr<ASTNode> value; };
struct ArrayNode : ASTNode { vector<shared_ptr<ASTNode>> elements; };
struct AddToArrayNode : ASTNode { shared_ptr<ASTNode> value; string arrayName; };
struct RemoveFromArrayNode : ASTNode { shared_ptr<ASTNode> value; string arrayName; };
struct GetIndexNode : ASTNode { shared_ptr<ASTNode> value; string arrayName; };
struct GetValueAtNode : ASTNode { shared_ptr<ASTNode> index; string arrayName; };
struct GetLastNode : ASTNode { string arrayName; };
struct LengthOfNode : ASTNode { string arrayName; };
struct UserInputNode : ASTNode { shared_ptr<ASTNode> prompt; string targetType = "text"; };
struct ReadFileNode : ASTNode { shared_ptr<ASTNode> filepath; bool asLines = false; };
struct WriteFileNode : ASTNode { shared_ptr<ASTNode> filepath, content; };
struct AppendFileNode : ASTNode { shared_ptr<ASTNode> filepath, content; };
struct FileExistsNode : ASTNode { shared_ptr<ASTNode> filepath; };
struct DeleteFileNode : ASTNode { shared_ptr<ASTNode> filepath; };
struct SplitNode : ASTNode { shared_ptr<ASTNode> text, delimiter; };
struct JoinNode : ASTNode { string arrayName; shared_ptr<ASTNode> delimiter; };
struct ReplaceNode : ASTNode { shared_ptr<ASTNode> text, oldStr, newStr; };
struct UppercaseNode : ASTNode { shared_ptr<ASTNode> text; };
struct LowercaseNode : ASTNode { shared_ptr<ASTNode> text; };
struct TrimNode : ASTNode { shared_ptr<ASTNode> text; };
struct SubstringNode : ASTNode { shared_ptr<ASTNode> text, start, length; };
struct ContainsNode : ASTNode { shared_ptr<ASTNode> text, search; };
struct StartsWithNode : ASTNode { shared_ptr<ASTNode> text, prefix; };
struct EndsWithNode : ASTNode { shared_ptr<ASTNode> text, suffix; };
struct MathOpNode : ASTNode { string op; shared_ptr<ASTNode> arg1, arg2; };
struct MaxNode : ASTNode { string arrayName; };
struct MinNode : ASTNode { string arrayName; };
struct SumNode : ASTNode { string arrayName; };
struct AverageNode : ASTNode { string arrayName; };
struct KeysNode : ASTNode { string mapName; };
struct ValuesNode : ASTNode { string mapName; };
struct SortNode : ASTNode { string arrayName; };
struct ReverseNode : ASTNode { string arrayName; };
struct UniqueNode : ASTNode { string arrayName; };
struct RangeNode : ASTNode { shared_ptr<ASTNode> start, end; };
struct RepeatNode : ASTNode { shared_ptr<ASTNode> count; vector<shared_ptr<ASTNode>> body; };
struct BreakNode : ASTNode {};
struct ContinueNode : ASTNode {};
struct ExitNode : ASTNode {};
struct DateNode : ASTNode { string part; };
struct RandomNode : ASTNode { shared_ptr<ASTNode> min, max; };
struct CurrentTimeNode : ASTNode {};
struct WaitNode : ASTNode { shared_ptr<ASTNode> seconds; };
struct MapNode : ASTNode { map<string, shared_ptr<ASTNode>> pairs; };
struct GetMapValueNode : ASTNode { shared_ptr<ASTNode> key; string mapName; };
struct GetFieldNode : ASTNode { shared_ptr<ASTNode> object; string field; };
struct ImportNode : ASTNode { shared_ptr<ASTNode> filepath; };
struct TryCatchNode : ASTNode { vector<shared_ptr<ASTNode>> tryBlock, catchBlock; string errorVar; };
struct AwaitNode : ASTNode { shared_ptr<ASTNode> task; };
struct HttpGetNode : ASTNode { shared_ptr<ASTNode> url; };
struct ParseNode : ASTNode { shared_ptr<ASTNode> json; };
struct StringifyNode : ASTNode { shared_ptr<ASTNode> mapExpr; };
struct SpawnNode : ASTNode { vector<shared_ptr<ASTNode>> body; string taskName; };
struct StopTaskNode : ASTNode { string taskName; };
struct SendToChannelNode : ASTNode { shared_ptr<ASTNode> message; string channelName; };
struct ReceiveFromChannelNode : ASTNode { string channelName; shared_ptr<ASTNode> timeout; };
struct LoadNativeNode : ASTNode { shared_ptr<ASTNode> lib; string alias; };
struct TraceNode : ASTNode { };
struct CreateChannelNode : ASTNode { string channelName; };
struct RunCommandNode : ASTNode { shared_ptr<ASTNode> command; bool captureOutput = false; };
struct GetEnvVarNode : ASTNode { shared_ptr<ASTNode> varName; };
struct LambdaNode : ASTNode { vector<string> params; vector<shared_ptr<ASTNode>> body; };

enum TT {
    T_EOF, T_NUM, T_STR, T_ID, T_SET, T_TO, T_DISPLAY, T_IF, T_ELSE, T_END,
    T_DO, T_WHILE, T_DEFINE, T_FUNC, T_WITH, T_AND, T_RETURN, T_CALL,
    T_ITERATE, T_USING, T_THROUGH, T_ADD, T_REMOVE, T_FROM, T_GET, T_INDEX,
    T_OF, T_IN, T_VALUE, T_AT, T_LENGTH, T_IS, T_EQUAL, T_NOT, T_GREATER,
    T_THAN, T_LESS, T_OR, T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT,
    T_LPAREN, T_RPAREN, T_LBRACKET, T_RBRACKET, T_COMMA, T_COLON,
    T_TRUE, T_FALSE, T_ITERATION, T_MOST, T_LEAST, T_USER, T_INPUT,
    T_PROMPT, T_READ, T_FILE, T_WRITE, T_INTO, T_APPEND, T_EXISTS,
    T_DELETE, T_SPLIT, T_BY, T_JOIN, T_REPLACE, T_UPPERCASE, T_LOWERCASE,
    T_TRIM, T_SUBSTRING, T_FROM_KW, T_FLOOR, T_CEILING, T_ROUND,
    T_ABSOLUTE, T_POWER, T_SQUARE, T_ROOT, T_SINE, T_COSINE, T_TANGENT,
    T_KEYS, T_VALUES, T_SORT, T_REVERSE, T_UNIQUE, T_MAXIMUM, T_MINIMUM,
    T_SUM, T_AVERAGE, T_RANGE, T_REPEAT, T_TIMES, T_BREAK, T_CONTINUE,
    T_EXIT, T_YEAR, T_MONTH, T_DAY, T_HOUR, T_MINUTE, T_STARTS, T_ENDS,
    T_CONTAINS, T_CURRENT, T_TIME, T_RANDOM, T_BETWEEN, T_WAIT, T_SECOND,
    T_MAP, T_MAPPED, T_AS, T_LINES, T_TRY, T_ERROR, T_HTTP, 
    T_THERE, T_AN, T_CODE, T_THE, T_BACKGROUND, T_TASK, T_FOR, T_FINISH, T_STOP,
    T_TYPE_INT, T_TYPE_NUM, T_TYPE_TEXT, T_TYPE_BOOL, T_EACH, T_PARSE, T_STRINGIFY,
    T_PLEASE, T_SEND, T_REQUEST, T_POST, T_LBRACE, T_RBRACE,
    T_ENVIRONMENT, T_VARIABLE, T_DOT, T_RUN, T_CAPTURE, T_OUTPUT, T_OBJECT,
    T_CHANNEL, T_CREATE, T_RECEIVE, T_LOAD, T_NATIVE, T_TRACE,
    // natural language tokens
    T_SAME, T_EXACTLY, T_BECOMES, T_INCREASE, T_INCREMENT, T_ASK, T_THEN, T_MORE, T_FIRST, T_THIRD, T_ITEM, T_LAST
};

struct Token { TT type; string value; int line; };

class Lexer {
    string s;
    size_t p = 0;
    int line = 1;
    map<string, TT> kw = {
        {"set", T_SET}, {"to", T_TO}, {"display", T_DISPLAY}, {"if", T_IF},
        {"else", T_ELSE}, {"end", T_END}, {"do", T_DO}, {"while", T_WHILE},
        {"define", T_DEFINE}, {"function", T_FUNC}, {"with", T_WITH},
        {"and", T_AND}, {"return", T_RETURN}, {"call", T_CALL},
        {"iterate", T_ITERATE}, {"using", T_USING}, {"through", T_THROUGH},
        {"add", T_ADD}, {"remove", T_REMOVE}, {"from", T_FROM}, {"get", T_GET},
        {"index", T_INDEX}, {"of", T_OF}, {"in", T_IN}, {"value", T_VALUE},
        {"at", T_AT}, {"length", T_LENGTH}, {"is", T_IS}, {"equal", T_EQUAL},
        {"not", T_NOT}, {"greater", T_GREATER}, {"than", T_THAN}, {"less", T_LESS},
        {"or", T_OR}, {"true", T_TRUE}, {"false", T_FALSE},
        {"iteration", T_ITERATION}, {"most", T_MOST}, {"least", T_LEAST},
        {"user", T_USER}, {"input", T_INPUT}, {"prompt", T_PROMPT},
        {"read", T_READ}, {"file", T_FILE}, {"write", T_WRITE}, {"save", T_WRITE}, {"into", T_INTO},
        {"append", T_APPEND}, {"exists", T_EXISTS}, {"delete", T_DELETE},
        {"split", T_SPLIT}, {"by", T_BY}, {"join", T_JOIN}, {"replace", T_REPLACE},
        {"uppercase", T_UPPERCASE}, {"lowercase", T_LOWERCASE}, {"trim", T_TRIM},
        {"substring", T_SUBSTRING}, {"floor", T_FLOOR}, {"ceiling", T_CEILING},
        {"round", T_ROUND}, {"absolute", T_ABSOLUTE}, {"power", T_POWER},
        {"square", T_SQUARE}, {"root", T_ROOT}, {"sine", T_SINE}, {"cosine", T_COSINE},
        {"tangent", T_TANGENT}, {"keys", T_KEYS}, {"values", T_VALUES},
        {"sort", T_SORT}, {"reverse", T_REVERSE}, {"unique", T_UNIQUE},
        {"maximum", T_MAXIMUM}, {"minimum", T_MINIMUM}, {"sum", T_SUM},
        {"average", T_AVERAGE}, {"range", T_RANGE}, {"repeat", T_REPEAT},
        {"times", T_TIMES}, {"break", T_BREAK}, {"continue", T_CONTINUE},
        {"exit", T_EXIT}, {"year", T_YEAR}, {"month", T_MONTH}, {"day", T_DAY},
        {"hour", T_HOUR}, {"minute", T_MINUTE}, {"starts", T_STARTS},
        {"ends", T_ENDS}, {"contains", T_CONTAINS}, {"current", T_CURRENT},
        {"time", T_TIME}, {"random", T_RANDOM}, {"between", T_BETWEEN},
        {"wait", T_WAIT}, {"second", T_SECOND}, {"map", T_MAP}, {"mapped", T_MAPPED},
        {"as", T_AS}, {"lines", T_LINES},
        {"try", T_TRY}, {"error", T_ERROR}, {"http", T_HTTP},
        {"there", T_THERE}, {"an", T_AN}, {"code", T_CODE}, {"the", T_THE},
        {"background", T_BACKGROUND}, {"task", T_TASK}, {"for", T_FOR}, 
        {"finish", T_FINISH}, {"stop", T_STOP},
        {"integer", T_TYPE_INT}, {"number", T_TYPE_NUM}, 
        {"text", T_TYPE_TEXT}, {"boolean", T_TYPE_BOOL},
        {"each", T_EACH}, {"parse", T_PARSE}, {"stringify", T_STRINGIFY},
        {"please", T_PLEASE}, {"send", T_SEND}, {"request", T_REQUEST},
        {"post", T_POST}, {"environment", T_ENVIRONMENT}, {"variable", T_VARIABLE},
        {"run", T_RUN}, {"capture", T_CAPTURE}, {"output", T_OUTPUT},
        {"object", T_OBJECT}, {"channel", T_CHANNEL}, {"create", T_CREATE},
        {"receive", T_RECEIVE}, {"load", T_LOAD}, {"native", T_NATIVE}, {"trace", T_TRACE},
        // natural language synonyms
        {"same", T_SAME}, {"exactly", T_EXACTLY}, {"becomes", T_BECOMES}, {"become", T_BECOMES}, {"increase", T_INCREASE}, {"increment", T_INCREMENT},
        {"ask", T_ASK}, {"then", T_THEN}, {"more", T_MORE}, {"first", T_FIRST}, {"second", T_SECOND}, {"third", T_THIRD}, {"item", T_ITEM}, {"last", T_LAST}
    };
    
    char peek() { return p < s.size() ? s[p] : '\0'; }
    char adv() { 
        if(p < s.size()) {
            char c = s[p++];
            if(c == '\n') ++line;
            return c;
        }
        return '\0';
    }
    void skip() { while(isspace(peek())) adv(); }
    
    bool isNoiseWord(const string& word) {
        static unordered_set<string> noise = {"the", "a", "an", "please", "can", "you", "new", "variable", "named", "value"};
        return noise.count(word);
    }
    
public:
    Lexer(const string& src) : s(src) {}
    
    vector<Token> tokenize() {
        vector<Token> ts;
        while(true) {
            skip();
            if(!peek()) { ts.push_back({T_EOF, "", line}); break; }

            string singleLine = "ignore this line:";
            if(p + singleLine.length() <= s.length() && 
               s.substr(p, singleLine.length()) == singleLine) {
                p += singleLine.length();
                while(peek() != '\n' && peek() != '\0') adv();
                continue;
            }

            string multiLineStart = "ignore all this:";
            if(p + multiLineStart.length() <= s.length() && 
               s.substr(p, multiLineStart.length()) == multiLineStart) {
                p += multiLineStart.length();
                string ender = "end ignore";
                bool foundEnd = false;
                while(peek() != '\0') {
                    if(p + ender.length() <= s.length() && 
                       s.substr(p, ender.length()) == ender) {
                        p += ender.length();
                        foundEnd = true;
                        break;
                    }
                    adv();
                }
                if(!foundEnd) throw runtime_error("Unclosed multi-line comment");
                continue;
            }

            if(peek() == '#') { while(peek() != '\n' && peek()) adv(); continue; }

            char c = peek();
            if(isdigit(c)) {
                string n;
                while(isdigit(peek()) || peek() == '.') n += adv();
                ts.push_back({T_NUM, n, line});
            } else if(c == '"') {
                adv();
                string st;
                while(peek() != '"' && peek()) st += adv();
                if(peek() == '"') adv();
                ts.push_back({T_STR, st, line});
            } else if(isalpha(c)) {
                string id;
                while(isalnum(peek()) || peek() == '_') id += adv();
                if(isNoiseWord(id)) continue;
                // support possessive "user's name" => user . name
                if(peek() == '\'' && p + 1 < s.size() && s[p+1] == 's') {
                    ts.push_back({kw.count(id) ? kw[id] : T_ID, id, line});
                    // consume '\'s' sequence
                    adv(); adv();
                    // emit dot token to represent possessive
                    ts.push_back({T_DOT, ".", line});
                } else {
                    ts.push_back({kw.count(id) ? kw[id] : T_ID, id, line});
                }
            } else if(c == '+') { adv(); ts.push_back({T_PLUS, "+", line}); }
            else if(c == '-') { adv(); ts.push_back({T_MINUS, "-", line}); }
            else if(c == '*') { adv(); ts.push_back({T_STAR, "*", line}); }
            else if(c == '/') { adv(); ts.push_back({T_SLASH, "/", line}); }
            else if(c == '%') { adv(); ts.push_back({T_PERCENT, "%", line}); }
            else if(c == '(') { adv(); ts.push_back({T_LPAREN, "(", line}); }
            else if(c == ')') { adv(); ts.push_back({T_RPAREN, ")", line}); }
            else if(c == '[') { adv(); ts.push_back({T_LBRACKET, "[", line}); }
            else if(c == ']') { adv(); ts.push_back({T_RBRACKET, "]", line}); }
            else if(c == '{') { adv(); ts.push_back({T_LBRACE, "{", line}); }
            else if(c == '}') { adv(); ts.push_back({T_RBRACE, "}", line}); }
            else if(c == ',') { adv(); ts.push_back({T_COMMA, ",", line}); }
            else if(c == ':') { adv(); ts.push_back({T_COLON, ":", line}); }
            else if(c == '.') { adv(); ts.push_back({T_DOT, ".", line}); }
            else adv();
        }
        return ts;
    }
};

class Parser {
    vector<Token> ts;
    size_t p = 0;
    
    Token peek() { return p < ts.size() ? ts[p] : ts.back(); }
    Token peekNext() { return (p + 1 < ts.size()) ? ts[p + 1] : ts.back(); }
    Token adv() { return p < ts.size() ? ts[p++] : ts.back(); }
    bool match(TT t) { if(peek().type == t) { adv(); return true; } return false; }
    void expect(TT t, string msg) { if(!match(t)) throw runtime_error(msg); }
    
    shared_ptr<ASTNode> expr() { return orExpr(); }
    
    shared_ptr<ASTNode> orExpr() {
        auto l = andExpr();
        while(match(T_OR)) {
            auto r = andExpr();
            auto n = make_shared<BinaryOpNode>();
            n->left = l; n->right = r; n->op = "or";
            l = n;
        }
        return l;
    }
    
    shared_ptr<ASTNode> andExpr() {
        auto l = cmpExpr();
        while(match(T_AND)) {
            auto r = cmpExpr();
            auto n = make_shared<BinaryOpNode>();
            n->left = l; n->right = r; n->op = "and";
            l = n;
        }
        return l;
    }
    
    shared_ptr<ASTNode> cmpExpr() {
        auto l = addExpr();
        if(match(T_IS)) {
            string op;
            // natural phrases: 'is same as' (noise word 'the' already filtered), 'is exactly', etc.
            if(match(T_SAME)) { if(peek().type == T_AS) adv(); op = "=="; }
            else if(match(T_EXACTLY)) { op = "=="; }
            else if(match(T_NOT) && match(T_SAME)) { if(peek().type == T_AS) adv(); op = "!="; }
            else if(match(T_NOT)) { expect(T_EQUAL, "Expected 'equal'"); expect(T_TO, "Expected 'to'"); op = "!="; }
            else if(match(T_GREATER) || match(T_MORE)) { if(peek().type == T_THAN) adv(); op = ">"; }
            else if(match(T_LESS)) { if(peek().type == T_THAN) adv(); op = "<"; }
            else if(match(T_AT)) {
                if(match(T_LEAST)) op = ">=";
                else if(match(T_MOST)) op = "<=";
            } else if(match(T_EQUAL)) { if(match(T_TO)) {} op = "=="; }
            // default: 'is x' -> equality
            if(op.empty()) op = "==";
            auto r = addExpr();
            auto n = make_shared<BinaryOpNode>();
            n->left = l; n->right = r; n->op = op;
            return n;
        }
        return l;
    }
    
    shared_ptr<ASTNode> addExpr() {
        auto l = mulExpr();
        while(peek().type == T_PLUS || peek().type == T_MINUS) {
            string op = adv().value;
            auto r = mulExpr();
            auto n = make_shared<BinaryOpNode>();
            n->left = l; n->right = r; n->op = op;
            l = n;
        }
        return l;
    }
    
    shared_ptr<ASTNode> mulExpr() {
        auto l = postfixExpr();
        while(peek().type == T_STAR || peek().type == T_SLASH || peek().type == T_PERCENT) {
            string op = adv().value;
            auto r = postfixExpr();
            auto n = make_shared<BinaryOpNode>();
            n->left = l; n->right = r; n->op = op;
            l = n;
        }
        return l;
    }
    
    // handle postfix operations like dot notation
    shared_ptr<ASTNode> postfixExpr() {
        auto l = unaryExpr();
        
        while(peek().type == T_DOT) {
            adv();
            if(peek().type != T_ID) throw runtime_error("Expected field name after '.'");
            string field = adv().value;
            
            auto n = make_shared<GetFieldNode>();
            n->object = l;
            n->field = field;
            l = n;
        }

        // postfix 'exists' -> fileexistsnode where the left expression is the filepath
        if(match(T_EXISTS)) {
            auto n = make_shared<FileExistsNode>();
            n->filepath = l;
            return n;
        }
        
        return l;
    }
    
    shared_ptr<ASTNode> unaryExpr() {
        if(match(T_NOT)) {
            auto n = make_shared<UnaryOpNode>();
            n->operand = unaryExpr();
            n->op = "not";
            return n;
        }
        if(match(T_MINUS)) {
            auto n = make_shared<UnaryOpNode>();
            n->operand = unaryExpr();
            n->op = "-";
            return n;
        }
        return primary();
    }
    
    shared_ptr<ASTNode> primary() {
        if(peek().type == T_NUM) {
            auto n = make_shared<NumberNode>();
            n->value = stod(adv().value);
            return n;
        }
        if(peek().type == T_STR) {
            // natural concatenation: adjacent strings or 'and' separated strings join together
            string combined = adv().value;
            while(true) {
                if(peek().type == T_STR) {
                    combined += adv().value; // adjacent
                } else if(peek().type == T_AND && peekNext().type == T_STR) {
                    adv(); // consume 'and'
                    combined += " ";
                    combined += adv().value;
                } else break;
            }
            auto n = make_shared<TextNode>();
            n->value = combined;
            return n;
        }
        if(match(T_TRUE)) { auto n = make_shared<BoolNode>(); n->value = true; return n; }
        if(match(T_FALSE)) { auto n = make_shared<BoolNode>(); n->value = false; return n; }

        // natural: 'length of tasks' -> lengthofnode
        if(match(T_LENGTH)) {
            expect(T_OF, "Expected 'of'");
            auto n = make_shared<LengthOfNode>();
            n->arrayName = adv().value;
            return n;
        }
        
        // lambda (anonymous function) - short and long forms
        if(match(T_FUNC)) {
            auto n = make_shared<LambdaNode>();
            if(match(T_WITH)) {
                n->params.push_back(adv().value);
                while(match(T_AND)) n->params.push_back(adv().value);
                match(T_DO); // ':' optional and 'do' optional in natural mode
                match(T_COLON);
                while(peek().type != T_END && peek().type != T_EOF) n->body.push_back(stmt());
                expect(T_END, "Expected 'end'");
                if(peek().type == T_FUNC) adv();
            } else {
                // short form: function x return expr
                n->params.push_back(adv().value);
                expect(T_RETURN, "Expected 'return'");
                auto r = make_shared<ReturnNode>();
                r->value = expr();
                n->body.push_back(r);
            }
            return n;
        }

        // natural indexing: 'first item in colors' or 'item 1 of colors'
        if(match(T_FIRST) || match(T_SECOND) || match(T_THIRD) || match(T_LAST)) {
            int idx = 0; if(ts[p-1].type == T_SECOND) idx = 1; if(ts[p-1].type == T_THIRD) idx = 2;
            if(match(T_LAST)) {
                // produce a getlastnode
                if(match(T_ITEM)) {}
                expect(T_IN, "Expected 'in'");
                string arr = adv().value;
                auto g = make_shared<GetLastNode>(); g->arrayName = arr; return g;
            }
            if(match(T_ITEM)) {} // optional 'item'
            expect(T_IN, "Expected 'in'");
            string arr = adv().value;
            auto get = make_shared<GetValueAtNode>();
            auto num = make_shared<NumberNode>(); num->value = (double)idx;
            get->index = num; get->arrayName = arr;
            return get;
        }
        if(match(T_ITEM)) {
            int idx = 0;
            if(peek().type == T_NUM) { idx = stoi(adv().value) - 1; }
            expect(T_OF, "Expected 'of'");
            string arr = adv().value;
            auto get = make_shared<GetValueAtNode>();
            auto num = make_shared<NumberNode>(); num->value = (double)idx;
            get->index = num; get->arrayName = arr;
            return get;
        }

        if(match(T_ASK)) {
            auto n = make_shared<UserInputNode>();
            if(peek().type == T_STR) { n->prompt = make_shared<TextNode>(); dynamic_pointer_cast<TextNode>(n->prompt)->value = adv().value; }
            else { n->prompt = make_shared<TextNode>(); dynamic_pointer_cast<TextNode>(n->prompt)->value = ""; }
            if(match(T_AS)) {
                if(match(T_TYPE_INT)) n->targetType = "integer";
                else if(match(T_TYPE_NUM)) n->targetType = "number";
                else if(match(T_TYPE_TEXT)) n->targetType = "text";
                else if(match(T_TYPE_BOOL)) n->targetType = "boolean";
            }
            return n;
        }

        if(peek().type == T_USER) {
            if(peekNext().type == T_INPUT) {
                adv(); // consume 'user'
                adv(); // consume 'input'
                auto n = make_shared<UserInputNode>();
                if(match(T_WITH)) {
                    expect(T_PROMPT, "Expected 'prompt'");
                    n->prompt = expr();
                } else {
                    n->prompt = make_shared<TextNode>();
                    dynamic_pointer_cast<TextNode>(n->prompt)->value = "";
                }
                if(match(T_AS)) {
                    if(match(T_TYPE_INT)) n->targetType = "integer";
                    else if(match(T_TYPE_NUM)) n->targetType = "number";
                    else if(match(T_TYPE_TEXT)) n->targetType = "text";
                    else if(match(T_TYPE_BOOL)) n->targetType = "boolean";
                }
                return n;
            } else {
                auto idn = make_shared<IdentifierNode>();
                idn->name = adv().value; // consume 'user'
                return idn;
            }
        }
        
        if(match(T_READ)) {
            expect(T_FILE, "Expected 'file'");
            auto filepath = expr();
            auto n = make_shared<ReadFileNode>();
            n->filepath = filepath;
            if(match(T_AS)) {
                expect(T_LINES, "Expected 'lines'");
                n->asLines = true;
            }
            return n;
        }
        
        if(match(T_RUN)) {
            auto cmd = expr();
            auto n = make_shared<RunCommandNode>();
            n->command = cmd;
            if(match(T_AND)) {
                expect(T_CAPTURE, "Expected 'capture'");
                expect(T_OUTPUT, "Expected 'output'");
                n->captureOutput = true;
            }
            return n;
        }

        if(match(T_RECEIVE)) {
            expect(T_FROM, "Expected 'from'");
            expect(T_CHANNEL, "Expected 'channel'");
            auto n = make_shared<ReceiveFromChannelNode>();
            n->channelName = adv().value;
            if(match(T_FOR)) {
                n->timeout = expr();
                expect(T_SECOND, "Expected 'second'");
            }
            return n;
        }
        
        if(match(T_FILE)) {
            auto filepath = expr();
            expect(T_EXISTS, "Expected 'exists'");
            auto n = make_shared<FileExistsNode>();
            n->filepath = filepath;
            return n;
        }
        
        if(match(T_SPLIT)) {
            auto text = expr();
            expect(T_BY, "Expected 'by'");
            auto delim = expr();
            auto n = make_shared<SplitNode>();
            n->text = text; n->delimiter = delim;
            return n;
        }
        
        if(match(T_JOIN)) {
            string arrName = adv().value;
            expect(T_BY, "Expected 'by'");
            auto delim = expr();
            auto n = make_shared<JoinNode>();
            n->arrayName = arrName; n->delimiter = delim;
            return n;
        }
        
        if(match(T_REPLACE)) {
            auto text = expr();
            expect(T_WITH, "Expected 'with'");
            auto oldStr = expr();
            expect(T_TO, "Expected 'to'");
            auto newStr = expr();
            auto n = make_shared<ReplaceNode>();
            n->text = text; n->oldStr = oldStr; n->newStr = newStr;
            return n;
        }
        
        if(match(T_UPPERCASE)) { auto n = make_shared<UppercaseNode>(); n->text = expr(); return n; }
        if(match(T_LOWERCASE)) { auto n = make_shared<LowercaseNode>(); n->text = expr(); return n; }
        if(match(T_TRIM)) { auto n = make_shared<TrimNode>(); n->text = expr(); return n; }
        
        if(match(T_CONTAINS)) {
            auto text = expr();
            auto search = expr();
            auto n = make_shared<ContainsNode>();
            n->text = text; n->search = search;
            return n;
        }
        
        if(match(T_STARTS)) {
            expect(T_WITH, "Expected 'with'");
            auto text = expr();
            auto prefix = expr();
            auto n = make_shared<StartsWithNode>();
            n->text = text; n->prefix = prefix;
            return n;
        }
        
        if(match(T_ENDS)) {
            expect(T_WITH, "Expected 'with'");
            auto text = expr();
            auto suffix = expr();
            auto n = make_shared<EndsWithNode>();
            n->text = text; n->suffix = suffix;
            return n;
        }
        
        if(match(T_SUBSTRING)) {
            expect(T_OF, "Expected 'of'");
            auto text = expr();
            expect(T_FROM, "Expected 'from'");
            auto start = expr();
            expect(T_WITH, "Expected 'with'");
            expect(T_LENGTH, "Expected 'length'");
            auto len = expr();
            auto n = make_shared<SubstringNode>();
            n->text = text; n->start = start; n->length = len;
            return n;
        }
        
        if(match(T_FLOOR)) { expect(T_OF, "Expected 'of'"); auto n = make_shared<MathOpNode>(); n->op = "floor"; n->arg1 = expr(); return n; }
        if(match(T_CEILING)) { expect(T_OF, "Expected 'of'"); auto n = make_shared<MathOpNode>(); n->op = "ceiling"; n->arg1 = expr(); return n; }
        if(match(T_ROUND)) { expect(T_OF, "Expected 'of'"); auto n = make_shared<MathOpNode>(); n->op = "round"; n->arg1 = expr(); return n; }
        if(match(T_ABSOLUTE)) { expect(T_OF, "Expected 'of'"); auto n = make_shared<MathOpNode>(); n->op = "abs"; n->arg1 = expr(); return n; }
        if(match(T_SINE)) { expect(T_OF, "Expected 'of'"); auto n = make_shared<MathOpNode>(); n->op = "sin"; n->arg1 = expr(); return n; }
        if(match(T_COSINE)) { expect(T_OF, "Expected 'of'"); auto n = make_shared<MathOpNode>(); n->op = "cos"; n->arg1 = expr(); return n; }
        if(match(T_TANGENT)) { expect(T_OF, "Expected 'of'"); auto n = make_shared<MathOpNode>(); n->op = "tan"; n->arg1 = expr(); return n; }
        
        if(match(T_POWER)) {
            expect(T_OF, "Expected 'of'");
            auto base = expr();
            expect(T_TO, "Expected 'to'");
            auto exp = expr();
            auto n = make_shared<MathOpNode>();
            n->op = "pow"; n->arg1 = base; n->arg2 = exp;
            return n;
        }
        
        if(match(T_SQUARE)) {
            expect(T_ROOT, "Expected 'root'");
            expect(T_OF, "Expected 'of'");
            auto n = make_shared<MathOpNode>();
            n->op = "sqrt"; n->arg1 = expr();
            return n;
        }
        
        if(match(T_MAXIMUM)) { expect(T_OF, "Expected 'of'"); auto n = make_shared<MaxNode>(); n->arrayName = adv().value; return n; }
        if(match(T_MINIMUM)) { expect(T_OF, "Expected 'of'"); auto n = make_shared<MinNode>(); n->arrayName = adv().value; return n; }
        if(match(T_SUM)) { expect(T_OF, "Expected 'of'"); auto n = make_shared<SumNode>(); n->arrayName = adv().value; return n; }
        if(match(T_AVERAGE)) { expect(T_OF, "Expected 'of'"); auto n = make_shared<AverageNode>(); n->arrayName = adv().value; return n; }
        if(match(T_KEYS)) { expect(T_OF, "Expected 'of'"); auto n = make_shared<KeysNode>(); n->mapName = adv().value; return n; }
        if(match(T_VALUES)) { expect(T_OF, "Expected 'of'"); auto n = make_shared<ValuesNode>(); n->mapName = adv().value; return n; }
        
        if(match(T_RANGE)) {
            expect(T_FROM, "Expected 'from'");
            auto start = expr();
            expect(T_TO, "Expected 'to'");
            auto end = expr();
            auto n = make_shared<RangeNode>();
            n->start = start; n->end = end;
            return n;
        }
        
        if(match(T_RANDOM)) {
            expect(T_FROM, "Expected 'from'");
            auto min = expr();
            expect(T_TO, "Expected 'to'");
            auto max = expr();
            auto n = make_shared<RandomNode>();
            n->min = min; n->max = max;
            return n;
        }
        
        if(match(T_CURRENT)) {
            if(match(T_TIME)) return make_shared<CurrentTimeNode>();
            if(match(T_YEAR)) { auto n = make_shared<DateNode>(); n->part = "year"; return n; }
            if(match(T_MONTH)) { auto n = make_shared<DateNode>(); n->part = "month"; return n; }
            if(match(T_DAY)) { auto n = make_shared<DateNode>(); n->part = "day"; return n; }
            if(match(T_HOUR)) { auto n = make_shared<DateNode>(); n->part = "hour"; return n; }
            if(match(T_MINUTE)) { auto n = make_shared<DateNode>(); n->part = "minute"; return n; }
        }
        
        if(match(T_LPAREN)) {
            auto e = expr();
            expect(T_RPAREN, "Expected ')'");
            return e;
        }
        
        if(match(T_LBRACKET)) {
            auto n = make_shared<ArrayNode>();
            if(!match(T_RBRACKET)) {
                n->elements.push_back(expr());
                while(match(T_COMMA)) n->elements.push_back(expr());
                expect(T_RBRACKET, "Expected ']'");
            }
            return n;
        }
        
        if(match(T_OBJECT)) {
            expect(T_WITH, "Expected 'with'");
            match(T_COLON); // optional ':' for natural english
            auto n = make_shared<MapNode>();
            while(peek().type == T_ID) {
                string key = adv().value;
                match(T_COLON); // optional ':' between key and value, but preferred
                n->pairs[key] = expr();
                if(peek().type == T_COMMA) {
                    adv();
                } else if(peek().type == T_ID) {
                    throw runtime_error("Expected ',' between object fields (missing comma after field '" + key + "')");
                } else {
                    break;
                }
            }
            expect(T_END, "Expected 'end'");
            if(peek().type == T_OBJECT) adv();
            return n;
        }
        
        if(match(T_MAP)) {
            expect(T_WITH, "Expected 'with'");
            match(T_COLON); // optional
            auto n = make_shared<MapNode>();
            while(peek().type == T_STR) {
                string key = adv().value;
                expect(T_MAPPED, "Expected 'mapped'");
                expect(T_TO, "Expected 'to'");
                n->pairs[key] = expr();
            }
            expect(T_END, "Expected 'end'");
            if(peek().type == T_MAP) adv();
            return n;
        }
        
        if(match(T_GET)) {
            if(match(T_CODE)) {
                expect(T_FROM, "Expected 'from'");
                auto n = make_shared<ImportNode>();
                n->filepath = expr();
                return n;
            }
            if(match(T_ENVIRONMENT)) {
                expect(T_VARIABLE, "Expected 'variable'");
                auto n = make_shared<GetEnvVarNode>();
                n->varName = expr();
                return n;
            }
        }
        
        if(match(T_HTTP)) {
            if(match(T_GET)) {
                auto n = make_shared<HttpGetNode>();
                n->url = expr();
                return n;
            }
        }
        
        if(match(T_PARSE)) {
            auto n = make_shared<ParseNode>();
            n->json = expr();
            return n;
        }
        
        if(match(T_STRINGIFY)) {
            auto n = make_shared<StringifyNode>();
            n->mapExpr = expr();
            return n;
        }
        
        if(match(T_CALL)) {
            auto n = make_shared<CallNode>();
            n->name = adv().value;
            if(match(T_WITH)) {
                n->args.push_back(expr());
                while(match(T_AND)) n->args.push_back(expr());
            }
            return n;
        }
        
        if(peek().type == T_ID) {
            auto n = make_shared<IdentifierNode>();
            n->name = adv().value;
            return n;
        }
        
        throw runtime_error("Unexpected token: '" + peek().value + "'");
    }
    
    shared_ptr<ASTNode> stmt() {
        // allow implied assignment: 'x is 10' or 'x becomes 10'
        if(peek().type == T_ID && peekNext().type == T_IS) {
            auto n = make_shared<SetNode>();
            n->name = adv().value; // id
            adv(); // consume 'is'
            n->value = expr();
            return n;
        }
        if(peek().type == T_ID && peekNext().type == T_BECOMES) {
            auto n = make_shared<SetNode>();
            n->name = adv().value;
            adv(); // consume 'becomes'
            n->value = expr();
            return n;
        }

        // shorthand increment: 'increase x by 5' or 'increment x'
        if(match(T_INCREASE)) {
            string name = adv().value;
            shared_ptr<ASTNode> byVal;
            if(match(T_BY)) byVal = expr();
            else { auto nv = make_shared<NumberNode>(); nv->value = 1; byVal = nv; }
            // transform into set name to name + byval
            auto n = make_shared<SetNode>(); n->name = name;
            auto left = make_shared<IdentifierNode>(); left->name = name;
            auto bin = make_shared<BinaryOpNode>(); bin->left = left; bin->right = byVal; bin->op = "+";
            n->value = bin; return n;
        }
        if(match(T_INCREMENT)) {
            string name = adv().value;
            auto n = make_shared<SetNode>(); n->name = name;
            auto left = make_shared<IdentifierNode>(); left->name = name;
            auto byVal = make_shared<NumberNode>(); byVal->value = 1;
            auto bin = make_shared<BinaryOpNode>(); bin->left = left; bin->right = byVal; bin->op = "+";
            n->value = bin; return n;
        }

        if(match(T_SET)) {
            auto n = make_shared<SetNode>();
            n->name = adv().value;
            
            // handle dot notation: set x.field to value
            if(peek().type == T_DOT) {
                auto objNode = make_shared<IdentifierNode>();
                objNode->name = n->name;
                shared_ptr<ASTNode> fieldNode = objNode;                
                while(match(T_DOT)) {
                    string field = adv().value;
                    auto getField = make_shared<GetFieldNode>();
                    getField->object = fieldNode;
                    getField->field = field;
                    fieldNode = getField;
                }
                
                expect(T_TO, "Expected 'to'");
                auto setField = make_shared<SetFieldNode>();
                setField->object = objNode;
                if(auto gf = dynamic_pointer_cast<GetFieldNode>(fieldNode)) {
                    setField->field = gf->field;
                    setField->object = gf->object;  
                }
                setField->value = expr();
                return setField;
            }
            
            expect(T_TO, "Expected 'to'");
            n->value = expr();
            return n;
        }
        
        if(match(T_DISPLAY)) {
            auto n = make_shared<DisplayNode>();
            n->value = expr();
            return n;
        }

        if(match(T_TRY)) {
            auto n = make_shared<TryCatchNode>();
            match(T_COLON); // optional
            while(peek().type != T_IF && peek().type != T_EOF) n->tryBlock.push_back(stmt());
            
            expect(T_IF, "Expected 'if'");
            expect(T_THERE, "Expected 'there'");
            expect(T_IS, "Expected 'is'");
            expect(T_AN, "Expected 'an'");
            expect(T_ERROR, "Expected 'error'");
            expect(T_SET, "Expected 'set'");
            n->errorVar = adv().value; 
            expect(T_TO, "Expected 'to'");
            expect(T_ERROR, "Expected 'error'");
            expect(T_AND, "Expected 'and'");
            match(T_DO); // optional
            match(T_COLON); // optional ':'
            
            while(peek().type != T_END && peek().type != T_EOF) n->catchBlock.push_back(stmt());
            expect(T_END, "Expected 'end'");
            expect(T_TRY, "Expected 'try'");
            return n;
        }

        if(match(T_LOAD)) {
            expect(T_NATIVE, "Expected 'native'");
            auto n = make_shared<LoadNativeNode>();
            n->lib = expr();
            expect(T_AS, "Expected 'as'");
            n->alias = adv().value;
            return n;
        }

        if(match(T_TRACE)) {
            return make_shared<TraceNode>();
        }

        if(match(T_CREATE)) {
            expect(T_CHANNEL, "Expected 'channel'");
            auto n = make_shared<CreateChannelNode>();
            if(peek().type == T_STR) {
                n->channelName = adv().value;
            } else {
                n->channelName = adv().value;
            }
            return n;
        }

        if(match(T_IN)) {
            expect(T_THE, "Expected 'the'");
            expect(T_BACKGROUND, "Expected 'background'");
            
            auto n = make_shared<SpawnNode>();
            if(match(T_AS)) {
                n->taskName = adv().value;
            }
            
            match(T_DO);
            match(T_COLON);
            while(peek().type != T_END && peek().type != T_EOF) n->body.push_back(stmt());
            expect(T_END, "Expected 'end'");
            if(peek().type == T_TASK) adv();
            return n;
        }

        if(match(T_STOP)) {
            expect(T_BACKGROUND, "Expected 'background'");
            expect(T_TASK, "Expected 'task'");
            auto n = make_shared<StopTaskNode>();
            n->taskName = adv().value;
            return n;
        }
        
        if(match(T_SEND)) {
            auto msg = expr();
            expect(T_TO, "Expected 'to'");
            
            if(match(T_CHANNEL)) {
                auto n = make_shared<SendToChannelNode>();
                n->message = msg;
                n->channelName = adv().value;
                return n;
            } else {
                throw runtime_error("Expected 'channel' after 'to'");
            }
        }

        if(match(T_IF)) {
            auto n = make_shared<IfNode>();
            n->condition = expr();
            if(match(T_THEN)) { // single-line 'if x is 10 then display "success"'
                n->thenBlock.push_back(stmt());
                return n;
            }
            match(T_DO); // 'do' is optional
            match(T_COLON); // ':' optional
            while(peek().type != T_ELSE && peek().type != T_END && peek().type != T_EOF)
                n->thenBlock.push_back(stmt());
            if(match(T_ELSE)) {
                match(T_COLON);
                while(peek().type != T_END && peek().type != T_EOF)
                    n->elseBlock.push_back(stmt());
            }
            expect(T_END, "Expected 'end'");
            if(peek().type == T_IF) adv();
            return n;
        }
        
        if(match(T_WHILE)) {
            auto n = make_shared<WhileNode>();
            n->condition = expr();
            if(match(T_THEN)) {
                n->body.push_back(stmt());
                return n;
            }
            match(T_DO);
            match(T_COLON);
            while(peek().type != T_END && peek().type != T_EOF)
                n->body.push_back(stmt());
            expect(T_END, "Expected 'end'");
            if(peek().type == T_WHILE) adv();
            return n;
        }
        
        if(match(T_ITERATE)) {
            expect(T_USING, "Expected 'using'");
            auto n = make_shared<IterateNode>();
            n->variable = adv().value;
            expect(T_THROUGH, "Expected 'through'");
            n->start = expr();
            expect(T_TO, "Expected 'to'");
            n->end = expr();
            expect(T_AND, "Expected 'and'");
            match(T_DO); // 'do' optional
            match(T_COLON);
            while(peek().type != T_END && peek().type != T_EOF)
                n->body.push_back(stmt());
            expect(T_END, "Expected 'end'");
            if(peek().type == T_ITERATION) adv();
            return n;
        }
        
        if(match(T_FOR)) {
            expect(T_EACH, "Expected 'each'");
            auto n = make_shared<ForEachNode>();
            n->variable = adv().value;
            expect(T_IN, "Expected 'in'");
            n->array = expr();
            match(T_DO);
            match(T_COLON);
            while(peek().type != T_END && peek().type != T_EOF)
                n->body.push_back(stmt());
            expect(T_END, "Expected 'end'");
            if(peek().type == T_FOR) adv();
            return n;
        }
        
        if(match(T_DEFINE)) {
            expect(T_FUNC, "Expected 'function'");
            auto n = make_shared<FunctionDefNode>();
            n->name = adv().value;
            if(match(T_WITH)) {
                n->params.push_back(adv().value);
                while(match(T_AND)) n->params.push_back(adv().value);
            }
            match(T_DO); // 'do' optional
            match(T_COLON); // ':' optional
            while(peek().type != T_END && peek().type != T_EOF)
                n->body.push_back(stmt());
            expect(T_END, "Expected 'end'");
            if(peek().type == T_FUNC) adv();
            return n;
        }
        
        if(match(T_RETURN)) {
            auto n = make_shared<ReturnNode>();
            n->value = expr();
            return n;
        }
        
        if(match(T_ADD)) {
            auto val = expr();
            expect(T_TO, "Expected 'to'");
            auto n = make_shared<AddToArrayNode>();
            n->value = val;
            n->arrayName = adv().value;
            return n;
        }
        
        if(match(T_REMOVE)) {
            auto val = expr();
            expect(T_FROM, "Expected 'from'");
            auto n = make_shared<RemoveFromArrayNode>();
            n->value = val;
            n->arrayName = adv().value;
            return n;
        }
        
        if(match(T_CALL)) {
            auto n = make_shared<CallNode>();
            n->name = adv().value;
            if(match(T_WITH)) {
                n->args.push_back(expr());
                while(match(T_AND)) n->args.push_back(expr());
            }
            return n;
        }
        
        if(match(T_WRITE)) {
            auto content = expr();
            // allow either 'into' or plain 'to' for natural 'save x to file y'
            if(!(match(T_INTO) || match(T_TO))) expect(T_INTO, "Expected 'into' or 'to'");
            expect(T_FILE, "Expected 'file'");
            auto filepath = expr();
            auto n = make_shared<WriteFileNode>();
            n->content = content; n->filepath = filepath;
            return n;
        }
        
        if(match(T_APPEND)) {
            auto content = expr();
            expect(T_INTO, "Expected 'into'");
            expect(T_FILE, "Expected 'file'");
            auto filepath = expr();
            auto n = make_shared<AppendFileNode>();
            n->content = content; n->filepath = filepath;
            return n;
        }
        
        if(match(T_DELETE)) {
            expect(T_FILE, "Expected 'file'");
            auto filepath = expr();
            auto n = make_shared<DeleteFileNode>();
            n->filepath = filepath;
            return n;
        }
        
        if(match(T_SORT)) {
            auto n = make_shared<SortNode>();
            n->arrayName = adv().value;
            return n;
        }
        
        if(match(T_REVERSE)) {
            auto n = make_shared<ReverseNode>();
            n->arrayName = adv().value;
            return n;
        }
        
        if(match(T_UNIQUE)) {
            auto n = make_shared<UniqueNode>();
            n->arrayName = adv().value;
            return n;
        }
        
        if(match(T_REPEAT)) {
            auto count = expr();
            expect(T_TIMES, "Expected 'times'");
            match(T_DO);
            match(T_COLON);
            auto n = make_shared<RepeatNode>();
            n->count = count;
            while(peek().type != T_END && peek().type != T_EOF)
                n->body.push_back(stmt());
            expect(T_END, "Expected 'end'");
            if(peek().type == T_REPEAT) adv();
            return n;
        }
        
        if(match(T_WAIT)) {
            if(match(T_FOR)) {
                auto n = make_shared<AwaitNode>();
                n->task = expr();
                expect(T_TO, "Expected 'to'");
                expect(T_FINISH, "Expected 'finish'");
                return n;
            }
            auto seconds = expr();
            expect(T_SECOND, "Expected 'second'");
            auto n = make_shared<WaitNode>();
            n->seconds = seconds;
            return n;
        }
        
        if(match(T_BREAK)) return make_shared<BreakNode>();
        if(match(T_CONTINUE)) return make_shared<ContinueNode>();
        if(match(T_EXIT)) return make_shared<ExitNode>();
        
        throw runtime_error("Unknown statement");
    }
    
public:
    Parser(const vector<Token>& tokens) : ts(tokens) {}
    
    vector<shared_ptr<ASTNode>> parse() {
        vector<shared_ptr<ASTNode>> stmts;
        while(peek().type != T_EOF) stmts.push_back(stmt());
        return stmts;
    }
};

// interpreter
class ReturnException : public exception {
public:
    shared_ptr<Value> value;
    ReturnException(shared_ptr<Value> v) : value(v) {}
};

class BreakException : public exception {};
class ContinueException : public exception {};
class TaskStoppedException : public exception {};

class Interpreter {
    shared_ptr<Environment> env;
    string currentTaskName = "";
    vector<string> callStack;
    
    void runScript(const string& content, shared_ptr<Environment> targetEnv) {
        Lexer l(content);
        Parser p(l.tokenize());
        auto nodes = p.parse();
        auto prev = env;
        env = targetEnv;
        for(auto& n : nodes) execute(n);
        env = prev;
    }

    string httpGet(const string& url) {
        string cmd = "curl -s \"" + url + "\"";
        string result;
        char buffer[128];
        FILE* pipe = popen(cmd.c_str(), "r");
        if(!pipe) throw runtime_error("Cannot execute HTTP request");
        while(fgets(buffer, sizeof(buffer), pipe)) result += buffer;
        pclose(pipe);
        return result;
    }
    
    string runCommand(const string& cmd) {
        string result;
        char buffer[128];
        FILE* pipe = popen(cmd.c_str(), "r");
        if(!pipe) throw runtime_error(string("Cannot execute command: ") + cmd + ". Tip: ensure the command exists and is in PATH.");
        while(fgets(buffer, sizeof(buffer), pipe)) result += buffer;
        pclose(pipe);
        return result;
    }
    
    shared_ptr<Value> parseJSON(const string& json) {
        size_t pos = 0;
        function<shared_ptr<Value>(size_t&)> parseValue = [&](size_t& p) -> shared_ptr<Value> {
            while(p < json.size() && isspace(json[p])) p++;
            if(p >= json.size()) throw runtime_error("JSON parse error");
            
            if(json[p] == '{') {
                p++;
                auto m = make_shared<Value>();
                m->type = Value::MAP;
                while(p < json.size() && json[p] != '}') {
                    while(p < json.size() && (isspace(json[p]) || json[p] == ',')) p++;
                    if(json[p] == '"') {
                        p++;
                        string key;
                        while(p < json.size() && json[p] != '"') key += json[p++];
                        if(p < json.size()) p++;
                        while(p < json.size() && (isspace(json[p]) || json[p] == ':')) p++;
                        auto val = parseValue(p);
                        m->mapData[key] = val;
                    }
                }
                if(p < json.size() && json[p] == '}') p++;
                return m;
            }
            else if(json[p] == '[') {
                p++;
                auto arr = make_shared<Value>();
                arr->type = Value::ARRAY;
                while(p < json.size() && json[p] != ']') {
                    arr->array.push_back(parseValue(p));
                    while(p < json.size() && (isspace(json[p]) || json[p] == ',')) p++;
                }
                if(p < json.size() && json[p] == ']') p++;
                return arr;
            }
            else if(json[p] == '"') {
                p++;
                string str;
                while(p < json.size() && json[p] != '"') str += json[p++];
                if(p < json.size()) p++;
                return make_shared<Value>(str);
            }
            else if(json[p] == 't' || json[p] == 'f') {
                bool val = json[p] == 't';
                p += val ? 4 : 5;
                return make_shared<Value>(val);
            }
            else {
                string num;
                while(p < json.size() && (isdigit(json[p]) || json[p] == '.' || json[p] == '-')) num += json[p++];
                return make_shared<Value>(stod(num));
            }
        };
        return parseValue(pos);
    }
    
    string valueToJSON(shared_ptr<Value> v) {
        if(v->type == Value::MAP) {
            string result = "{";
            bool first = true;
            for(auto& [k, val] : v->mapData) {
                if(!first) result += ",";
                result += "\"" + k + "\":" + valueToJSON(val);
                first = false;
            }
            return result + "}";
        }
        else if(v->type == Value::ARRAY) {
            string result = "[";
            for(size_t i = 0; i < v->array.size(); i++) {
                if(i > 0) result += ",";
                result += valueToJSON(v->array[i]);
            }
            return result + "]";
        }
        else if(v->type == Value::TEXT) {
            return "\"" + v->text + "\"";
        }
        else if(v->type == Value::BOOLEAN) {
            return v->boolean ? "true" : "false";
        }
        else {
            return v->toString();
        }
    }

    shared_ptr<Value> eval(shared_ptr<ASTNode> node) {
        if(auto n = dynamic_pointer_cast<NumberNode>(node))
            return make_shared<Value>(n->value);
        if(auto n = dynamic_pointer_cast<TextNode>(node))
            return make_shared<Value>(n->value);
        if(auto n = dynamic_pointer_cast<BoolNode>(node))
            return make_shared<Value>(n->value);
        if(auto n = dynamic_pointer_cast<IdentifierNode>(node))
            return env->get(n->name);
        
        if(auto n = dynamic_pointer_cast<UserInputNode>(node)) {
            auto promptVal = eval(n->prompt);
            {
                lock_guard<mutex> lock(output_mutex);
                if(promptVal->toString() != "") cout << promptVal->toString();
            }
            string input;
            getline(cin, input);

            if(n->targetType == "integer") {
                size_t idx;
                try {
                    double d = stod(input, &idx);
                    if(idx != input.length()) throw exception();
                    if(floor(d) != d) throw exception();
                    return make_shared<Value>(d);
                } catch(...) {
                    throw runtime_error("Type error: cannot convert '" + input + "' to integer");
                }
            } 
            else if(n->targetType == "number") {
                size_t idx;
                try {
                    double d = stod(input, &idx);
                    if(idx != input.length()) throw exception();
                    return make_shared<Value>(d);
                } catch(...) {
                    throw runtime_error("Type error: cannot convert '" + input + "' to number");
                }
            } 
            else if(n->targetType == "boolean") {
                string lower = input;
                transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if(lower == "true") return make_shared<Value>(true);
                if(lower == "false") return make_shared<Value>(false);
                throw runtime_error("Type error: expected 'true' or 'false'");
            }

            return make_shared<Value>(input);
        }
        
        if(auto n = dynamic_pointer_cast<GetFieldNode>(node)) {
            auto obj = eval(n->object);
            if(obj->type != Value::MAP) {
                throw runtime_error("Can only access fields on objects");
            }
            if(!obj->mapData.count(n->field)) {
                string available;
                for(auto& [k, v] : obj->mapData) available += (available.empty() ? "" : ", ") + k;
                throw runtime_error("Field '" + n->field +" does not exist. Available: " + available);
            }
            return obj->mapData[n->field];
        }
        
        if(auto n = dynamic_pointer_cast<ImportNode>(node)) {
            auto path = eval(n->filepath);
            ifstream f(path->text);
            if(!f) throw runtime_error(string("Cannot import: ") + path->text + ". Tip: check file path and permissions.");
            string content((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
            runScript(content, env);
            return make_shared<Value>();
        }

        if(auto n = dynamic_pointer_cast<HttpGetNode>(node)) {
            auto url = eval(n->url);
            return make_shared<Value>(httpGet(url->text));
        }
        
        if(auto n = dynamic_pointer_cast<RunCommandNode>(node)) {
            auto cmd = eval(n->command);
            string result = runCommand(cmd->text);
            if(n->captureOutput) {
                if(!result.empty() && result.back() == '\n') result.pop_back();
                return make_shared<Value>(result);
            }
            return make_shared<Value>();
        }
        
        if(auto n = dynamic_pointer_cast<ParseNode>(node)) {
            auto json = eval(n->json);
            return parseJSON(json->text);
        }
        
        if(auto n = dynamic_pointer_cast<StringifyNode>(node)) {
            auto mapVal = eval(n->mapExpr);
            return make_shared<Value>(valueToJSON(mapVal));
        }
        
        if(auto n = dynamic_pointer_cast<ReadFileNode>(node)) {
            auto fp = eval(n->filepath);
            ifstream f(fp->text);
            if(!f) throw runtime_error(string("Cannot read file: ") + fp->text + ". Tip: check the path and permissions.");
            
            if(n->asLines) {
                auto arr = make_shared<Value>();
                arr->type = Value::ARRAY;
                string line;
                while(getline(f, line)) {
                    arr->array.push_back(make_shared<Value>(line));
                }
                return arr;
            } else {
                stringstream ss;
                ss << f.rdbuf();
                return make_shared<Value>(ss.str());
            }
        }
        
        if(auto n = dynamic_pointer_cast<FileExistsNode>(node)) {
            auto fp = eval(n->filepath);
            ifstream f(fp->text);
            return make_shared<Value>(f.good());
        }
        
        if(auto n = dynamic_pointer_cast<SplitNode>(node)) {
            auto text = eval(n->text);
            auto delim = eval(n->delimiter);
            auto arr = make_shared<Value>();
            arr->type = Value::ARRAY;
            string str = text->text, d = delim->text;
            size_t pos = 0, found;
            while((found = str.find(d, pos)) != string::npos) {
                arr->array.push_back(make_shared<Value>(str.substr(pos, found - pos)));
                pos = found + d.length();
            }
            arr->array.push_back(make_shared<Value>(str.substr(pos)));
            return arr;
        }
        
        if(auto n = dynamic_pointer_cast<JoinNode>(node)) {
            auto arr = env->get(n->arrayName);
            auto delim = eval(n->delimiter);
            string result;
            for(size_t i = 0; i < arr->array.size(); i++) {
                if(i > 0) result += delim->text;
                result += arr->array[i]->toString();
            }
            return make_shared<Value>(result);
        }
        
        if(auto n = dynamic_pointer_cast<ReplaceNode>(node)) {
            auto text = eval(n->text);
            auto oldStr = eval(n->oldStr);
            auto newStr = eval(n->newStr);
            string str = text->text, o = oldStr->text, nw = newStr->text;
            size_t pos = 0;
            while((pos = str.find(o, pos)) != string::npos) {
                str.replace(pos, o.length(), nw);
                pos += nw.length();
            }
            return make_shared<Value>(str);
        }
        
        if(auto n = dynamic_pointer_cast<UppercaseNode>(node)) {
            auto text = eval(n->text);
            string str = text->text;
            transform(str.begin(), str.end(), str.begin(), ::toupper);
            return make_shared<Value>(str);
        }
        
        if(auto n = dynamic_pointer_cast<LowercaseNode>(node)) {
            auto text = eval(n->text);
            string str = text->text;
            transform(str.begin(), str.end(), str.begin(), ::tolower);
            return make_shared<Value>(str);
        }
        
        if(auto n = dynamic_pointer_cast<TrimNode>(node)) {
            auto text = eval(n->text);
            string str = text->text;
            str.erase(0, str.find_first_not_of(" \t\n\r"));
            str.erase(str.find_last_not_of(" \t\n\r") + 1);
            return make_shared<Value>(str);
        }
        
        if(auto n = dynamic_pointer_cast<SubstringNode>(node)) {
            auto text = eval(n->text);
            auto start = eval(n->start);
            auto len = eval(n->length);
            return make_shared<Value>(text->text.substr((int)start->number, (int)len->number));
        }
        
        if(auto n = dynamic_pointer_cast<ContainsNode>(node)) {
            auto text = eval(n->text);
            auto search = eval(n->search);
            return make_shared<Value>(text->text.find(search->text) != string::npos);
        }
        
        if(auto n = dynamic_pointer_cast<StartsWithNode>(node)) {
            auto text = eval(n->text);
            auto prefix = eval(n->prefix);
            return make_shared<Value>(text->text.find(prefix->text) == 0);
        }
        
        if(auto n = dynamic_pointer_cast<EndsWithNode>(node)) {
            auto text = eval(n->text);
            auto suffix = eval(n->suffix);
            string t = text->text, s = suffix->text;
            return make_shared<Value>(t.size() >= s.size() && 
                t.compare(t.size() - s.size(), s.size(), s) == 0);
        }
        
        if(auto n = dynamic_pointer_cast<MathOpNode>(node)) {
            auto arg1 = eval(n->arg1);
            if(n->op == "floor") return make_shared<Value>(floor(arg1->number));
            if(n->op == "ceiling") return make_shared<Value>(ceil(arg1->number));
            if(n->op == "round") return make_shared<Value>(round(arg1->number));
            if(n->op == "abs") return make_shared<Value>(fabs(arg1->number));
            if(n->op == "sin") return make_shared<Value>(sin(arg1->number));
            if(n->op == "cos") return make_shared<Value>(cos(arg1->number));
            if(n->op == "tan") return make_shared<Value>(tan(arg1->number));
            if(n->op == "sqrt") return make_shared<Value>(sqrt(arg1->number));
            if(n->op == "pow") {
                auto arg2 = eval(n->arg2);
                return make_shared<Value>(pow(arg1->number, arg2->number));
            }
        }
        
        if(auto n = dynamic_pointer_cast<MaxNode>(node)) {
            auto arr = env->get(n->arrayName);
            if(arr->array.empty()) throw runtime_error("Cannot get max of empty array");
            double max = arr->array[0]->number;
            for(auto& v : arr->array) if(v->number > max) max = v->number;
            return make_shared<Value>(max);
        }
        
        if(auto n = dynamic_pointer_cast<MinNode>(node)) {
            auto arr = env->get(n->arrayName);
            if(arr->array.empty()) throw runtime_error("Cannot get min of empty array");
            double min = arr->array[0]->number;
            for(auto& v : arr->array) if(v->number < min) min = v->number;
            return make_shared<Value>(min);
        }
        
        if(auto n = dynamic_pointer_cast<SumNode>(node)) {
            auto arr = env->get(n->arrayName);
            double sum = 0;
            for(auto& v : arr->array) sum += v->number;
            return make_shared<Value>(sum);
        }
        
        if(auto n = dynamic_pointer_cast<AverageNode>(node)) {
            auto arr = env->get(n->arrayName);
            if(arr->array.empty()) throw runtime_error("Cannot get average of empty array");
            double sum = 0;
            for(auto& v : arr->array) sum += v->number;
            return make_shared<Value>(sum / arr->array.size());
        }
        
        if(auto n = dynamic_pointer_cast<KeysNode>(node)) {
            auto m = env->get(n->mapName);
            auto arr = make_shared<Value>();
            arr->type = Value::ARRAY;
            for(auto& [k, v] : m->mapData) arr->array.push_back(make_shared<Value>(k));
            return arr;
        }
        
        if(auto n = dynamic_pointer_cast<ValuesNode>(node)) {
            auto m = env->get(n->mapName);
            auto arr = make_shared<Value>();
            arr->type = Value::ARRAY;
            for(auto& [k, v] : m->mapData) arr->array.push_back(v);
            return arr;
        }
        
        if(auto n = dynamic_pointer_cast<RangeNode>(node)) {
            auto start = eval(n->start);
            auto end = eval(n->end);
            auto arr = make_shared<Value>();
            arr->type = Value::ARRAY;
            for(int i = (int)start->number; i <= (int)end->number; i++)
                arr->array.push_back(make_shared<Value>((double)i));
            return arr;
        }
        
        if(auto n = dynamic_pointer_cast<RandomNode>(node)) {
            auto minVal = eval(n->min);
            auto maxVal = eval(n->max);
            int min = (int)minVal->number;
            int max = (int)maxVal->number;
            return make_shared<Value>((double)(rand() % (max - min + 1) + min));
        }
        
        if(auto n = dynamic_pointer_cast<CurrentTimeNode>(node)) {
            return make_shared<Value>((double)time(nullptr));
        }
        
        if(auto n = dynamic_pointer_cast<DateNode>(node)) {
            time_t now = time(nullptr);
            tm* lt = localtime(&now);
            if(n->part == "year") return make_shared<Value>((double)(lt->tm_year + 1900));
            if(n->part == "month") return make_shared<Value>((double)(lt->tm_mon + 1));
            if(n->part == "day") return make_shared<Value>((double)lt->tm_mday);
            if(n->part == "hour") return make_shared<Value>((double)lt->tm_hour);
            if(n->part == "minute") return make_shared<Value>((double)lt->tm_min);
        }
        
        if(auto n = dynamic_pointer_cast<MapNode>(node)) {
            auto m = make_shared<Value>();
            m->type = Value::MAP;
            for(auto& [k, v] : n->pairs) m->mapData[k] = eval(v);
            return m;
        }
        
        if(auto n = dynamic_pointer_cast<GetMapValueNode>(node)) {
            auto m = env->get(n->mapName);
            auto key = eval(n->key);
            if(m->mapData.count(key->text)) return m->mapData[key->text];
            throw runtime_error("Key not found: " + key->text);
        }
        
        if(auto n = dynamic_pointer_cast<BinaryOpNode>(node)) {
            auto l = eval(n->left);
            auto r = eval(n->right);
            if(n->op == "+") {
                if(l->type == Value::TEXT || r->type == Value::TEXT)
                    return make_shared<Value>(l->toString() + r->toString());
                return make_shared<Value>(l->number + r->number);
            }
            if(n->op == "-") return make_shared<Value>(l->number - r->number);
            if(n->op == "*") return make_shared<Value>(l->number * r->number);
            if(n->op == "/") {
                if(r->number == 0) throw runtime_error("Division by zero");
                return make_shared<Value>(l->number / r->number);
            }
            if(n->op == "%") return make_shared<Value>(fmod(l->number, r->number));
            if(n->op == "==") return make_shared<Value>(l->toString() == r->toString());
            if(n->op == "!=") return make_shared<Value>(l->toString() != r->toString());
            if(n->op == ">") return make_shared<Value>(l->number > r->number);
            if(n->op == "<") return make_shared<Value>(l->number < r->number);
            if(n->op == ">=") return make_shared<Value>(l->number >= r->number);
            if(n->op == "<=") return make_shared<Value>(l->number <= r->number);
            if(n->op == "and") return make_shared<Value>(l->boolean && r->boolean);
            if(n->op == "or") return make_shared<Value>(l->boolean || r->boolean);
        }
        
        if(auto n = dynamic_pointer_cast<UnaryOpNode>(node)) {
            auto op = eval(n->operand);
            if(n->op == "not") return make_shared<Value>(!op->boolean);
            if(n->op == "-") return make_shared<Value>(-op->number);
        }
        
        if(auto n = dynamic_pointer_cast<ArrayNode>(node)) {
            auto arr = make_shared<Value>();
            arr->type = Value::ARRAY;
            for(auto& e : n->elements) arr->array.push_back(eval(e));
            return arr;
        }
        
        if(auto n = dynamic_pointer_cast<GetIndexNode>(node)) {
            auto arr = env->get(n->arrayName);
            auto val = eval(n->value);
            for(size_t i = 0; i < arr->array.size(); i++) {
                if(arr->array[i]->toString() == val->toString())
                    return make_shared<Value>((double)i);
            }
            return make_shared<Value>(-1.0);
        }
        
        if(auto n = dynamic_pointer_cast<GetValueAtNode>(node)) {
            auto arr = env->get(n->arrayName);
            auto idx = eval(n->index);
            int i = (int)idx->number;
            if(i >= 0 && i < (int)arr->array.size())
                return arr->array[i];
            throw runtime_error("Index out of bounds: " + to_string(i));
        }

        if(auto n = dynamic_pointer_cast<GetLastNode>(node)) {
            auto arr = env->get(n->arrayName);
            if(arr->array.empty()) throw runtime_error("Cannot get last item of empty array");
            return arr->array.back();
        }
        
        if(auto n = dynamic_pointer_cast<LengthOfNode>(node)) {
            auto arr = env->get(n->arrayName);
            return make_shared<Value>((double)arr->array.size());
        }
        
        if(auto n = dynamic_pointer_cast<CallNode>(node)) {
            auto func = env->get(n->name);
            if(func->type != Value::FUNCTION) throw runtime_error("'" + n->name + "' is not a function");
            // native implementation
            if(func->func->nativeImpl) {
                vector<shared_ptr<Value>> argv;
                for(auto& a : n->args) argv.push_back(eval(a));
                callStack.push_back(n->name);
                auto res = func->func->nativeImpl(argv);
                callStack.pop_back();
                return res;
            }
            auto fEnv = make_shared<Environment>(func->func->closure);
            for(size_t i = 0; i < func->func->params.size(); i++)
                fEnv->set(func->func->params[i], i < n->args.size() ? eval(n->args[i]) : make_shared<Value>());
            auto prev = env;
            env = fEnv;
            callStack.push_back(n->name);
            try {
                for(auto& s : func->func->body) execute(s);
            } catch(ReturnException& e) {
                callStack.pop_back();
                env = prev;
                return e.value;
            }
            callStack.pop_back();
            env = prev;
            return make_shared<Value>();
        }

        if(auto n = dynamic_pointer_cast<CallFuncNode>(node)) {
            auto fval = eval(n->func);
            if(fval->type != Value::FUNCTION) throw runtime_error("Attempt to call a non-function value");
            if(fval->func->nativeImpl) {
                vector<shared_ptr<Value>> argv;
                for(auto& a : n->args) argv.push_back(eval(a));
                callStack.push_back("<native>");
                auto res = fval->func->nativeImpl(argv);
                callStack.pop_back();
                return res;
            }
            auto fEnv = make_shared<Environment>(fval->func->closure);
            for(size_t i = 0; i < fval->func->params.size(); i++)
                fEnv->set(fval->func->params[i], i < n->args.size() ? eval(n->args[i]) : make_shared<Value>());
            auto prev = env;
            env = fEnv;
            callStack.push_back("<lambda>");
            try {
                for(auto& s : fval->func->body) execute(s);
            } catch(ReturnException& e) {
                callStack.pop_back();
                env = prev;
                return e.value;
            }
            callStack.pop_back();
            env = prev;
            return make_shared<Value>();
        }

        if(auto n = dynamic_pointer_cast<ReceiveFromChannelNode>(node)) {
            lock_guard<mutex> lock(channel_registry_mutex);
            if(channel_registry.count(n->channelName)) {
                double t = -1;
                if(n->timeout) t = eval(n->timeout)->number;
                auto msg = channel_registry[n->channelName]->receiveBlocking(t);
                if(!msg) return make_shared<Value>();
                return msg;
            } else {
                throw runtime_error("Channel '" + n->channelName + "' not found");
            }
        }
        
        return make_shared<Value>();
    }
    
    void execute(shared_ptr<ASTNode> node) {
        if(!currentTaskName.empty()) {
            lock_guard<mutex> lock(task_registry_mutex);
            if(task_registry.count(currentTaskName) && !task_registry[currentTaskName]) {
                throw TaskStoppedException();
            }
        }

        if(auto n = dynamic_pointer_cast<TryCatchNode>(node)) {
            try {
                for(auto& s : n->tryBlock) execute(s);
            } catch(exception& e) {
                env->set(n->errorVar, make_shared<Value>(string(e.what())));
                for(auto& s : n->catchBlock) execute(s);
            }
        }
        else if(auto n = dynamic_pointer_cast<CreateChannelNode>(node)) {
            lock_guard<mutex> lock(channel_registry_mutex);
            channel_registry[n->channelName] = make_shared<Mailbox>();
        }
        else if(auto n = dynamic_pointer_cast<SpawnNode>(node)) {
            auto threadEnv = make_shared<Environment>(*env);
            string name = n->taskName;
            
            if(!name.empty()) {
                lock_guard<mutex> lock(task_registry_mutex);
                task_registry[name] = true;
            }
            
            thread([this, threadEnv, n, name]() {
                Interpreter threadInterp;
                threadInterp.env = threadEnv;
                threadInterp.currentTaskName = name; 
                try {
                    for(auto& s : n->body) threadInterp.execute(s);
                } catch(TaskStoppedException&) {
                } catch(...) {}
            }).detach();
        }
        else if(auto n = dynamic_pointer_cast<StopTaskNode>(node)) {
            lock_guard<mutex> lock(task_registry_mutex);
            task_registry[n->taskName] = false;
        }
        else if(auto n = dynamic_pointer_cast<SendToChannelNode>(node)) {
            auto msg = eval(n->message);
            lock_guard<mutex> lock(channel_registry_mutex);
            if(channel_registry.count(n->channelName)) {
                channel_registry[n->channelName]->send(msg);
            } else {
                throw runtime_error("Channel '" + n->channelName + "' not found");
            }
        }
        else if(auto n = dynamic_pointer_cast<LoadNativeNode>(node)) {
            auto libNameVal = eval(n->lib);
            string libName = libNameVal->text;
            // built-in 'math' module for examples
            if(libName == "math") {
                auto mod = make_shared<Value>();
                mod->type = Value::MAP;
                auto makeNative = [&](function<shared_ptr<Value>(const vector<shared_ptr<Value>>&)> impl){
                    auto fv = make_shared<Value>(); fv->type = Value::FUNCTION; fv->func = make_shared<FunctionValue>(); fv->func->nativeImpl = impl; return fv;
                };
                mod->mapData["square"] = makeNative([](const vector<shared_ptr<Value>>& a){
                    double v = a.size() ? a[0]->number : 0; return make_shared<Value>(v*v);
                });
                mod->mapData["sqrt"] = makeNative([](const vector<shared_ptr<Value>>& a){
                    double v = a.size() ? a[0]->number : 0; return make_shared<Value>(sqrt(v));
                });
                env->set(n->alias, mod);
            } else {
                throw runtime_error("Native module '" + libName + "' not found. Tip: only built-in 'math' is available in this build.");
            }
        }
        else if(auto n = dynamic_pointer_cast<TraceNode>(node)) {
            lock_guard<mutex> lock(output_mutex);
            if(callStack.empty()) cout << "Trace: (empty)" << endl;
            else {
                cout << "Trace (most recent last):" << endl;
                for(auto& s : callStack) cout << " - " << s << endl;
            }
        }
        else if(auto n = dynamic_pointer_cast<ReceiveFromChannelNode>(node)) {
            lock_guard<mutex> lock(channel_registry_mutex);
            if(channel_registry.count(n->channelName)) {
                double t = -1;
                if(n->timeout) t = eval(n->timeout)->number;
                auto msg = channel_registry[n->channelName]->receiveBlocking(t);
                // on statement receive we discard the message
            } else {
                throw runtime_error("Channel '" + n->channelName + "' not found");
            }
        }
        else if(auto n = dynamic_pointer_cast<AwaitNode>(node)) {
            auto futureVal = eval(n->task);
            if(futureVal->type != Value::FUTURE) 
                throw runtime_error("Can only await async tasks");
            futureVal->futureVal.get();
        }
        else if(auto n = dynamic_pointer_cast<SetNode>(node)) {
            env->set(n->name, eval(n->value));
        }
        else if(auto n = dynamic_pointer_cast<SetFieldNode>(node)) {
            auto obj = eval(n->object);
            if(obj->type != Value::MAP) throw runtime_error("Can only set fields on objects");
            obj->mapData[n->field] = eval(n->value);
        }
        else if(auto n = dynamic_pointer_cast<DisplayNode>(node)) {
            lock_guard<mutex> lock(output_mutex);
            cout << eval(n->value)->toString() << endl;
        }
        else if(auto n = dynamic_pointer_cast<IfNode>(node)) {
            auto cond = eval(n->condition);
            if(cond->boolean) {
                for(auto& s : n->thenBlock) execute(s);
            } else {
                for(auto& s : n->elseBlock) execute(s);
            }
        }
        else if(auto n = dynamic_pointer_cast<WhileNode>(node)) {
            while(eval(n->condition)->boolean) {
                try {
                    for(auto& s : n->body) execute(s);
                } catch(BreakException&) {
                    break;
                } catch(ContinueException&) {
                    continue;
                }
            }
        }
        else if(auto n = dynamic_pointer_cast<IterateNode>(node)) {
            int start = (int)eval(n->start)->number;
            int end = (int)eval(n->end)->number;
            for(int i = start; i <= end; i++) {
                env->set(n->variable, make_shared<Value>((double)i));
                try {
                    for(auto& s : n->body) execute(s);
                } catch(BreakException&) {
                    break;
                } catch(ContinueException&) {
                    continue;
                }
            }
        }
        else if(auto n = dynamic_pointer_cast<ForEachNode>(node)) {
            auto arr = eval(n->array);
            if(arr->type != Value::ARRAY)
                throw runtime_error("for each requires an array");
            for(auto& item : arr->array) {
                env->set(n->variable, item);
                try {
                    for(auto& s : n->body) execute(s);
                } catch(BreakException&) {
                    break;
                } catch(ContinueException&) {
                    continue;
                }
            }
        }
        else if(auto n = dynamic_pointer_cast<FunctionDefNode>(node)) {
            auto fv = make_shared<FunctionValue>();
            fv->params = n->params;
            fv->body = n->body;
            fv->closure = env;
            auto func = make_shared<Value>();
            func->type = Value::FUNCTION;
            func->func = fv;
            env->set(n->name, func);
        }
        else if(auto n = dynamic_pointer_cast<ReturnNode>(node)) {
            throw ReturnException(eval(n->value));
        }
        else if(auto n = dynamic_pointer_cast<AddToArrayNode>(node)) {
            auto arr = env->get(n->arrayName);
            arr->array.push_back(eval(n->value));
        }
        else if(auto n = dynamic_pointer_cast<RemoveFromArrayNode>(node)) {
            auto arr = env->get(n->arrayName);
            auto val = eval(n->value);
            arr->array.erase(remove_if(arr->array.begin(), arr->array.end(),
                [&](auto& v) { return v->toString() == val->toString(); }), arr->array.end());
        }
        else if(auto n = dynamic_pointer_cast<WriteFileNode>(node)) {
            auto fp = eval(n->filepath);
            auto content = eval(n->content);
            ofstream f(fp->text);
            if(!f) throw runtime_error("Cannot write file: " + fp->text);
            f << content->toString();
        }
        else if(auto n = dynamic_pointer_cast<AppendFileNode>(node)) {
            auto fp = eval(n->filepath);
            auto content = eval(n->content);
            ofstream f(fp->text, ios::app);
            if(!f) throw runtime_error("Cannot append to file: " + fp->text);
            f << content->toString();
        }
        else if(auto n = dynamic_pointer_cast<DeleteFileNode>(node)) {
            auto fp = eval(n->filepath);
            remove(fp->text.c_str());
        }
        else if(auto n = dynamic_pointer_cast<SortNode>(node)) {
            auto arr = env->get(n->arrayName);
            sort(arr->array.begin(), arr->array.end(), [](auto& a, auto& b) {
                return a->type == Value::NUMBER && b->type == Value::NUMBER ? 
                    a->number < b->number : a->toString() < b->toString();
            });
        }
        else if(auto n = dynamic_pointer_cast<ReverseNode>(node)) {
            auto arr = env->get(n->arrayName);
            reverse(arr->array.begin(), arr->array.end());
        }
        else if(auto n = dynamic_pointer_cast<UniqueNode>(node)) {
            auto arr = env->get(n->arrayName);
            vector<shared_ptr<Value>> unique;
            for(auto& v : arr->array) {
                bool found = false;
                for(auto& u : unique) {
                    if(v->toString() == u->toString()) { found = true; break; }
                }
                if(!found) unique.push_back(v);
            }
            arr->array = unique;
        }
        else if(auto n = dynamic_pointer_cast<RepeatNode>(node)) {
            int count = (int)eval(n->count)->number;
            for(int i = 0; i < count; i++) {
                try {
                    for(auto& s : n->body) execute(s);
                } catch(BreakException&) {
                    break;
                } catch(ContinueException&) {
                    continue;
                }
            }
        }
        else if(auto n = dynamic_pointer_cast<WaitNode>(node)) {
            auto sec = eval(n->seconds);
            this_thread::sleep_for(chrono::milliseconds((int)(sec->number * 1000)));
        }
        else if(auto n = dynamic_pointer_cast<BreakNode>(node)) {
            throw BreakException();
        }
        else if(auto n = dynamic_pointer_cast<ContinueNode>(node)) {
            throw ContinueException();
        }
        else if(auto n = dynamic_pointer_cast<ExitNode>(node)) {
            exit(0);
        }
        else {
            eval(node);
        }
    }
    
public:
    Interpreter() : env(make_shared<Environment>()) { srand(time(0)); }
    
    void run(const vector<shared_ptr<ASTNode>>& prog) {
        for(auto& s : prog) execute(s);
    }
};

int main(int argc, char** argv) {
    string code = R"(
display "=== Nibyo v4.0 - Full Feature Demo ==="
display ""

# DEMO 1: DOT NOTATION FOR OBJECTS
display "1. Objects with Dot Notation:"
set user to object with:
    name: "Alice"
    age: 30
    city: "NYC"
end object

display "Name: " + user.name
display "Age: " + user.age
set user.age to 31
display "Updated age: " + user.age
display ""

# DEMO 2: CHANNELS FOR MESSAGE PASSING
display "2. Channels (Send/Receive):"
create channel "results"

in the background as "worker" do:
    wait 1 second
    send "Task complete!" to channel "results"
end task

wait 2 second
display "Worker sent a message via channel"
display ""

# DEMO 3: SYSTEM COMMANDS
display "3. Running System Commands:"
set output to run "echo Hello from Nibyo" and capture output
display "Command output: " + output
display ""

# DEMO 4: FILE OPERATIONS
display "4. File Operations:"
write "Nibyo is awesome!" into file "test.txt"
set content to read file "test.txt"
display "File content: " + content
display ""

# DEMO 5: JSON PARSING
display "5. JSON Parse & Stringify:"
set json_str to "{\"product\": \"Nibyo\", \"version\": 4.0}"
set data to parse json_str
display "Parsed product: " + data.product
display ""

# DEMO 6: ARRAY OPERATIONS WITH FOR EACH
display "6. Array Operations:"
set numbers to [10, 20, 30, 40, 50]
set total to 0
for each num in numbers do:
    set total to total + num
end for
display "Sum of array: " + total
display ""

# DEMO 7: ERROR HANDLING
display "7. Error Handling:"
try:
    set x to 10
    set y to 0
    display x / y
if there is an error set err to error and do:
    display "Caught error: " + err
end try
display ""

display "=== Demo Complete ==="
)";
    
    if(argc > 1) {
        ifstream f(argv[1]);
        if(!f) { cerr << "Cannot open file" << endl; return 1; }
        code = string((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
    }
    
    try {
        Lexer lex(code);
        auto tokens = lex.tokenize();
        
        Parser parser(tokens);
        auto program = parser.parse();
        
        Interpreter interp;
        interp.run(program);
        
    } catch(exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}