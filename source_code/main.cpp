// nibyo v1.0 beta - natural english programming language
// main.cpp - entry point
// compile: g++ -std=c++17 -o3 *.cpp -o nibyo.exe -static -pthread
// usage: .\nibyo.exe .\program.nb

#include "Common.h"
#include "Lexer.h"
#include "Parser.h"
#include "Interpreter.h"
#include <fstream>
#include <sstream>
#include <iostream>

int main(int argc, char** argv) {
    // usage: nibyo.exe file.nb
    if (argc != 2) {
        std::cerr << "nibyo v1.0 beta - natural english programming language" << std::endl;
        std::cerr << "Usage: " << argv[0] << " <program.nb>" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Example:" << std::endl;
        std::cerr << "  " << argv[0] << " hello.nb" << std::endl;
        return 1;
    }
    
    // read the source file
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
        // lexical analysis
        Lexer lexer(code);
        auto tokens = lexer.tokenize();
        
        // parsing
        Parser parser(tokens);
        auto program = parser.parse();
        
        // execution
        Interpreter interpreter;
        interpreter.run(program);
        
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
