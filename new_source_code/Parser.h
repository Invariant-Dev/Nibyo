// nibyo v1.0 beta - natural english programming language
// parser.h - parser declaration
#pragma once

#include "Token.h"
#include "AST.h"
#include <vector>
#include <memory>

class Parser {
private:
    std::vector<Token> tokens;
    size_t pos = 0;
    
    Token peek() const;
    Token peekNext() const;
    Token advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    void expect(TokenType type, const std::string& msg);
    bool isAtEnd() const;
    int currentLine() const;
    
    std::shared_ptr<ASTNode> expression();
    std::shared_ptr<ASTNode> orExpr();
    std::shared_ptr<ASTNode> andExpr();
    std::shared_ptr<ASTNode> compExpr();
    std::shared_ptr<ASTNode> addExpr();
    std::shared_ptr<ASTNode> mulExpr();
    std::shared_ptr<ASTNode> unaryExpr();
    std::shared_ptr<ASTNode> postfixExpr();
    std::shared_ptr<ASTNode> primary();
    std::shared_ptr<ASTNode> statement();
    
    std::vector<std::shared_ptr<ASTNode>> parseBlock(TokenType endToken);
    
public:
    explicit Parser(const std::vector<Token>& toks);
    std::vector<std::shared_ptr<ASTNode>> parse();
};
