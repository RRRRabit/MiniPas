
#pragma once

#include <string>
#include "CompileResult.h"

using std::string;

// CompilerFacade 是总控类。
// 它负责把词法、语法、语义、四元式、优化串成一条线。
class CompilerFacade
{
public:
    // 与正式工程对齐：也使用 compileAndRun 作为统一入口名。
    CompileResult compileAndRun(const string &sourceCode);

    // 兼容旧调用：保留 compile 名称，内部转发到 compileAndRun。
    CompileResult compile(const string &sourceCode);
};
