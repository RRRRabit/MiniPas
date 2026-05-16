cmake_minimum_required(VERSION 3.16)

project(MiniPasPlus VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

add_executable(MiniPasPlus
    src/main.cpp
    src/MainWindow.cpp
    src/CompilerFacade.cpp
    src/Lexer.cpp
    src/Parser.cpp
    src/SymbolTable.cpp
    src/TypeTable.cpp
    src/Interpreter.cpp
    src/VirtualMachine.cpp
    include/MainWindow.h
    include/CompilerFacade.h
    include/CompileResult.h
    include/Token.h
    include/Lexer.h
    include/Parser.h
    include/SymbolTable.h
    include/TypeTable.h
    include/Quadruple.h
    include/Interpreter.h
    include/VirtualMachine.h
)

target_include_directories(MiniPasPlus PRIVATE include)

target_link_libraries(MiniPasPlus PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
