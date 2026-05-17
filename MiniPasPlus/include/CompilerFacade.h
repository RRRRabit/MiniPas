#ifndef MINIPASPLUS_COMPILERFACADE_H
#define MINIPASPLUS_COMPILERFACADE_H

#include "CompileResult.h"
#include <string>

// 编译器门面：GUI 以后只调用这个类，不直接依赖 Lexer、Parser、Interpreter。
class CompilerFacade {
    public:
    // 完成词法分析、语法分析、四元式生成和解释执行，并统一返回结果。
    CompileResult compileAndRun(const std::string& sourceCode);
};

#endif
