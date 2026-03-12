// Nexo v5.0 - Natural English Programming Language
// main.cpp - Entry point
// Compile: g++ -std=c++17 -O3 *.cpp -o nexo.exe -static -pthread
// Usage: .\nexo.exe .\program.nx

#include "Common.h"
#include "Lexer.h"
#include "Parser.h"
#include "Interpreter.h"
#include <fstream>
#include <sstream>
#include <iostream>

int main(int argc, char** argv) {
    // Usage: nexo.exe file.nx
    if (argc != 2) {
        std::cerr << "Nexo v5.0 - Natural English Programming Language" << std::endl;
        std::cerr << "Usage: " << argv[0] << " <program.nx>" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Example:" << std::endl;
        std::cerr << "  " << argv[0] << " hello.nx" << std::endl;
        return 1;
    }
    
    // Read the source file
    std::string filepath = argv[1];
    std::ifstream file(filepath);
    if (!file) {
        std::cerr << "Error: Cannot open file '" << filepath << "'" << std::endl;
        return 1;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string code = buffer.str();
    
    try {
        // Lexical analysis
        Lexer lexer(code);
        auto tokens = lexer.tokenize();
        
        // Parsing
        Parser parser(tokens);
        auto program = parser.parse();
        
        // Execution
        Interpreter interpreter;
        interpreter.run(program);
        
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
