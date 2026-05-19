cmake_minimum_required(VERSION 3.16)

project(MiniPasPlus VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_UI "Build Qt visualization app" ON)

add_library(MiniPasCore STATIC
    src/CompilerFacade.cpp
    src/BackendOptimization.cpp
    src/BackendTargetCodeGen.cpp
    src/Lexer.cpp
    src/SimplePrecedence.cpp
    src/Parser.cpp
    src/ParserSupport.cpp
    src/SymbolTable.cpp
    src/TypeTable.cpp
    src/Interpreter.cpp
    src/VirtualMachine.cpp
    include/CompilerFacade.h
    include/CompilerBackendStages.h
    include/CompileResult.h
    include/Token.h
    include/Lexer.h
    include/SimplePrecedence.h
    include/Parser.h
    include/SymbolTable.h
    include/TypeTable.h
    include/Quadruple.h
    include/Interpreter.h
    include/VirtualMachine.h
)

target_include_directories(MiniPasCore PUBLIC include)

if(BUILD_UI)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

    add_executable(MiniPasPlus
        src/main.cpp
        src/MainWindow.cpp
        src/CodeEditorWithLineNumber.cpp
        include/MainWindow.h
        include/CodeEditorWithLineNumber.h
    )

    target_include_directories(MiniPasPlus PRIVATE include)
    target_link_libraries(MiniPasPlus PRIVATE MiniPasCore Qt${QT_VERSION_MAJOR}::Widgets)
endif()

add_executable(MiniPasCoreCLI
    src/main_cli.cpp
)

target_include_directories(MiniPasCoreCLI PRIVATE include)
target_link_libraries(MiniPasCoreCLI PRIVATE MiniPasCore)
