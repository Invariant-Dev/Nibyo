// nexo v1.0 beta - natural english programming language
// lexer.h - lexer declaration
#pragma once

#include "Token.h"
#include <string>
#include <vector>
#include <map>
#include <unordered_set>

class Lexer {
private:
    std::string source;
    size_t pos = 0;
    int line = 1;
    int column = 1;
    
    std::map<std::string, TokenType> keywords;
    std::unordered_set<std::string> noiseWords;
    
    char peek() const;
    char peekNext() const;
    char advance();
    void skipWhitespace();
    void skipComment();
    void skipMultilineComment();
    bool isAtEnd() const;
    
    bool isDigit(char c) const;
    bool isAlpha(char c) const;
    bool isAlphaNumeric(char c) const;
    
    Token makeToken(TokenType type, const std::string& value);
    Token readNumber();
    Token readString();
    Token readIdentifier();
    
    std::string processEscapeSequences(const std::string& s);
    bool isNoiseWord(const std::string& word) const;
    bool matchKeywordSequence(const std::string& seq);
    
public:
    explicit Lexer(const std::string& src);
    std::vector<Token> tokenize();
};
