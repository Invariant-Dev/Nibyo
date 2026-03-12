// Nexo v5.0 - Natural English Programming Language
// Parser.cpp - Parser implementation (Complete with all features)
#include "Parser.h"
#include <stdexcept>
#include <algorithm>

Parser::Parser(const std::vector<Token>& toks) : tokens(toks) {}

Token Parser::peek() const {
    return pos < tokens.size() ? tokens[pos] : tokens.back();
}

Token Parser::peekNext() const {
    return (pos + 1) < tokens.size() ? tokens[pos + 1] : tokens.back();
}

Token Parser::advance() {
    if (!isAtEnd()) pos++;
    return tokens[pos - 1];
}

bool Parser::check(TokenType type) const {
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) { advance(); return true; }
    return false;
}

void Parser::expect(TokenType type, const std::string& msg) {
    if (!match(type)) {
        throw std::runtime_error("Line " + std::to_string(currentLine()) + ": " + msg);
    }
}

bool Parser::isAtEnd() const {
    return peek().type == T_EOF;
}

int Parser::currentLine() const {
    return peek().line;
}

std::vector<std::shared_ptr<ASTNode>> Parser::parseBlock(TokenType endToken) {
    std::vector<std::shared_ptr<ASTNode>> block;
    while (!isAtEnd() && !check(endToken)) {
        block.push_back(statement());
    }
    return block;
}

std::vector<std::shared_ptr<ASTNode>> Parser::parse() {
    std::vector<std::shared_ptr<ASTNode>> program;
    while (!isAtEnd()) {
        program.push_back(statement());
    }
    return program;
}

// Expression parsing with precedence
std::shared_ptr<ASTNode> Parser::expression() { return orExpr(); }

std::shared_ptr<ASTNode> Parser::orExpr() {
    auto left = andExpr();
    while (match(T_OR)) {
        auto right = andExpr();
        auto node = std::make_shared<BinaryOpNode>();
        node->left = left; node->right = right; node->op = "or";
        left = node;
    }
    return left;
}

std::shared_ptr<ASTNode> Parser::andExpr() {
    auto left = compExpr();
    while (match(T_AND)) {
        auto right = compExpr();
        auto node = std::make_shared<BinaryOpNode>();
        node->left = left; node->right = right; node->op = "and";
        left = node;
    }
    return left;
}

std::shared_ptr<ASTNode> Parser::compExpr() {
    auto left = addExpr();
    
    if (match(T_IS)) {
        std::string op;
        
        if (match(T_THE)) match(T_SAME);
        if (match(T_SAME)) { match(T_AS); op = "=="; }
        else if (match(T_EXACTLY)) { op = "=="; }
        else if (match(T_NOT)) {
            if (match(T_EQUAL)) { match(T_TO); op = "!="; }
            else if (match(T_SAME)) { match(T_AS); op = "!="; }
            else { op = "!="; }
        }
        else if (match(T_DIFFERENT)) { match(T_FROM); op = "!="; }
        else if (match(T_GREATER) || match(T_MORE)) { match(T_THAN); op = ">"; }
        else if (match(T_LESS) || match(T_FEWER)) { match(T_THAN); op = "<"; }
        else if (match(T_AT)) {
            if (match(T_LEAST)) op = ">=";
            else if (match(T_MOST)) op = "<=";
        }
        else if (match(T_EQUAL)) { match(T_TO); op = "=="; }
        else { op = "=="; }
        
        auto right = addExpr();
        auto node = std::make_shared<BinaryOpNode>();
        node->left = left; node->right = right; node->op = op;
        return node;
    }
    
    // "contains" as postfix: expr contains expr
    if (match(T_CONTAINS)) {
        auto search = addExpr();
        auto node = std::make_shared<ContainsNode>();
        node->text = left;
        node->search = search;
        return node;
    }
    
    // "starts with" as postfix
    if (match(T_STARTS)) {
        match(T_WITH);
        auto prefix = addExpr();
        auto node = std::make_shared<StartsWithNode>();
        node->text = left;
        node->prefix = prefix;
        return node;
    }
    
    // "ends with" as postfix
    if (match(T_ENDS)) {
        match(T_WITH);
        auto suffix = addExpr();
        auto node = std::make_shared<EndsWithNode>();
        node->text = left;
        node->suffix = suffix;
        return node;
    }
    
    return left;
}

std::shared_ptr<ASTNode> Parser::addExpr() {
    auto left = mulExpr();
    
    while (check(T_PLUS) || check(T_MINUS) || check(T_JOINED)) {
        std::string op;
        if (match(T_PLUS)) op = "+";
        else if (match(T_MINUS)) op = "-";
        else if (match(T_JOINED)) { match(T_WITH); op = "+"; }
        else break;
        
        auto right = mulExpr();
        auto node = std::make_shared<BinaryOpNode>();
        node->left = left; node->right = right; node->op = op;
        left = node;
    }
    return left;
}

std::shared_ptr<ASTNode> Parser::mulExpr() {
    auto left = unaryExpr();
    
    while (check(T_MULTIPLY) || check(T_DIVIDE) || check(T_MODULO)) {
        std::string op;
        if (match(T_MULTIPLY)) op = "*";
        else if (match(T_DIVIDE)) { match(T_BY); op = "/"; }
        else if (match(T_MODULO)) op = "%";
        else break;
        
        auto right = unaryExpr();
        auto node = std::make_shared<BinaryOpNode>();
        node->left = left; node->right = right; node->op = op;
        left = node;
    }
    return left;
}

std::shared_ptr<ASTNode> Parser::unaryExpr() {
    if (match(T_NOT)) {
        auto node = std::make_shared<UnaryOpNode>();
        node->operand = unaryExpr();
        node->op = "not";
        return node;
    }
    if (match(T_MINUS)) {
        auto node = std::make_shared<UnaryOpNode>();
        node->operand = unaryExpr();
        node->op = "-";
        return node;
    }
    if (match(T_NEGATIVE)) {
        auto node = std::make_shared<UnaryOpNode>();
        node->operand = unaryExpr();
        node->op = "-";
        return node;
    }
    return postfixExpr();
}

std::shared_ptr<ASTNode> Parser::postfixExpr() {
    auto left = primary();
    
    // Dot notation: user.name
    while (match(T_DOT)) {
        if (!check(T_IDENTIFIER)) {
            throw std::runtime_error("Line " + std::to_string(currentLine()) + 
                ": Expected property name after '.'");
        }
        std::string field = advance().value;
        auto node = std::make_shared<GetFieldNode>();
        node->object = left;
        node->field = field;
        left = node;
    }
    
    // Postfix "exists" for files
    if (match(T_EXISTS)) {
        auto node = std::make_shared<FileExistsNode>();
        node->filepath = left;
        return node;
    }
    
    return left;
}

// Primary expressions: literals, identifiers, built-in functions
std::shared_ptr<ASTNode> Parser::primary() {
    // Numbers
    if (check(T_NUMBER)) {
        auto node = std::make_shared<NumberNode>();
        node->value = std::stod(advance().value);
        return node;
    }
    
    // Strings
    if (check(T_STRING)) {
        auto node = std::make_shared<TextNode>();
        node->value = advance().value;
        return node;
    }
    
    // Booleans
    if (match(T_TRUE)) {
        auto node = std::make_shared<BoolNode>();
        node->value = true;
        return node;
    }
    if (match(T_FALSE)) {
        auto node = std::make_shared<BoolNode>();
        node->value = false;
        return node;
    }
    
    // "length of X"
    if (match(T_LENGTH)) {
        match(T_OF);
        auto node = std::make_shared<LengthOfNode>();
        node->target = expression();
        return node;
    }
    
    // "the first/last/Nth item in X"
    if (match(T_THE)) {
        if (match(T_FIRST)) {
            match(T_ITEM); match(T_IN);
            std::string arr = advance().value;
            auto node = std::make_shared<GetFirstNode>();
            node->arrayName = arr;
            return node;
        }
        if (match(T_LAST)) {
            match(T_ITEM); match(T_IN);
            std::string arr = advance().value;
            auto node = std::make_shared<GetLastNode>();
            node->arrayName = arr;
            return node;
        }
        // "the keys of X"
        if (match(T_KEYS)) {
            match(T_OF);
            auto n = std::make_shared<KeysNode>();
            n->mapName = advance().value;
            return n;
        }
        // "the values of X"
        if (match(T_VALUES)) {
            match(T_OF);
            auto n = std::make_shared<ValuesNode>();
            n->mapName = advance().value;
            return n;
        }
        // "the environment variable X"
        if (match(T_ENVIRONMENT)) {
            match(T_VARIABLE);
            auto n = std::make_shared<GetEnvVarNode>();
            n->varName = expression();
            return n;
        }
        // "the X of Y" - field access
        if (check(T_IDENTIFIER)) {
            std::string field = advance().value;
            if (match(T_OF)) {
                auto obj = expression();
                auto node = std::make_shared<GetFieldNode>();
                node->object = obj;
                node->field = field;
                return node;
            }
            // Just identifier
            auto node = std::make_shared<IdentifierNode>();
            node->name = field;
            return node;
        }
    }
    
    // "first item in X"
    if (match(T_FIRST)) {
        match(T_ITEM); match(T_IN);
        std::string arr = advance().value;
        auto node = std::make_shared<GetFirstNode>();
        node->arrayName = arr;
        return node;
    }
    
    // "last item in X"
    if (match(T_LAST)) {
        match(T_ITEM); match(T_IN);
        std::string arr = advance().value;
        auto node = std::make_shared<GetLastNode>();
        node->arrayName = arr;
        return node;
    }
    
    // "item N of X"
    if (match(T_ITEM)) {
        auto idx = expression();
        match(T_OF);
        std::string arr = advance().value;
        auto node = std::make_shared<GetValueAtNode>();
        node->index = idx;
        node->arrayName = arr;
        return node;
    }
    
    // "read the file X" / "read file X as lines"
    if (match(T_READ)) {
        match(T_THE); match(T_FILE);
        auto filepath = expression();
        auto node = std::make_shared<ReadFileNode>();
        node->filepath = filepath;
        if (match(T_AS)) {
            match(T_LINES);
            node->asLines = true;
        }
        return node;
    }
    
    // "a random number from X to Y"
    if (match(T_A) || match(T_AN)) {
        if (match(T_RANDOM)) {
            match(T_NUMBER_TYPE); match(T_FROM);
            auto minVal = expression();
            match(T_TO);
            auto maxVal = expression();
            auto node = std::make_shared<RandomNode>();
            node->min = minVal;
            node->max = maxVal;
            return node;
        }
        if (match(T_EMPTY)) {
            match(T_LIST);
            auto node = std::make_shared<ArrayNode>();
            return node;
        }
        if (match(T_LIST)) {
            if (match(T_CONTAINING)) {
                auto arrNode = std::make_shared<ArrayNode>();
                arrNode->elements.push_back(expression());
                while (match(T_AND) || match(T_COMMA)) {
                    arrNode->elements.push_back(expression());
                }
                return arrNode;
            }
            return std::make_shared<ArrayNode>();
        }
    }
    
    // Math functions
    if (match(T_FLOOR)) {
        match(T_OF);
        auto node = std::make_shared<MathOpNode>();
        node->op = "floor"; node->arg1 = expression();
        return node;
    }
    if (match(T_CEILING)) {
        match(T_OF);
        auto node = std::make_shared<MathOpNode>();
        node->op = "ceiling"; node->arg1 = expression();
        return node;
    }
    if (match(T_ROUND)) {
        match(T_OF);
        auto node = std::make_shared<MathOpNode>();
        node->op = "round"; node->arg1 = expression();
        return node;
    }
    if (match(T_ABSOLUTE)) {
        match(T_VALUE); match(T_OF);
        auto node = std::make_shared<MathOpNode>();
        node->op = "abs"; node->arg1 = expression();
        return node;
    }
    if (match(T_SQUARE)) {
        match(T_ROOT); match(T_OF);
        auto node = std::make_shared<MathOpNode>();
        node->op = "sqrt"; node->arg1 = expression();
        return node;
    }
    if (match(T_SINE)) {
        match(T_OF);
        auto node = std::make_shared<MathOpNode>();
        node->op = "sin"; node->arg1 = expression();
        return node;
    }
    if (match(T_COSINE)) {
        match(T_OF);
        auto node = std::make_shared<MathOpNode>();
        node->op = "cos"; node->arg1 = expression();
        return node;
    }
    
    // "current time/year/month/day"
    if (match(T_CURRENT)) {
        if (match(T_TIME)) return std::make_shared<CurrentTimeNode>();
        if (match(T_YEAR)) { auto n = std::make_shared<DateNode>(); n->part = "year"; return n; }
        if (match(T_MONTH)) { auto n = std::make_shared<DateNode>(); n->part = "month"; return n; }
        if (match(T_DAY)) { auto n = std::make_shared<DateNode>(); n->part = "day"; return n; }
        if (match(T_HOUR)) { auto n = std::make_shared<DateNode>(); n->part = "hour"; return n; }
        if (match(T_MINUTE)) { auto n = std::make_shared<DateNode>(); n->part = "minute"; return n; }
    }
    
    // Array/list aggregate functions
    if (match(T_MAXIMUM)) { match(T_OF); auto n = std::make_shared<MaxNode>(); n->arrayName = advance().value; return n; }
    if (match(T_MINIMUM)) { match(T_OF); auto n = std::make_shared<MinNode>(); n->arrayName = advance().value; return n; }
    if (match(T_SUM)) { match(T_OF); auto n = std::make_shared<SumNode>(); n->arrayName = advance().value; return n; }
    if (match(T_AVERAGE)) { match(T_OF); auto n = std::make_shared<AverageNode>(); n->arrayName = advance().value; return n; }
    if (match(T_KEYS)) { match(T_OF); auto n = std::make_shared<KeysNode>(); n->mapName = advance().value; return n; }
    if (match(T_VALUES)) { match(T_OF); auto n = std::make_shared<ValuesNode>(); n->mapName = advance().value; return n; }
    
    // String operations
    if (match(T_UPPERCASE)) { auto n = std::make_shared<UppercaseNode>(); n->text = expression(); return n; }
    if (match(T_LOWERCASE)) { auto n = std::make_shared<LowercaseNode>(); n->text = expression(); return n; }
    if (match(T_TRIM)) { auto n = std::make_shared<TrimNode>(); n->text = expression(); return n; }
    
    // "split X by Y"
    if (match(T_SPLIT)) {
        auto text = expression();
        match(T_BY);
        auto delim = expression();
        auto node = std::make_shared<SplitNode>();
        node->text = text; node->delimiter = delim;
        return node;
    }
    
    // "join X by Y"
    if (match(T_JOIN)) {
        std::string arr = advance().value;
        match(T_BY);
        auto delim = expression();
        auto node = std::make_shared<JoinNode>();
        node->arrayName = arr; node->delimiter = delim;
        return node;
    }
    
    // "substring of X from Y to/with length Z"
    if (match(T_SUBSTRING)) {
        match(T_OF);
        auto text = expression();
        match(T_FROM);
        auto start = expression();
        std::shared_ptr<ASTNode> len = nullptr;
        if (match(T_WITH)) {
            match(T_LENGTH);
            len = expression();
        } else if (match(T_TO)) {
            // "to" means end index, convert to length at runtime? Keep as length node
            len = expression();
        }
        auto node = std::make_shared<SubstringNode>();
        node->text = text;
        node->start = start;
        node->length = len;
        return node;
    }
    
    // HTTP GET as expression: "http get X"
    if (match(T_HTTP)) {
        match(T_GET);
        auto url = expression();
        auto node = std::make_shared<HttpGetNode>();
        node->url = url;
        return node;
    }
    
    // "parse X as json"
    if (match(T_PARSE)) {
        auto json = expression();
        match(T_AS); match(T_JSON);
        auto node = std::make_shared<ParseNode>();
        node->json = json;
        return node;
    }
    
    // "stringify X"
    if (match(T_STRINGIFY)) {
        auto obj = expression();
        auto node = std::make_shared<StringifyNode>();
        node->object = obj;
        return node;
    }
    
    // Parentheses
    if (match(T_LPAREN)) {
        auto expr = expression();
        expect(T_RPAREN, "Expected ')'");
        return expr;
    }
    
    // Array literal [a, b, c]
    if (match(T_LBRACKET)) {
        auto node = std::make_shared<ArrayNode>();
        if (!check(T_RBRACKET)) {
            node->elements.push_back(expression());
            while (match(T_COMMA)) {
                node->elements.push_back(expression());
            }
        }
        expect(T_RBRACKET, "Expected ']'");
        return node;
    }
    
    // Identifier
    if (check(T_IDENTIFIER)) {
        auto node = std::make_shared<IdentifierNode>();
        node->name = advance().value;
        return node;
    }
    
    throw std::runtime_error("Line " + std::to_string(currentLine()) + 
        ": Unexpected token '" + peek().value + "'");
}

// Statement parsing
std::shared_ptr<ASTNode> Parser::statement() {
    // "display ..." or "display the message ..."
    if (match(T_DISPLAY)) {
        match(T_THE); match(T_MESSAGE);
        auto node = std::make_shared<DisplayNode>();
        node->value = expression();
        return node;
    }
    
    // "set the X to Y"
    if (match(T_SET)) {
        match(T_THE);
        
        // Check for "set the X of Y to Z" (field assignment)
        if (check(T_IDENTIFIER) && peekNext().type == T_OF) {
            std::string field = advance().value;
            match(T_OF);
            auto obj = expression();
            match(T_TO);
            auto val = expression();
            auto node = std::make_shared<SetFieldNode>();
            node->object = obj;
            node->field = field;
            node->value = val;
            return node;
        }
        
        std::string name = advance().value;
        match(T_TO);
        auto val = expression();
        auto node = std::make_shared<SetNode>();
        node->name = name;
        node->value = val;
        return node;
    }
    
    // "ask the user ... and store it in X"
    if (match(T_ASK)) {
        match(T_THE); match(T_USER);
        auto prompt = expression();
        expect(T_AND, "Expected 'and store it in'");
        expect(T_STORE, "Expected 'store'");
        match(T_IT); match(T_AS);
        std::string type = "text";
        if (match(T_A)) {
            if (match(T_NUMBER_TYPE)) type = "number";
            else if (match(T_INTEGER)) type = "integer";
        }
        match(T_IN);
        std::string varName = advance().value;
        auto node = std::make_shared<UserInputNode>();
        node->prompt = prompt;
        node->targetVar = varName;
        node->targetType = type;
        return node;
    }
    
    // "if ... then do the following:" or "if ... do:"
    if (match(T_IF)) {
        auto cond = expression();
        match(T_THEN); match(T_DO); match(T_THE); match(T_FOLLOWING);
        match(T_COLON);
        
        auto node = std::make_shared<IfNode>();
        node->condition = cond;
        node->thenBlock = parseBlock(T_END);
        
        if (match(T_END)) {
            if (match(T_IF)) { /* done */ }
            else if (match(T_OTHERWISE) || check(T_ELSE)) {
                // There's an else block
            }
        }
        if (match(T_OTHERWISE) || match(T_ELSE)) {
            match(T_DO); match(T_THE); match(T_FOLLOWING); match(T_COLON);
            node->elseBlock = parseBlock(T_END);
            match(T_END); match(T_IF);
        }
        
        return node;
    }
    
    // "while ... do the following:"
    if (match(T_WHILE)) {
        match(T_THE);
        auto cond = expression();
        match(T_DO); match(T_THE); match(T_FOLLOWING); match(T_COLON);
        
        auto node = std::make_shared<WhileNode>();
        node->condition = cond;
        node->body = parseBlock(T_END);
        expect(T_END, "Expected 'end while'");
        match(T_WHILE);
        return node;
    }
    
    // "repeat N times:"
    if (match(T_REPEAT)) {
        auto count = expression();
        match(T_TIMES); match(T_COLON);
        
        auto node = std::make_shared<RepeatNode>();
        node->count = count;
        node->body = parseBlock(T_END);
        expect(T_END, "Expected 'end repeat'");
        match(T_REPEAT);
        return node;
    }
    
    // "iterate from X through Y using var:"
    if (match(T_ITERATE)) {
        match(T_FROM);
        auto start = expression();
        match(T_THROUGH);
        auto end = expression();
        match(T_USING);
        std::string var = advance().value;
        match(T_COLON);
        
        auto node = std::make_shared<IterateNode>();
        node->variable = var;
        node->start = start;
        node->end = end;
        node->body = parseBlock(T_END);
        expect(T_END, "Expected 'end iterate'");
        match(T_ITERATE);
        return node;
    }
    
    // "for each X in Y do the following:"
    if (match(T_FOR)) {
        match(T_EACH);
        std::string var = advance().value;
        match(T_IN);
        auto collection = expression();
        match(T_DO); match(T_THE); match(T_FOLLOWING); match(T_COLON);
        
        auto node = std::make_shared<ForEachNode>();
        node->variable = var;
        node->collection = collection;
        node->body = parseBlock(T_END);
        expect(T_END, "Expected 'end for'");
        match(T_FOR);
        return node;
    }
    
    // "define function called X with parameters:"
    if (match(T_DEFINE)) {
        match(T_FUNCTION); match(T_CALLED);
        std::string name = advance().value;
        
        std::vector<std::string> params;
        if (match(T_WITH)) {
            if (match(T_PARAMETER) || match(T_PARAMETERS)) {
                params.push_back(advance().value);
                while (match(T_AND) || match(T_COMMA)) {
                    params.push_back(advance().value);
                }
            }
        }
        match(T_COLON);
        
        auto node = std::make_shared<FunctionDefNode>();
        node->name = name;
        node->params = params;
        node->body = parseBlock(T_END);
        expect(T_END, "Expected 'end function'");
        match(T_FUNCTION);
        return node;
    }
    
    // "call the function X with Y"
    if (match(T_CALL)) {
        match(T_THE); match(T_FUNCTION);
        std::string name = advance().value;
        
        std::vector<std::shared_ptr<ASTNode>> args;
        if (match(T_WITH)) {
            args.push_back(expression());
            while (match(T_AND) || match(T_COMMA)) {
                args.push_back(expression());
            }
        }
        
        auto node = std::make_shared<CallNode>();
        node->name = name;
        node->args = args;
        return node;
    }
    
    // "return ..."
    if (match(T_RETURN)) {
        auto node = std::make_shared<ReturnNode>();
        if (!check(T_END) && !isAtEnd()) {
            node->value = expression();
        }
        return node;
    }
    
    // "add X to Y"
    if (match(T_ADD)) {
        auto val = expression();
        match(T_TO);
        std::string arr = advance().value;
        auto node = std::make_shared<AddToArrayNode>();
        node->value = val;
        node->arrayName = arr;
        return node;
    }
    
    // "remove X from Y"
    if (match(T_REMOVE)) {
        // "remove duplicates from X"
        if (match(T_IDENTIFIER) && tokens[pos-1].value == "duplicates") {
            match(T_FROM);
            std::string arr = advance().value;
            auto node = std::make_shared<UniqueNode>();
            node->arrayName = arr;
            return node;
        }
        auto val = expression();
        match(T_FROM);
        std::string arr = advance().value;
        auto node = std::make_shared<RemoveFromArrayNode>();
        node->value = val;
        node->arrayName = arr;
        return node;
    }
    
    // "sort the list X"
    if (match(T_SORT)) {
        match(T_THE); match(T_LIST);
        std::string arr = advance().value;
        auto node = std::make_shared<SortNode>();
        node->arrayName = arr;
        return node;
    }
    
    // "reverse the list X"
    if (match(T_REVERSE)) {
        match(T_THE); match(T_LIST);
        std::string arr = advance().value;
        auto node = std::make_shared<ReverseNode>();
        node->arrayName = arr;
        return node;
    }
    
    // "unique the list X" / "make X unique"
    if (match(T_UNIQUE)) {
        match(T_THE); match(T_LIST);
        std::string arr = advance().value;
        auto node = std::make_shared<UniqueNode>();
        node->arrayName = arr;
        return node;
    }
    
    // "replace X with Y in Z"
    if (match(T_REPLACE)) {
        auto oldStr = expression();
        match(T_WITH);
        auto newStr = expression();
        match(T_IN);
        auto text = expression();
        auto node = std::make_shared<ReplaceNode>();
        node->text = text;
        node->oldStr = oldStr;
        node->newStr = newStr;
        // Since replace is an expression that returns a value, we wrap it as a set
        // Actually, let's just return the expression node - the default handler will eval it
        return node;
    }
    
    // "write X into the file Y"
    if (match(T_WRITE)) {
        auto content = expression();
        match(T_INTO); match(T_THE); match(T_FILE);
        auto filepath = expression();
        auto node = std::make_shared<WriteFileNode>();
        node->content = content;
        node->filepath = filepath;
        return node;
    }
    
    // "append X to the file Y"
    if (match(T_APPEND)) {
        auto content = expression();
        match(T_TO); match(T_THE); match(T_FILE);
        auto filepath = expression();
        auto node = std::make_shared<AppendFileNode>();
        node->content = content;
        node->filepath = filepath;
        return node;
    }
    
    // "delete the file X"
    if (match(T_DELETE)) {
        match(T_THE); match(T_FILE);
        auto filepath = expression();
        auto node = std::make_shared<DeleteFileNode>();
        node->filepath = filepath;
        return node;
    }
    
    // "wait X seconds"
    if (match(T_WAIT)) {
        auto secs = expression();
        match(T_SECONDS);
        auto node = std::make_shared<WaitNode>();
        node->seconds = secs;
        return node;
    }
    
    // "try the following:"
    if (match(T_TRY)) {
        match(T_THE); match(T_FOLLOWING); match(T_COLON);
        auto node = std::make_shared<TryCatchNode>();
        node->tryBlock = parseBlock(T_IF);
        
        match(T_IF); match(T_AN); match(T_ERROR); match(T_OCCURS);
        match(T_THEN); match(T_STORE); match(T_IT); match(T_IN);
        node->errorVar = advance().value;
        match(T_AND); match(T_DO); match(T_THE); match(T_FOLLOWING); match(T_COLON);
        node->catchBlock = parseBlock(T_END);
        expect(T_END, "Expected 'end try'");
        match(T_TRY);
        return node;
    }
    
    // "create channel called X" / "create an object called X"
    if (match(T_CREATE)) {
        if (match(T_CHANNEL)) {
            match(T_CALLED);
            std::string name = advance().value;
            auto node = std::make_shared<CreateChannelNode>();
            node->channelName = name;
            return node;
        }
        // "create an object called X with the following properties:"
        if (match(T_AN) || match(T_A)) {
            match(T_OBJECT); match(T_CALLED);
            std::string name = advance().value;
            match(T_WITH); match(T_THE); match(T_FOLLOWING); match(T_PROPERTIES); match(T_COLON);
            
            auto node = std::make_shared<CreateObjectNode>();
            node->name = name;
            
            while (!check(T_END) && !isAtEnd()) {
                std::string prop = advance().value;
                match(T_IS);
                node->properties[prop] = expression();
            }
            expect(T_END, "Expected 'end object'");
            match(T_OBJECT);
            return node;
        }
    }
    
    // "in the background as X do the following:"
    if (match(T_IN)) {
        match(T_THE); match(T_BACKGROUND); match(T_AS);
        std::string taskName = advance().value;
        match(T_DO); match(T_THE); match(T_FOLLOWING); match(T_COLON);
        
        auto node = std::make_shared<SpawnNode>();
        node->taskName = taskName;
        node->body = parseBlock(T_END);
        expect(T_END, "Expected 'end background task'");
        match(T_BACKGROUND); match(T_TASK);
        return node;
    }
    
    // "send X to channel Y"
    if (match(T_SEND)) {
        // Check if it's "send http ..."
        if (check(T_HTTP)) {
            match(T_HTTP);
            if (match(T_GET)) {
                match(T_REQUEST); match(T_TO);
                auto url = expression();
                auto node = std::make_shared<HttpGetNode>();
                node->url = url;
                return node;
            }
            // HTTP POST: "send http post request to X with body Y"
            match(T_REQUEST); match(T_TO);
            auto url = expression();
            std::shared_ptr<ASTNode> body = nullptr;
            if (match(T_WITH)) {
                advance(); // skip "body"
                body = expression();
            }
            auto node = std::make_shared<HttpPostNode>();
            node->url = url;
            node->body = body;
            return node;
        }
        auto msg = expression();
        match(T_TO); match(T_CHANNEL);
        std::string ch = advance().value;
        auto node = std::make_shared<SendToChannelNode>();
        node->message = msg;
        node->channelName = ch;
        return node;
    }
    
    // "receive from channel X and store it in Y"
    if (match(T_RECEIVE)) {
        match(T_FROM); match(T_CHANNEL);
        std::string ch = advance().value;
        match(T_AND); match(T_STORE); match(T_IT); match(T_IN);
        std::string var = advance().value;
        auto node = std::make_shared<ReceiveFromChannelNode>();
        node->channelName = ch;
        node->targetVar = var;
        return node;
    }
    
    // "import X"
    if (match(T_IMPORT)) {
        auto filepath = expression();
        auto node = std::make_shared<ImportNode>();
        node->filepath = filepath;
        return node;
    }
    
    // "run the command X" / "run command X and capture output in Y"
    if (match(T_RUN)) {
        match(T_THE);
        // Skip "command" if it appears as an identifier
        if (check(T_IDENTIFIER) && peek().value == "command") advance();
        auto cmd = expression();
        auto node = std::make_shared<RunCommandNode>();
        node->command = cmd;
        if (match(T_AND)) {
            if (match(T_CAPTURE)) {
                match(T_OUTPUT); match(T_IN);
                node->captureOutput = true;
                node->targetVar = advance().value;
            } else if (match(T_STORE)) {
                match(T_IT); match(T_IN);
                node->captureOutput = true;
                node->targetVar = advance().value;
            }
        }
        return node;
    }
    
    // "load native X as Y"
    if (match(T_LOAD)) {
        match(T_NATIVE);
        auto lib = expression();
        match(T_AS);
        std::string alias = advance().value;
        auto node = std::make_shared<LoadNativeNode>();
        node->lib = lib;
        node->alias = alias;
        return node;
    }
    
    // "open gui window with name ..."
    if (match(T_OPEN)) {
        match(T_GUI); match(T_WINDOW); match(T_WITH); 
        match(T_NAME); match(T_LABEL);
        auto title = expression();
        match(T_AND); match(T_SIZE);
        auto w = expression();
        match(T_BY);
        auto h = expression();
        match(T_COLON);
        
        auto node = std::make_shared<GUIWindowNode>();
        node->title = title;
        node->width = w;
        node->height = h;
        node->body = parseBlock(T_END);
        expect(T_END, "Expected 'end gui window'");
        match(T_GUI); match(T_WINDOW);
        return node;
    }
    
    // "draw text/rectangle/circle..."
    if (match(T_DRAW)) {
        if (match(T_TEXT)) {
            auto text = expression();
            match(T_AT); match(T_POSITION);
            auto x = expression();
            match(T_BY);
            auto y = expression();
            
            auto node = std::make_shared<GUIDrawTextNode>();
            node->text = text;
            node->x = x;
            node->y = y;
            
            if (match(T_WITH)) {
                if (match(T_COLOR)) {
                    if (match(T_RED)) node->color = "red";
                    else if (match(T_GREEN)) node->color = "green";
                    else if (match(T_BLUE)) node->color = "blue";
                    else if (match(T_WHITE)) node->color = "white";
                    else if (match(T_BLACK)) node->color = "black";
                    else if (match(T_YELLOW)) node->color = "yellow";
                }
            }
            return node;
        }
        if (match(T_RECTANGLE)) {
            match(T_AT); match(T_POSITION);
            auto x = expression();
            match(T_BY);
            auto y = expression();
            match(T_WITH); match(T_SIZE);
            auto w = expression();
            match(T_BY);
            auto h = expression();
            
            auto node = std::make_shared<GUIDrawRectNode>();
            node->x = x; node->y = y;
            node->width = w; node->height = h;
            
            if (match(T_AND)) {
                match(T_COLOR);
                if (match(T_RED)) node->color = "red";
                else if (match(T_GREEN)) node->color = "green";
                else if (match(T_BLUE)) node->color = "blue";
            }
            return node;
        }
        if (match(T_CIRCLE)) {
            match(T_AT); match(T_POSITION);
            auto x = expression();
            match(T_BY);
            auto y = expression();
            match(T_WITH); match(T_RADIUS);
            auto r = expression();
            
            auto node = std::make_shared<GUIDrawCircleNode>();
            node->x = x; node->y = y;
            node->radius = r;
            
            if (match(T_AND)) {
                match(T_COLOR);
                if (match(T_RED)) node->color = "red";
                else if (match(T_GREEN)) node->color = "green";
                else if (match(T_BLUE)) node->color = "blue";
            }
            return node;
        }
    }
    
    // "break", "continue", "exit"
    if (match(T_BREAK)) return std::make_shared<BreakNode>();
    if (match(T_CONTINUE)) return std::make_shared<ContinueNode>();
    if (match(T_EXIT)) return std::make_shared<ExitNode>();
    
    // "stop task X"
    if (match(T_STOP)) {
        match(T_TASK);
        std::string name = advance().value;
        auto node = std::make_shared<StopTaskNode>();
        node->taskName = name;
        return node;
    }
    
    // "trace"
    if (match(T_TRACE)) return std::make_shared<TraceNode>();
    
    // "close window"
    if (match(T_CLOSE)) {
        match(T_WINDOW);
        return std::make_shared<GUICloseNode>();
    }
    
    // Default: try to parse as expression
    return expression();
}
