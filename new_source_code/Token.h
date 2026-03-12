// Nexo v5.0 - Natural English Programming Language
// Token.h - Token types and structures
#pragma once

#include <string>

// Token types - all English keywords for natural language syntax
enum TokenType {
    // End of file
    T_EOF,
    
    // Literals
    T_NUMBER,           // 123, 3.14
    T_STRING,           // "hello"
    T_IDENTIFIER,       // variable names
    
    // Boolean literals
    T_TRUE,             // true
    T_FALSE,            // false
    
    // Core keywords
    T_SET,              // set
    T_THE,              // the
    T_TO,               // to
    T_AN,               // an
    T_A,                // a
    T_IS,               // is
    T_ARE,              // are
    T_IT,               // it
    T_IN,               // in
    T_OF,               // of
    T_AT,               // at
    T_BY,               // by
    T_WITH,             // with
    T_AND,              // and
    T_OR,               // or
    T_NOT,              // not
    T_AS,               // as
    T_FOR,              // for
    T_FROM,             // from
    T_THROUGH,          // through
    T_USING,            // using
    T_CALLED,           // called
    T_NAMED,            // named
    
    // Display and input
    T_DISPLAY,          // display
    T_MESSAGE,          // message
    T_ASK,              // ask
    T_STORE,            // store
    T_NAME,             // name
    T_USER,             // user
    T_PROMPT,           // prompt
    
    // Conditionals
    T_IF,               // if
    T_THEN,             // then
    T_ELSE,             // else
    T_OTHERWISE,        // otherwise
    T_DO,               // do
    T_FOLLOWING,        // following
    T_END,              // end
    
    // Loops
    T_WHILE,            // while
    T_REPEAT,           // repeat
    T_TIMES,            // times
    T_ITERATE,          // iterate
    T_EACH,             // each
    T_LOOP,             // loop
    T_BREAK,            // break
    T_CONTINUE,         // continue
    T_EXIT,             // exit
    
    // Functions
    T_DEFINE,           // define
    T_FUNCTION,         // function
    T_PARAMETER,        // parameter
    T_PARAMETERS,       // parameters
    T_CALL,             // call
    T_RETURN,           // return
    
    // Comparisons
    T_EQUAL,            // equal
    T_SAME,             // same
    T_EXACTLY,          // exactly
    T_GREATER,          // greater
    T_LESS,             // less
    T_MORE,             // more
    T_FEWER,            // fewer
    T_THAN,             // than
    T_LEAST,            // least
    T_MOST,             // most
    T_DIFFERENT,        // different
    
    // Math keywords
    T_PLUS,             // plus, +
    T_MINUS,            // minus, -
    T_MULTIPLY,         // multiplied, times (as operator)
    T_DIVIDE,           // divided
    T_MODULO,           // modulo, remainder
    T_FLOOR,            // floor
    T_CEILING,          // ceiling
    T_ROUND,            // round
    T_ABSOLUTE,         // absolute
    T_POWER,            // power
    T_RAISED,           // raised
    T_SQUARE,           // square
    T_ROOT,             // root
    T_SINE,             // sine
    T_COSINE,           // cosine
    T_TANGENT,          // tangent
    T_RANDOM,           // random
    T_NEGATIVE,         // negative
    T_VALUE,            // value
    
    // Arrays/Lists
    T_LIST,             // list
    T_ARRAY,            // array
    T_EMPTY,            // empty
    T_CONTAINING,       // containing
    T_ADD,              // add
    T_REMOVE,           // remove
    T_GET,              // get
    T_FIRST,            // first
    T_SECOND,           // second
    T_THIRD,            // third
    T_LAST,             // last
    T_ITEM,             // item
    T_LENGTH,           // length
    T_SORT,             // sort
    T_REVERSE,          // reverse
    T_UNIQUE,           // unique
    T_JOIN,             // join
    T_JOINED,           // joined
    T_SPLIT,            // split
    T_MAXIMUM,          // maximum
    T_MINIMUM,          // minimum
    T_SUM,              // sum
    T_AVERAGE,          // average
    
    // Objects/Maps
    T_OBJECT,           // object
    T_CREATE,           // create
    T_NEW,              // new
    T_PROPERTIES,       // properties
    T_PROPERTY,         // property
    T_KEYS,             // keys
    T_VALUES,           // values
    
    // Strings
    T_TEXT,             // text
    T_UPPERCASE,        // uppercase
    T_LOWERCASE,        // lowercase
    T_TRIM,             // trim
    T_SUBSTRING,        // substring
    T_REPLACE,          // replace
    T_CONTAINS,         // contains
    T_STARTS,           // starts
    T_ENDS,             // ends
    
    // Files
    T_FILE,             // file
    T_READ,             // read
    T_WRITE,            // write
    T_APPEND,           // append
    T_DELETE,           // delete
    T_EXISTS,           // exists
    T_INTO,             // into
    T_LINES,            // lines
    
    // Time and date
    T_CURRENT,          // current
    T_TIME,             // time
    T_YEAR,             // year
    T_MONTH,            // month
    T_DAY,              // day
    T_HOUR,             // hour
    T_MINUTE,           // minute
    T_SECONDS,          // seconds
    T_WAIT,             // wait
    
    // Error handling
    T_TRY,              // try
    T_ERROR,            // error
    T_OCCURS,           // occurs
    T_AN_ERROR,         // special for "an error"
    
    // HTTP and JSON
    T_HTTP,             // http
    T_SEND,             // send
    T_REQUEST,          // request
    T_RESPONSE,         // response
    T_PARSE,            // parse
    T_STRINGIFY,        // stringify
    T_JSON,             // json
    
    // Concurrency
    T_BACKGROUND,       // background
    T_TASK,             // task
    T_CHANNEL,          // channel
    T_RECEIVE,          // receive
    T_STOP,             // stop
    T_FINISH,           // finish
    T_AWAIT,            // await
    
    // GUI keywords
    T_OPEN,             // open
    T_CLOSE,            // close
    T_GUI,              // gui
    T_WINDOW,           // window
    T_SIZE,             // size
    T_DRAW,             // draw
    T_BUTTON,           // button
    T_LABEL,            // label
    T_INPUT,            // input
    T_PLACEHOLDER,      // placeholder
    T_POSITION,         // position
    T_RECTANGLE,        // rectangle
    T_CIRCLE,           // circle
    T_RADIUS,           // radius
    T_COLOR,            // color
    T_WHEN,             // when
    T_CLICKED,          // clicked
    T_CODE,             // code
    
    // Colors
    T_RED,              // red
    T_GREEN,            // green
    T_BLUE,             // blue
    T_WHITE,            // white
    T_BLACK,            // black
    T_YELLOW,           // yellow
    T_ORANGE,           // orange
    T_PURPLE,           // purple
    T_PINK,             // pink
    T_GRAY,             // gray
    
    // Modules
    T_LOAD,             // load
    T_NATIVE,           // native
    T_IMPORT,           // import
    
    // Miscellaneous
    T_ENVIRONMENT,      // environment
    T_VARIABLE,         // variable
    T_RUN,              // run
    T_CAPTURE,          // capture
    T_OUTPUT,           // output
    T_TRACE,            // trace
    T_RANGE,            // range
    T_NUMBER_TYPE,      // number (as type)
    T_INTEGER,          // integer
    T_BOOLEAN,          // boolean
    T_THERE,            // there
    
    // Comments
    T_IGNORE,           // ignore
    T_THIS,             // this
    T_LINE,             // line
    T_ALL,              // all
    
    // Punctuation (minimal, for parsing)
    T_COLON,            // :
    T_COMMA,            // ,
    T_DOT,              // .
    T_LPAREN,           // (
    T_RPAREN,           // )
    T_LBRACKET,         // [
    T_RBRACKET,         // ]
};

// Token structure with line information for error reporting
struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;
    
    Token() : type(T_EOF), value(""), line(0), column(0) {}
    Token(TokenType t, const std::string& v, int l, int c = 0) 
        : type(t), value(v), line(l), column(c) {}
};
