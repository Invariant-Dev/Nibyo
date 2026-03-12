// nexo v1.0 beta - natural english programming language
// lexer.cpp - tokenizer implementation
#include "Lexer.h"
#include <stdexcept>
#include <algorithm>

Lexer::Lexer(const std::string& src) : source(src) {
    // initialize keywords map
    keywords = {
        // boolean
        {"true", T_TRUE}, {"false", T_FALSE},
        
        // core keywords
        {"set", T_SET}, {"the", T_THE}, {"to", T_TO}, {"an", T_AN}, {"a", T_A},
        {"is", T_IS}, {"are", T_ARE}, {"it", T_IT}, {"in", T_IN}, {"of", T_OF},
        {"at", T_AT}, {"by", T_BY}, {"with", T_WITH}, {"and", T_AND}, {"or", T_OR},
        {"not", T_NOT}, {"as", T_AS}, {"for", T_FOR}, {"from", T_FROM},
        {"through", T_THROUGH}, {"using", T_USING}, {"called", T_CALLED}, {"named", T_NAMED},
        
        // display and input
        {"display", T_DISPLAY}, {"message", T_MESSAGE}, {"ask", T_ASK}, {"store", T_STORE},
        {"user", T_USER}, {"prompt", T_PROMPT}, {"name", T_NAME},
        
        // conditionals
        {"if", T_IF}, {"then", T_THEN}, {"else", T_ELSE}, {"otherwise", T_OTHERWISE},
        {"do", T_DO}, {"following", T_FOLLOWING}, {"end", T_END},
        
        // loops
        {"while", T_WHILE}, {"repeat", T_REPEAT}, {"times", T_TIMES},
        {"iterate", T_ITERATE}, {"each", T_EACH}, {"loop", T_LOOP},
        {"break", T_BREAK}, {"continue", T_CONTINUE}, {"exit", T_EXIT},
        
        // functions
        {"define", T_DEFINE}, {"function", T_FUNCTION}, {"parameter", T_PARAMETER},
        {"parameters", T_PARAMETERS}, {"call", T_CALL}, {"return", T_RETURN},
        
        // comparisons
        {"equal", T_EQUAL}, {"same", T_SAME}, {"exactly", T_EXACTLY},
        {"greater", T_GREATER}, {"less", T_LESS}, {"more", T_MORE}, {"fewer", T_FEWER},
        {"than", T_THAN}, {"least", T_LEAST}, {"most", T_MOST}, {"different", T_DIFFERENT},
        
        // math
        {"plus", T_PLUS}, {"minus", T_MINUS}, {"multiplied", T_MULTIPLY},
        {"divided", T_DIVIDE}, {"modulo", T_MODULO}, {"remainder", T_MODULO},
        {"floor", T_FLOOR}, {"ceiling", T_CEILING}, {"round", T_ROUND},
        {"absolute", T_ABSOLUTE}, {"power", T_POWER}, {"raised", T_RAISED},
        {"square", T_SQUARE}, {"root", T_ROOT}, {"sine", T_SINE},
        {"cosine", T_COSINE}, {"tangent", T_TANGENT}, {"random", T_RANDOM},
        {"negative", T_NEGATIVE}, {"value", T_VALUE},
        
        // arrays/lists
        {"list", T_LIST}, {"array", T_ARRAY}, {"empty", T_EMPTY},
        {"containing", T_CONTAINING}, {"add", T_ADD}, {"remove", T_REMOVE},
        {"get", T_GET}, {"first", T_FIRST}, {"second", T_SECOND}, {"third", T_THIRD},
        {"last", T_LAST}, {"item", T_ITEM}, {"length", T_LENGTH},
        {"sort", T_SORT}, {"reverse", T_REVERSE}, {"unique", T_UNIQUE},
        {"join", T_JOIN}, {"joined", T_JOINED}, {"split", T_SPLIT},
        {"maximum", T_MAXIMUM}, {"minimum", T_MINIMUM}, {"sum", T_SUM}, {"average", T_AVERAGE},
        
        // objects
        {"object", T_OBJECT}, {"create", T_CREATE}, {"new", T_NEW},
        {"properties", T_PROPERTIES}, {"property", T_PROPERTY},
        {"keys", T_KEYS}, {"values", T_VALUES},
        
        // strings
        {"text", T_TEXT}, {"uppercase", T_UPPERCASE}, {"lowercase", T_LOWERCASE},
        {"trim", T_TRIM}, {"substring", T_SUBSTRING}, {"replace", T_REPLACE},
        {"contains", T_CONTAINS}, {"starts", T_STARTS}, {"ends", T_ENDS},
        
        // files
        {"file", T_FILE}, {"read", T_READ}, {"write", T_WRITE},
        {"append", T_APPEND}, {"delete", T_DELETE}, {"exists", T_EXISTS},
        {"into", T_INTO}, {"lines", T_LINES},
        
        // time
        {"current", T_CURRENT}, {"time", T_TIME}, {"year", T_YEAR},
        {"month", T_MONTH}, {"day", T_DAY}, {"hour", T_HOUR},
        {"minute", T_MINUTE}, {"seconds", T_SECONDS}, {"second", T_SECONDS}, {"wait", T_WAIT},
        
        // error handling
        {"try", T_TRY}, {"error", T_ERROR}, {"occurs", T_OCCURS},
        
        // http/json
        {"http", T_HTTP}, {"send", T_SEND}, {"request", T_REQUEST},
        {"response", T_RESPONSE}, {"parse", T_PARSE}, {"stringify", T_STRINGIFY}, {"json", T_JSON},
        
        // concurrency
        {"background", T_BACKGROUND}, {"task", T_TASK}, {"channel", T_CHANNEL},
        {"receive", T_RECEIVE}, {"stop", T_STOP}, {"finish", T_FINISH}, {"await", T_AWAIT},
        
        // gui
        {"open", T_OPEN}, {"close", T_CLOSE}, {"gui", T_GUI}, {"window", T_WINDOW},
        {"size", T_SIZE}, {"draw", T_DRAW}, {"button", T_BUTTON}, {"label", T_LABEL},
        {"input", T_INPUT}, {"placeholder", T_PLACEHOLDER}, {"position", T_POSITION},
        {"rectangle", T_RECTANGLE}, {"circle", T_CIRCLE}, {"radius", T_RADIUS},
        {"color", T_COLOR}, {"when", T_WHEN}, {"clicked", T_CLICKED}, {"code", T_CODE},
        
        // colors
        {"red", T_RED}, {"green", T_GREEN}, {"blue", T_BLUE},
        {"white", T_WHITE}, {"black", T_BLACK}, {"yellow", T_YELLOW},
        {"orange", T_ORANGE}, {"purple", T_PURPLE}, {"pink", T_PINK}, {"gray", T_GRAY},
        
        // modules
        {"load", T_LOAD}, {"native", T_NATIVE}, {"import", T_IMPORT},
        
        // misc
        {"environment", T_ENVIRONMENT}, {"variable", T_VARIABLE},
        {"run", T_RUN}, {"capture", T_CAPTURE}, {"output", T_OUTPUT},
        {"trace", T_TRACE}, {"range", T_RANGE}, {"number", T_NUMBER_TYPE},
        {"integer", T_INTEGER}, {"boolean", T_BOOLEAN}, {"there", T_THERE},
        
        // comments
        {"ignore", T_IGNORE}, {"this", T_THIS}, {"line", T_LINE}, {"all", T_ALL},
    };
    
    // noise words to be filtered out
    noiseWords = {"please", "can", "you", "new", "variable"};
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source[pos];
}

char Lexer::peekNext() const {
    if (pos + 1 >= source.size()) return '\0';
    return source[pos + 1];
}

char Lexer::advance() {
    if (isAtEnd()) return '\0';
    char c = source[pos++];
    if (c == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }
    return c;
}

void Lexer::skipWhitespace() {
    while (!isAtEnd() && std::isspace(peek())) {
        advance();
    }
}

void Lexer::skipComment() {
    // skip to end of line
    while (!isAtEnd() && peek() != '\n') {
        advance();
    }
}

void Lexer::skipMultilineComment() {
    // look for "end ignore"
    std::string endMarker = "end ignore";
    while (!isAtEnd()) {
        // check if we're at "end ignore"
        if (pos + endMarker.length() <= source.size()) {
            std::string check = source.substr(pos, endMarker.length());
            std::transform(check.begin(), check.end(), check.begin(), ::tolower);
            if (check == endMarker) {
                pos += endMarker.length();
                return;
            }
        }
        advance();
    }
    throw std::runtime_error("Line " + std::to_string(line) + ": Unclosed multi-line comment. Use 'end ignore' to close.");
}

bool Lexer::isAtEnd() const {
    return pos >= source.size();
}

bool Lexer::isDigit(char c) const {
    return c >= '0' && c <= '9';
}

bool Lexer::isAlpha(char c) const {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool Lexer::isAlphaNumeric(char c) const {
    return isAlpha(c) || isDigit(c);
}

Token Lexer::makeToken(TokenType type, const std::string& value) {
    return Token(type, value, line, column);
}

Token Lexer::readNumber() {
    std::string num;
    while (!isAtEnd() && (isDigit(peek()) || peek() == '.')) {
        num += advance();
    }
    return makeToken(T_NUMBER, num);
}

std::string Lexer::processEscapeSequences(const std::string& s) {
    std::string result;
    for (size_t i = 0; i < s.length(); i++) {
        if (s[i] == '\\' && i + 1 < s.length()) {
            switch (s[i + 1]) {
                case 'n': result += '\n'; i++; break;
                case 't': result += '\t'; i++; break;
                case 'r': result += '\r'; i++; break;
                case '\\': result += '\\'; i++; break;
                case '"': result += '"'; i++; break;
                default: result += s[i]; break;
            }
        } else {
            result += s[i];
        }
    }
    return result;
}

Token Lexer::readString() {
    advance(); // skip opening quote
    std::string str;
    while (!isAtEnd() && peek() != '"') {
        if (peek() == '\\' && peekNext() == '"') {
            advance(); // Skip backslash
            str += advance(); // Add quote
        } else {
            str += advance();
        }
    }
    if (isAtEnd()) {
        throw std::runtime_error("Line " + std::to_string(line) + ": Unterminated string.");
    }
    advance(); // Skip closing quote
    return makeToken(T_STRING, processEscapeSequences(str));
}

bool Lexer::isNoiseWord(const std::string& word) const {
    std::string lower = word;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return noiseWords.count(lower) > 0;
}

Token Lexer::readIdentifier() {
    std::string id;
    while (!isAtEnd() && (isAlphaNumeric(peek()))) {
        id += advance();
    }
    
    // check for possessive form (user's)
    if (peek() == '\'' && peekNext() == 's') {
        std::string lower = id;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        auto it = keywords.find(lower);
        Token tok = (it != keywords.end()) 
            ? makeToken(it->second, id)
            : makeToken(T_IDENTIFIER, id);
        advance(); // Skip '
        advance(); // Skip s
        // The caller should handle the possessive by emitting a DOT token next
        return tok;
    }
    
    // convert to lowercase for keyword lookup
    std::string lower = id;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    // check if it's a keyword
    auto it = keywords.find(lower);
    if (it != keywords.end()) {
        return makeToken(it->second, id);
    }
    
    return makeToken(T_IDENTIFIER, id);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    
    while (!isAtEnd()) {
        skipWhitespace();
        if (isAtEnd()) break;
        
        // check for "ignore this line:" comment
        if (pos + 17 <= source.size()) {
            std::string check = source.substr(pos, 17);
            std::transform(check.begin(), check.end(), check.begin(), ::tolower);
            if (check == "ignore this line:") {
                pos += 17;
                skipComment();
                continue;
            }
        }
        
        // check for "ignore all this:" multi-line comment
        if (pos + 16 <= source.size()) {
            std::string check = source.substr(pos, 16);
            std::transform(check.begin(), check.end(), check.begin(), ::tolower);
            if (check == "ignore all this:") {
                pos += 16;
                skipMultilineComment();
                continue;
            }
        }
        
        // check for # comment
        if (peek() == '#') {
            skipComment();
            continue;
        }
        
        char c = peek();
        
        // numbers
        if (isDigit(c)) {
            tokens.push_back(readNumber());
            continue;
        }
        
        // strings
        if (c == '"') {
            tokens.push_back(readString());
            continue;
        }
        
        // identifiers and keywords
        if (isAlpha(c)) {
            Token tok = readIdentifier();
            
            // check for possessive - emit dot token
            if (!tokens.empty() && tokens.back().type == T_IDENTIFIER && peek() == '\'') {
                // already handled in readidentifier
            }
            
            // skip noise words
            if (isNoiseWord(tok.value)) {
                continue;
            }
            
            // check for possessive after identifier
            if (source.length() > pos + 1 && source[pos] == '\'' && source[pos+1] == 's') {
                tokens.push_back(tok);
                advance(); // '
                advance(); // s
                tokens.push_back(makeToken(T_DOT, "."));
                continue;
            }
            
            tokens.push_back(tok);
            continue;
        }
        
        // operators and punctuation
        switch (c) {
            case '+': advance(); tokens.push_back(makeToken(T_PLUS, "+")); break;
            case '-': advance(); tokens.push_back(makeToken(T_MINUS, "-")); break;
            case '*': advance(); tokens.push_back(makeToken(T_MULTIPLY, "*")); break;
            case '/': advance(); tokens.push_back(makeToken(T_DIVIDE, "/")); break;
            case '%': advance(); tokens.push_back(makeToken(T_MODULO, "%")); break;
            case '(': advance(); tokens.push_back(makeToken(T_LPAREN, "(")); break;
            case ')': advance(); tokens.push_back(makeToken(T_RPAREN, ")")); break;
            case '[': advance(); tokens.push_back(makeToken(T_LBRACKET, "[")); break;
            case ']': advance(); tokens.push_back(makeToken(T_RBRACKET, "]")); break;
            case ',': advance(); tokens.push_back(makeToken(T_COMMA, ",")); break;
            case ':': advance(); tokens.push_back(makeToken(T_COLON, ":")); break;
            case '.': advance(); tokens.push_back(makeToken(T_DOT, ".")); break;
            default:
                // skip unknown characters
                advance();
                break;
        }
    }
    
    tokens.push_back(makeToken(T_EOF, ""));
    return tokens;
}
