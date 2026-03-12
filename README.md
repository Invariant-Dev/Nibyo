<div align="center">
  <img src="https://via.placeholder.com/150/000000/FFFFFF?text=Nexo+v1.0+beta" alt="Nexo Logo" width="150" height="150"/>
  
  # Nexo v1.0 beta
  **The Programming Language That Speaks English.**
  
  [![License: GPL-3.0](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
  [![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
  [![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)]()
  [![Engine](https://img.shields.io/badge/Engine-v1.0_beta-success.svg)]()
</div>

---

## 👋 Welcome to Nexo!

Nexo is a personal side-project I have been working on for around a year. It's an experimental programming language designed to bridge the gap between human thought and machine execution. If you can describe it in English, you can code it in Nexo! 

While this is currently the **v1.0 beta** release, I have big plans to expand the language, add new features, and continue refining the engine in the future.

Gone are the days of cryptic syntax, curly braces, and semicolons. The v1.0 beta engine provides **fast execution**, **robust concurrency**, and **system integration**, all wrapped in a syntax that reads like a book.

---

## ⚡ Why Nexo?

- 🧠 **Zero Learning Curve**: Reads almost exactly like pseudocode.
- 🚀 **Blazing Fast**: The v1.0 beta engine features optimized AST dispatch and memory pooling.
- 🧵 **Built-in Concurrency**: Seamlessly spawn background tasks and communicate via channels.
- 🌐 **Native Web Support**: First-class JSON parsing and HTTP GET/POST capabilities.
- 💻 **System Mastery**: Run shell commands and access environment variables effortlessly.
- 🔒 **Safe & Robust**: Comprehensive error handling using `try/catch` and safe type casting.

---

## 🚀 Installation

Getting Nexo up and running is incredibly simple. All you need is a modern C++ compiler (C++17 or higher).

### Prerequisites
- **Windows**: [MinGW-w64](https://www.mingw-w64.org/) (provides `g++`)
- **macOS**: Xcode Command Line Tools (`xcode-select --install`)
- **Linux**: GCC (`sudo apt install g++`)

### 📦 Prebuilt Binaries
Don't want to compile from source? You can download the latest prebuilt binaries for Windows, Linux, and macOS directly from the **[Releases](https://github.com/Invariant-Dev/Nexo/releases)** section of this repository.

1. Download the version for your OS.
2. Add the executable to your system PATH for easy access.

### Building from Source

1. **Clone the repository** (or download the source code):
   ```bash
   git clone https://github.com/Invariant-Dev/project-nexo.git
   cd "project-nexo/new_source_code"
   ```

2. **Compile the engine**:
   ```bash
   g++ -std=c++17 -O3 Common.cpp Lexer.cpp Parser.cpp Interpreter.cpp main.cpp -o nexo -static -static-libgcc -static-libstdc++ -pthread
   ```
   *Note: On Windows, the output will automatically be `nexo.exe`.*

3. **Verify the installation**:
   ```bash
   ./nexo --version
   ```

---

## 💻 Getting Started

You can use Nexo by running script files.

### Running a Script

1. Create a file named `hello.nx`:
   ```nexo
   display "Hello, world!"
   set user to "Alice"
   display "Welcome to Nexo, " + user + "!"
   ```

2. Execute the script:
   ```bash
   ./nexo hello.nx
   ```

> **Pro Tip**: Nexo ignores "noise words" like `the`, `a`, `an`, and `please`. 
> Writing `set the user to "Alice"` works exactly the same as `set user to "Alice"`! Use the noise words simply for readability.

---

## 📖 Language Guide Tour

### Variables & Data Types
```nexo
set the name to "Nexo"
set version to 1.0
set is_awesome to true
set the empty_list to []
```

### Objects
```nexo
set config to an object called myConfig with the following properties:
    host is "localhost"
    port is 8080
    ssl_enabled is true
end object

display "Connecting to " + config.host + " on port " + config.port
set config.port to 443
```

### Arrays & Iteration
```nexo
set colors to ["red", "green", "blue"]
add "yellow" to colors

for each color in colors do the following:
    display uppercase color
end for

display "There are " + length of colors + " colors."
```

### Control Flow
```nexo
set limit to 10
if limit is greater than 5 then do the following:
    display "Limit is high!"
otherwise do the following:
    display "Limit is low."
end if

iterate from 1 through 5 using i:
    display "Counting: " + i
end iterate
```

### Functions & Logic
```nexo
define function called calculate_area with width, height:
    return width * height
end function

set area to call the function calculate_area with 10, 5
display "Area is " + area
```

### String Wizardry
```nexo
set text to "The quick brown fox"
if text contains "quick" do the following:
    display replace "fox" with "wolf" in text
end if

display substring of text from 4 with length 5  # Outputs: quick
```

---

## 🌟 Example Projects

Here are a few real-world examples to show you how powerful Nexo can be.

### 📝 1. To-Do List Manager (File I/O & JSON)
```nexo
display "=== Nexo Task Manager ==="

set file_path to "tasks.json"

if file file_path exists do the following:
    set content to read the file file_path
    set tasks to parse content as json
otherwise do the following:
    set tasks to []
end if

add "Learn Nexo v1.0 beta" to tasks
add "Build an awesome app" to tasks

write stringify tasks into the file file_path
display "Saved " + length of tasks + " tasks to disk."
```

### 🌐 2. Web API Fetcher (HTTP & JSON)
```nexo
display "Fetching weather data..."

set city to "London"
set url to "https://wttr.in/" + city + "?format=j1"

try the following:
    set response to http get url
    set weather_data to parse response as json
    
    set current_temp to the first item in weather_data.current_condition
    display "The current temperature in " + city + " is " + current_temp.temp_C + "°C"
if an error occurs then store it in err and do the following:
    display "Failed to fetch weather: " + err
end try
```

### 🚀 3. High-Concurrency Worker Pool
```nexo
display "Starting background workers..."

create channel called "jobs_queue"
create channel called "results"

in the background as "worker1" do the following:
    while true do the following:
        receive from channel "jobs_queue" and store it in job
        if job is equal to "STOP" do the following:
            break
        end if
        
        display "[Worker 1] Processing: " + job
        wait 1 seconds
        send job + " (Done by W1)" to channel "results"
    end while
end background task

# Send jobs
send "Compile Code" to channel "jobs_queue"
send "Run Tests" to channel "jobs_queue"
send "STOP" to channel "jobs_queue"

# Await results
receive from channel "results" and store it in res1
receive from channel "results" and store it in res2

display "All jobs completed!"
display "\nResults:"
display " - " + res1
display " - " + res2
```

---

## 🛠️ Editor Support

We're working on bringing syntax highlighting to your favorite editors!
- **VS Code**: Under development (Check back soon!)
- **Sublime Text**: Under development
- **Notepad++**: Define a custom language using the keywords found in `Token.h`.

---

## 🤝 Contributing

Nexo is a community-driven side-project! If you want to contribute to the v1.0 beta engine:
1. Fork the repository.
2. Check out the `new_source_code` directory.
3. The core engine lives in `Interpreter.cpp` and `Parser.cpp`. Feel free to implement new AST nodes!
4. Submit a Pull Request.

---

<div align="center">
  <b>Built with ❤️ as a passion project.</b><br>
  <i>"Programming has never been this natural."</i>
</div>