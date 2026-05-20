#include "CompilerFacade.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

// 命令行版本入口。
//
// 你可以把这个文件理解成“外壳程序”，它本身不做词法分析、语法分析、四元式生成或 VM 执行。
// 它只负责两件事：
// 1. 从文件或控制台读入 MiniPas 源程序文本。
// 2. 把文本交给 CompilerFacade::compileAndRun，然后把结果打印出来。
//
// 答辩时如果老师问“程序从哪里开始”，就指这里；
// 如果老师问“真正的编译逻辑在哪里”，就跳到 CompilerFacade.cpp。
int main(int argc, char* argv[]) {
    // 关闭 C++ iostream 和 C stdio 的同步，能让 cin/cout 稍微快一点。
    // tie(nullptr) 表示 cin 读入前不强制刷新 cout。这里不是核心逻辑，可以不用深究。
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string source;
    bool interactiveInput = false;

    // 两种输入方式：
    // 1. argc >= 2：用户把源文件路径作为命令行参数传进来。
    // 2. 否则：进入交互模式，用户一行一行粘贴代码，输入 :end 表示结束。
    if (argc >= 2) {
        std::ifstream in(argv[1], std::ios::binary);
        if (!in) {
            std::cerr << "无法打开源文件: " << argv[1] << "\n";
            return 2;
        }
        // istreambuf_iterator 可以一次性把整个文件内容读成 string。
        // 读入后，source 就是一整份 MiniPas 源程序。
        source.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    } else {
        interactiveInput = true;
        std::cout << "请粘贴源代码，单独输入一行 :end 后开始运行。\n";
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line == ":end") {
                break;
            }
            source += line;
            source.push_back('\n');
        }
    }

    // 这是本文件最关键的两行：
    // CompilerFacade 是“编译器总控类”，compileAndRun 会完整执行：
    // 词法分析 -> 语法/语义分析 -> 四元式优化 -> VM 执行。
    CompilerFacade compiler;
    CompileResult result = compiler.compileAndRun(source);

    // 如果 success 为 false，说明某个阶段失败了。
    // errorMessage 已经由 CompilerFacade 整理成人能读懂的错误信息。
    if (!result.success) {
        std::cerr << "编译失败: " << result.errorMessage << "\n";
        return 1;
    }

    // 下面这些输出主要是给你确认每个阶段确实产生了结果。
    // Token数：词法分析结果数量。
    // 四元式数：Parser 生成的中间代码数量。
    // 优化后四元式数：优化器处理后的中间代码数量。
    // VM指令数：真正交给虚拟机执行的指令数量。
    std::cout << "编译成功\n";
    std::cout << "Token数: " << result.tokens.size() << "\n";
    std::cout << "四元式数: " << result.quadruples.size() << "\n";
    std::cout << "优化后四元式数: " << result.optimizedQuadruples.size() << "\n";
    std::cout << "VM指令数: " << result.vmInstructions.size() << "\n";
    std::cout << "VM执行状态: " << (result.vmFallbackUsed ? "回退解释器" : "VM执行成功") << "\n";
    if (!result.vmErrorMessage.empty()) {
        std::cout << "VM报错: " << result.vmErrorMessage << "\n";
    }

    // runtimeValues 是最终变量表。
    // 例如源程序里 a := 1; b := a + 2;，这里就会打印 a=1, b=3。
    std::cout << "运行时长(ms): " << result.vmRuntimeMs << "\n";
    std::cout << "最终变量值:\n";
    for (const auto& kv : result.runtimeValues) {
        std::cout << "  " << kv.first << " = " << kv.second << "\n";
    }

    if (interactiveInput) {
        // Windows 控制台下避免程序结束后一闪而过。
        std::cout << "\n运行结束。按回车退出...";
        std::cin.clear();
        std::string dummy;
        std::getline(std::cin, dummy);
    }
    return 0;
}
