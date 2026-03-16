// nibyo v1.0 beta - natural english programming language
// ast.h - abstract syntax tree node definitions with fast nodetype dispatch
#pragma once

#include "Common.h"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

// nodetype enum for high-performance switch-based dispatch
enum class NodeType {
    // literals
    Number, Text, Bool, Identifier, None,
    // expressions
    BinaryOp, UnaryOp, Array, Map, Range,
    // variables
    Set, SetField, GetField,
    // i/o
    Display, UserInput,
    // control flow
    If, While, Iterate, ForEach, Repeat,
    Break, Continue, Exit,
    // functions
    FunctionDef, Call, CallFunc, Lambda, Return,
    // array/list
    AddToArray, RemoveFromArray, GetValueAt, GetLast, GetFirst,
    LengthOf, GetIndex, Sort, Reverse, Unique,
    Max, Min, Sum, Average, Keys, Values,
    // strings
    Split, Join, JoinedWith, Replace,
    Uppercase, Lowercase, Trim,
    Substring, Contains, StartsWith, EndsWith,
    // math
    MathOp, Random,
    // files
    ReadFile, WriteFile, AppendFile, FileExists, DeleteFile,
    // time
    CurrentTime, Date, Wait,
    // error handling
    TryCatch,
    // http/json
    HttpGet, HttpPost, Parse, Stringify,
    // concurrency
    Spawn, StopTask, CreateChannel, SendToChannel, ReceiveFromChannel, Await,
    // gui
    GUIWindow, GUIDrawText, GUIDrawRect, GUIDrawCircle, GUIButton, GUITextInput, GUIClose,
    // misc
    Import, LoadNative, RunCommand, GetEnvVar, Trace, CreateObject
};

// base ast node with type tag for fast dispatch
struct ASTNode {
    const NodeType nodeType;
    int line = 0;
    explicit ASTNode(NodeType t) : nodeType(t) {}
    virtual ~ASTNode() = default;
};

// literal nodes

struct NumberNode : ASTNode {
    double value;
    NumberNode() : ASTNode(NodeType::Number) {}
};

struct TextNode : ASTNode {
    std::string value;
    TextNode() : ASTNode(NodeType::Text) {}
};

struct BoolNode : ASTNode {
    bool value;
    BoolNode() : ASTNode(NodeType::Bool) {}
};

struct IdentifierNode : ASTNode {
    std::string name;
    IdentifierNode() : ASTNode(NodeType::Identifier) {}
};

struct NoneNode : ASTNode {
    NoneNode() : ASTNode(NodeType::None) {}
};

// expression nodes

struct BinaryOpNode : ASTNode {
    std::shared_ptr<ASTNode> left;
    std::shared_ptr<ASTNode> right;
    std::string op;
    BinaryOpNode() : ASTNode(NodeType::BinaryOp) {}
};

struct UnaryOpNode : ASTNode {
    std::shared_ptr<ASTNode> operand;
    std::string op;
    UnaryOpNode() : ASTNode(NodeType::UnaryOp) {}
};

struct ArrayNode : ASTNode {
    std::vector<std::shared_ptr<ASTNode>> elements;
    ArrayNode() : ASTNode(NodeType::Array) {}
};

struct MapNode : ASTNode {
    std::unordered_map<std::string, std::shared_ptr<ASTNode>> pairs;
    MapNode() : ASTNode(NodeType::Map) {}
};

struct RangeNode : ASTNode {
    std::shared_ptr<ASTNode> start;
    std::shared_ptr<ASTNode> end;
    RangeNode() : ASTNode(NodeType::Range) {}
};

// variable nodes

struct SetNode : ASTNode {
    std::string name;
    std::shared_ptr<ASTNode> value;
    SetNode() : ASTNode(NodeType::Set) {}
};

struct SetFieldNode : ASTNode {
    std::shared_ptr<ASTNode> object;
    std::string field;
    std::shared_ptr<ASTNode> value;
    SetFieldNode() : ASTNode(NodeType::SetField) {}
};

struct GetFieldNode : ASTNode {
    std::shared_ptr<ASTNode> object;
    std::string field;
    GetFieldNode() : ASTNode(NodeType::GetField) {}
};

// i/o nodes

struct DisplayNode : ASTNode {
    std::shared_ptr<ASTNode> value;
    DisplayNode() : ASTNode(NodeType::Display) {}
};

struct UserInputNode : ASTNode {
    std::shared_ptr<ASTNode> prompt;
    std::string targetVar;
    std::string targetType = "text";
    UserInputNode() : ASTNode(NodeType::UserInput) {}
};

// control flow nodes

struct IfNode : ASTNode {
    std::shared_ptr<ASTNode> condition;
    std::vector<std::shared_ptr<ASTNode>> thenBlock;
    std::vector<std::shared_ptr<ASTNode>> elseBlock;
    IfNode() : ASTNode(NodeType::If) {}
};

struct WhileNode : ASTNode {
    std::shared_ptr<ASTNode> condition;
    std::vector<std::shared_ptr<ASTNode>> body;
    WhileNode() : ASTNode(NodeType::While) {}
};

struct IterateNode : ASTNode {
    std::string variable;
    std::shared_ptr<ASTNode> start;
    std::shared_ptr<ASTNode> end;
    std::vector<std::shared_ptr<ASTNode>> body;
    IterateNode() : ASTNode(NodeType::Iterate) {}
};

struct ForEachNode : ASTNode {
    std::string variable;
    std::shared_ptr<ASTNode> collection;
    std::vector<std::shared_ptr<ASTNode>> body;
    ForEachNode() : ASTNode(NodeType::ForEach) {}
};

struct RepeatNode : ASTNode {
    std::shared_ptr<ASTNode> count;
    std::vector<std::shared_ptr<ASTNode>> body;
    RepeatNode() : ASTNode(NodeType::Repeat) {}
};

struct BreakNode : ASTNode { BreakNode() : ASTNode(NodeType::Break) {} };
struct ContinueNode : ASTNode { ContinueNode() : ASTNode(NodeType::Continue) {} };
struct ExitNode : ASTNode { ExitNode() : ASTNode(NodeType::Exit) {} };

// function nodes

struct FunctionDefNode : ASTNode {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::shared_ptr<ASTNode>> body;
    FunctionDefNode() : ASTNode(NodeType::FunctionDef) {}
};

struct CallNode : ASTNode {
    std::string name;
    std::vector<std::shared_ptr<ASTNode>> args;
    CallNode() : ASTNode(NodeType::Call) {}
};

struct CallFuncNode : ASTNode {
    std::shared_ptr<ASTNode> func;
    std::vector<std::shared_ptr<ASTNode>> args;
    CallFuncNode() : ASTNode(NodeType::CallFunc) {}
};

struct LambdaNode : ASTNode {
    std::vector<std::string> params;
    std::vector<std::shared_ptr<ASTNode>> body;
    LambdaNode() : ASTNode(NodeType::Lambda) {}
};

struct ReturnNode : ASTNode {
    std::shared_ptr<ASTNode> value;
    ReturnNode() : ASTNode(NodeType::Return) {}
};

// array/list nodes

struct AddToArrayNode : ASTNode {
    std::shared_ptr<ASTNode> value;
    std::string arrayName;
    AddToArrayNode() : ASTNode(NodeType::AddToArray) {}
};

struct RemoveFromArrayNode : ASTNode {
    std::shared_ptr<ASTNode> value;
    std::string arrayName;
    RemoveFromArrayNode() : ASTNode(NodeType::RemoveFromArray) {}
};

struct GetValueAtNode : ASTNode {
    std::shared_ptr<ASTNode> index;
    std::string arrayName;
    GetValueAtNode() : ASTNode(NodeType::GetValueAt) {}
};

struct GetLastNode : ASTNode {
    std::string arrayName;
    GetLastNode() : ASTNode(NodeType::GetLast) {}
};

struct GetFirstNode : ASTNode {
    std::string arrayName;
    GetFirstNode() : ASTNode(NodeType::GetFirst) {}
};

struct LengthOfNode : ASTNode {
    std::shared_ptr<ASTNode> target;
    LengthOfNode() : ASTNode(NodeType::LengthOf) {}
};

struct GetIndexNode : ASTNode {
    std::shared_ptr<ASTNode> value;
    std::string arrayName;
    GetIndexNode() : ASTNode(NodeType::GetIndex) {}
};

struct SortNode : ASTNode {
    std::string arrayName;
    SortNode() : ASTNode(NodeType::Sort) {}
};

struct ReverseNode : ASTNode {
    std::string arrayName;
    ReverseNode() : ASTNode(NodeType::Reverse) {}
};

struct UniqueNode : ASTNode {
    std::string arrayName;
    UniqueNode() : ASTNode(NodeType::Unique) {}
};

struct MaxNode : ASTNode { std::string arrayName; MaxNode() : ASTNode(NodeType::Max) {} };
struct MinNode : ASTNode { std::string arrayName; MinNode() : ASTNode(NodeType::Min) {} };
struct SumNode : ASTNode { std::string arrayName; SumNode() : ASTNode(NodeType::Sum) {} };
struct AverageNode : ASTNode { std::string arrayName; AverageNode() : ASTNode(NodeType::Average) {} };
struct KeysNode : ASTNode { std::string mapName; KeysNode() : ASTNode(NodeType::Keys) {} };
struct ValuesNode : ASTNode { std::string mapName; ValuesNode() : ASTNode(NodeType::Values) {} };

// string nodes

struct SplitNode : ASTNode {
    std::shared_ptr<ASTNode> text;
    std::shared_ptr<ASTNode> delimiter;
    SplitNode() : ASTNode(NodeType::Split) {}
};

struct JoinNode : ASTNode {
    std::string arrayName;
    std::shared_ptr<ASTNode> delimiter;
    JoinNode() : ASTNode(NodeType::Join) {}
};

struct JoinedWithNode : ASTNode {
    std::shared_ptr<ASTNode> left;
    std::shared_ptr<ASTNode> right;
    JoinedWithNode() : ASTNode(NodeType::JoinedWith) {}
};

struct ReplaceNode : ASTNode {
    std::shared_ptr<ASTNode> text;
    std::shared_ptr<ASTNode> oldStr;
    std::shared_ptr<ASTNode> newStr;
    ReplaceNode() : ASTNode(NodeType::Replace) {}
};

struct UppercaseNode : ASTNode { std::shared_ptr<ASTNode> text; UppercaseNode() : ASTNode(NodeType::Uppercase) {} };
struct LowercaseNode : ASTNode { std::shared_ptr<ASTNode> text; LowercaseNode() : ASTNode(NodeType::Lowercase) {} };
struct TrimNode : ASTNode { std::shared_ptr<ASTNode> text; TrimNode() : ASTNode(NodeType::Trim) {} };

struct SubstringNode : ASTNode {
    std::shared_ptr<ASTNode> text;
    std::shared_ptr<ASTNode> start;
    std::shared_ptr<ASTNode> length;
    SubstringNode() : ASTNode(NodeType::Substring) {}
};

struct ContainsNode : ASTNode {
    std::shared_ptr<ASTNode> text;
    std::shared_ptr<ASTNode> search;
    ContainsNode() : ASTNode(NodeType::Contains) {}
};

struct StartsWithNode : ASTNode {
    std::shared_ptr<ASTNode> text;
    std::shared_ptr<ASTNode> prefix;
    StartsWithNode() : ASTNode(NodeType::StartsWith) {}
};

struct EndsWithNode : ASTNode {
    std::shared_ptr<ASTNode> text;
    std::shared_ptr<ASTNode> suffix;
    EndsWithNode() : ASTNode(NodeType::EndsWith) {}
};

// math nodes

struct MathOpNode : ASTNode {
    std::string op;
    std::shared_ptr<ASTNode> arg1;
    std::shared_ptr<ASTNode> arg2;
    MathOpNode() : ASTNode(NodeType::MathOp) {}
};

struct RandomNode : ASTNode {
    std::shared_ptr<ASTNode> min;
    std::shared_ptr<ASTNode> max;
    RandomNode() : ASTNode(NodeType::Random) {}
};

// file nodes

struct ReadFileNode : ASTNode {
    std::shared_ptr<ASTNode> filepath;
    bool asLines = false;
    ReadFileNode() : ASTNode(NodeType::ReadFile) {}
};

struct WriteFileNode : ASTNode {
    std::shared_ptr<ASTNode> filepath;
    std::shared_ptr<ASTNode> content;
    WriteFileNode() : ASTNode(NodeType::WriteFile) {}
};

struct AppendFileNode : ASTNode {
    std::shared_ptr<ASTNode> filepath;
    std::shared_ptr<ASTNode> content;
    AppendFileNode() : ASTNode(NodeType::AppendFile) {}
};

struct FileExistsNode : ASTNode {
    std::shared_ptr<ASTNode> filepath;
    FileExistsNode() : ASTNode(NodeType::FileExists) {}
};

struct DeleteFileNode : ASTNode {
    std::shared_ptr<ASTNode> filepath;
    DeleteFileNode() : ASTNode(NodeType::DeleteFile) {}
};

// time nodes

struct CurrentTimeNode : ASTNode { CurrentTimeNode() : ASTNode(NodeType::CurrentTime) {} };

struct DateNode : ASTNode {
    std::string part;
    DateNode() : ASTNode(NodeType::Date) {}
};

struct WaitNode : ASTNode {
    std::shared_ptr<ASTNode> seconds;
    WaitNode() : ASTNode(NodeType::Wait) {}
};

// error handling nodes

struct TryCatchNode : ASTNode {
    std::vector<std::shared_ptr<ASTNode>> tryBlock;
    std::vector<std::shared_ptr<ASTNode>> catchBlock;
    std::string errorVar;
    TryCatchNode() : ASTNode(NodeType::TryCatch) {}
};

// http/json nodes

struct HttpGetNode : ASTNode {
    std::shared_ptr<ASTNode> url;
    HttpGetNode() : ASTNode(NodeType::HttpGet) {}
};

struct HttpPostNode : ASTNode {
    std::shared_ptr<ASTNode> url;
    std::shared_ptr<ASTNode> body;
    HttpPostNode() : ASTNode(NodeType::HttpPost) {}
};

struct ParseNode : ASTNode {
    std::shared_ptr<ASTNode> json;
    ParseNode() : ASTNode(NodeType::Parse) {}
};

struct StringifyNode : ASTNode {
    std::shared_ptr<ASTNode> object;
    StringifyNode() : ASTNode(NodeType::Stringify) {}
};

// concurrency nodes

struct SpawnNode : ASTNode {
    std::vector<std::shared_ptr<ASTNode>> body;
    std::string taskName;
    SpawnNode() : ASTNode(NodeType::Spawn) {}
};

struct StopTaskNode : ASTNode {
    std::string taskName;
    StopTaskNode() : ASTNode(NodeType::StopTask) {}
};

struct CreateChannelNode : ASTNode {
    std::string channelName;
    CreateChannelNode() : ASTNode(NodeType::CreateChannel) {}
};

struct SendToChannelNode : ASTNode {
    std::shared_ptr<ASTNode> message;
    std::string channelName;
    SendToChannelNode() : ASTNode(NodeType::SendToChannel) {}
};

struct ReceiveFromChannelNode : ASTNode {
    std::string channelName;
    std::string targetVar;
    std::shared_ptr<ASTNode> timeout;
    ReceiveFromChannelNode() : ASTNode(NodeType::ReceiveFromChannel) {}
};

struct AwaitNode : ASTNode {
    std::shared_ptr<ASTNode> task;
    AwaitNode() : ASTNode(NodeType::Await) {}
};

// gui nodes

struct GUIWindowNode : ASTNode {
    std::shared_ptr<ASTNode> title;
    std::shared_ptr<ASTNode> width;
    std::shared_ptr<ASTNode> height;
    std::vector<std::shared_ptr<ASTNode>> body;
    GUIWindowNode() : ASTNode(NodeType::GUIWindow) {}
};

struct GUIDrawTextNode : ASTNode {
    std::shared_ptr<ASTNode> text;
    std::shared_ptr<ASTNode> x;
    std::shared_ptr<ASTNode> y;
    std::string color = "black";
    std::shared_ptr<ASTNode> fontSize;
    GUIDrawTextNode() : ASTNode(NodeType::GUIDrawText) {}
};

struct GUIDrawRectNode : ASTNode {
    std::shared_ptr<ASTNode> x;
    std::shared_ptr<ASTNode> y;
    std::shared_ptr<ASTNode> width;
    std::shared_ptr<ASTNode> height;
    std::string color = "white";
    bool filled = true;
    GUIDrawRectNode() : ASTNode(NodeType::GUIDrawRect) {}
};

struct GUIDrawCircleNode : ASTNode {
    std::shared_ptr<ASTNode> x;
    std::shared_ptr<ASTNode> y;
    std::shared_ptr<ASTNode> radius;
    std::string color = "white";
    bool filled = true;
    GUIDrawCircleNode() : ASTNode(NodeType::GUIDrawCircle) {}
};

struct GUIButtonNode : ASTNode {
    std::string name;
    std::shared_ptr<ASTNode> label;
    std::shared_ptr<ASTNode> x;
    std::shared_ptr<ASTNode> y;
    std::vector<std::shared_ptr<ASTNode>> onClick;
    GUIButtonNode() : ASTNode(NodeType::GUIButton) {}
};

struct GUITextInputNode : ASTNode {
    std::string name;
    std::shared_ptr<ASTNode> placeholder;
    std::shared_ptr<ASTNode> x;
    std::shared_ptr<ASTNode> y;
    std::shared_ptr<ASTNode> width;
    GUITextInputNode() : ASTNode(NodeType::GUITextInput) {}
};

struct GUICloseNode : ASTNode { GUICloseNode() : ASTNode(NodeType::GUIClose) {} };

// miscellaneous nodes

struct ImportNode : ASTNode {
    std::shared_ptr<ASTNode> filepath;
    ImportNode() : ASTNode(NodeType::Import) {}
};

struct LoadNativeNode : ASTNode {
    std::shared_ptr<ASTNode> lib;
    std::string alias;
    LoadNativeNode() : ASTNode(NodeType::LoadNative) {}
};

struct RunCommandNode : ASTNode {
    std::shared_ptr<ASTNode> command;
    bool captureOutput = false;
    std::string targetVar;
    RunCommandNode() : ASTNode(NodeType::RunCommand) {}
};

struct GetEnvVarNode : ASTNode {
    std::shared_ptr<ASTNode> varName;
    GetEnvVarNode() : ASTNode(NodeType::GetEnvVar) {}
};

struct TraceNode : ASTNode { TraceNode() : ASTNode(NodeType::Trace) {} };

struct CreateObjectNode : ASTNode {
    std::string name;
    std::unordered_map<std::string, std::shared_ptr<ASTNode>> properties;
    CreateObjectNode() : ASTNode(NodeType::CreateObject) {}
};
