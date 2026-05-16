#include "CompilerFacade.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string source;
    bool interactiveInput = false;
    if (argc >= 2) {
        std::ifstream in(argv[1], std::ios::binary);
        if (!in) {
            std::cerr << "无法打开源文件: " << argv[1] << "\n";
            return 2;
        }
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

    CompilerFacade compiler;
    CompileResult result = compiler.compileAndRun(source);
    if (!result.success) {
        std::cerr << "编译失败: " << result.errorMessage << "\n";
        return 1;
    }

    std::cout << "编译成功\n";
    std::cout << "Token数: " << result.tokens.size() << "\n";
    std::cout << "四元式数: " << result.quadruples.size() << "\n";
    std::cout << "优化后四元式数: " << result.optimizedQuadruples.size() << "\n";
    std::cout << "VM指令数: " << result.vmInstructions.size() << "\n";
    std::cout << "VM执行状态: " << (result.vmFallbackUsed ? "回退解释器" : "VM执行成功") << "\n";
    if (!result.vmErrorMessage.empty()) {
        std::cout << "VM报错: " << result.vmErrorMessage << "\n";
    }
    std::cout << "运行时长(ms): " << result.vmRuntimeMs << "\n";
    std::cout << "最终变量值:\n";
    for (const auto& kv : result.runtimeValues) {
        std::cout << "  " << kv.first << " = " << kv.second << "\n";
    }

    if (interactiveInput) {
        std::cout << "\n运行结束。按回车退出...";
        std::cin.clear();
        std::string dummy;
        std::getline(std::cin, dummy);
    }
    return 0;
}
