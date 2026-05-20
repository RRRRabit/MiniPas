#ifndef MINIPASPLUS_COMPILERFACADE_H
#define MINIPASPLUS_COMPILERFACADE_H

#include "CompileResult.h"
#include <string>

// 编译器门面（Facade）。
//
// “门面”的意思是：外部代码不需要知道编译器内部有多少步骤，
// 只要调用 compileAndRun(sourceCode)，就能一次性拿到完整结果。
//
// 如果没有这个类，GUI/CLI 就必须自己按顺序创建 Lexer、Parser、VirtualMachine，
// 那样主界面代码会和编译核心耦合得很严重，不方便维护。
class CompilerFacade {
    public:
    // 完成整个编译运行流程：
    // 1. 词法分析：源代码 -> Token；
    // 2. 语法/语义分析：Token -> 符号表、类型表、四元式；
    // 3. 后端优化：四元式 -> 优化后四元式；
    // 4. 目标代码生成：四元式 -> VM 指令；
    // 5. 虚拟机执行：VM 指令 -> 最终变量值。
    CompileResult compileAndRun(const std::string& sourceCode);
};

#endif
