cmake_minimum_required(VERSION 3.16)

project(MiniPasPlus VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 工程说明：核心编译逻辑在 .qt/src 与 .qt/include，
# Qt 可视化界面在 GUI/，VM 执行器在 VM/。
option(BUILD_UI "Build Qt visualization app" ON)

add_library(MiniPasCore STATIC
    .qt/src/CompilerFacade.cpp
    .qt/src/BackendOptimization.cpp
    .qt/src/Lexer.cpp
    .qt/src/Parser.cpp
    .qt/src/ParserSupport.cpp
    .qt/src/SymbolTable.cpp
    .qt/src/TypeTable.cpp
    .qt/src/Interpreter.cpp
    VM/VirtualMachine.cpp
    .qt/include/CompilerFacade.h
    .qt/include/CompilerBackendStages.h
    .qt/include/CompileResult.h
    .qt/include/Token.h
    .qt/include/Lexer.h
    .qt/include/Parser.h
    .qt/include/SymbolTable.h
    .qt/include/TypeTable.h
    .qt/include/Quadruple.h
    .qt/include/Interpreter.h
    VM/VirtualMachine.h
)

target_include_directories(MiniPasCore PUBLIC .qt/include VM)

if(BUILD_UI)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

    add_executable(MiniPasPlus
        GUI/main.cpp
        GUI/MainWindow.cpp
        GUI/CodeEditorWithLineNumber.cpp
        GUI/BackendTargetCodeGen.cpp
        GUI/MainWindow.h
        GUI/CodeEditorWithLineNumber.h
        GUI/TargetCodeTrace.h
    )

    target_include_directories(MiniPasPlus PRIVATE .qt/include GUI)
    target_link_libraries(MiniPasPlus PRIVATE MiniPasCore Qt${QT_VERSION_MAJOR}::Widgets)
endif()

