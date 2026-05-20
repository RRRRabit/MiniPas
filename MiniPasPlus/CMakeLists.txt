cmake_minimum_required(VERSION 3.16)

project(MiniPasPlus VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 答辩阅读版默认只构建命令行核心。
# Qt 可视化代码集中放在 GUI/ 中，VM 执行器单独放在 VM/ 中，避免 src/include 里混入展示或运行器细节。
option(BUILD_UI "Build Qt visualization app" OFF)

add_library(MiniPasCore STATIC
    src/CompilerFacade.cpp
    src/BackendOptimization.cpp
    src/Lexer.cpp
    src/Parser.cpp
    src/ParserSupport.cpp
    src/SymbolTable.cpp
    src/TypeTable.cpp
    src/Interpreter.cpp
    VM/VirtualMachine.cpp
    include/CompilerFacade.h
    include/CompilerBackendStages.h
    include/CompileResult.h
    include/Token.h
    include/Lexer.h
    include/Parser.h
    include/SymbolTable.h
    include/TypeTable.h
    include/Quadruple.h
    include/Interpreter.h
    VM/VirtualMachine.h
)

target_include_directories(MiniPasCore PUBLIC include VM)

if(BUILD_UI)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

    add_executable(MiniPasPlus
        GUI/main.cpp
        GUI/MainWindow.cpp
        GUI/CodeEditorWithLineNumber.cpp
        GUI/SimplePrecedence.cpp
        GUI/BackendTargetCodeGen.cpp
        GUI/MainWindow.h
        GUI/CodeEditorWithLineNumber.h
        GUI/SimplePrecedence.h
        GUI/TargetCodeTrace.h
    )

    target_include_directories(MiniPasPlus PRIVATE include GUI)
    target_link_libraries(MiniPasPlus PRIVATE MiniPasCore Qt${QT_VERSION_MAJOR}::Widgets)
endif()

add_executable(MiniPasCoreCLI
    src/main_cli.cpp
)

target_include_directories(MiniPasCoreCLI PRIVATE include)
target_link_libraries(MiniPasCoreCLI PRIVATE MiniPasCore)
