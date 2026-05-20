#ifndef MINIPASPLUS_VIRTUALMACHINE_H
#define MINIPASPLUS_VIRTUALMACHINE_H

#include "CompileResult.h"
#include <map>
#include <string>
#include <vector>

// 简单虚拟机 VM（Virtual Machine）。
//
// 编译器前面生成的是“四元式”，四元式更接近编译原理课本里的中间代码；
// 真正运行时，本项目会先把四元式翻译成 VMInstruction，再由这个类逐条执行。
//
// 可以把 VM 想象成一个“很小的 CPU”：
// - programCounter 表示当前执行到第几条指令；
// - globals_ 保存全局变量；
// - frames_ 保存函数调用时的局部变量；
// - 每条 VMInstruction 像 ADD、MOV、JMP 这种指令，会改变变量或跳转位置。
class VirtualMachine
{
public:
    // 执行 VM 指令序列。
    // 参数 program 是 generateVmInstructions 生成出来的目标代码。
    void execute(const std::vector<VMInstruction>& program);

    // 返回最终全局变量表。
    // GUI 和 CLI 都用它来展示“程序运行结束后每个变量的值”。
    const std::map<std::string, double>& values() const;

private:
    // Frame 表示一次函数调用的运行现场。
    // returnPc：函数执行完以后，要回到哪条指令继续执行。
    // locals：这个函数自己的局部变量和形参。
    struct Frame
    {
        int returnPc = -1;
        std::map<std::string, double> locals;
    };

    // 全局变量区：主程序变量、数组元素展开名、记录字段展开名都存在这里。
    std::map<std::string, double> globals_;

    // 调用栈：每调用一个函数就压入一个 Frame，函数返回时弹出。
    std::vector<Frame> frames_;

    // 参数缓冲区：CALL 之前会先执行若干 ARG，把实参值暂存在这里。
    std::vector<double> argBuffer_;

    // 最近一次函数返回值。CALL 结束后，MOVRET 会把 retVal_ 写入接收变量。
    double retVal_ = 0.0;

    // 读操作数的值。操作数可能是数字常量、变量名、临时变量名或标签形式。
    double readValue(const std::string& operand) const;

    // 给变量写值。它会根据当前是否在函数里，决定写到 locals 还是 globals。
    void writeValue(const std::string& operand, double value);

    // 一些小工具函数：判断数字、判断布尔真值、把 "L12" 解析成 12。
    bool isNumber(const std::string& text) const;
    bool isTrue(double v) const;
    int parseLabel(const std::string& text) const;

    // 下面这些函数把不同类型的 VM 指令分组处理。
    // 返回 true 表示“这条指令已经被我处理了”，execute 主循环就会继续下一轮。
    bool executeDataTransferInstruction(const VMInstruction& instruction, int& programCounter);
    bool executeBinaryInstruction(const VMInstruction& instruction, int& programCounter);
    bool executeUnaryInstruction(const VMInstruction& instruction, int& programCounter);
    bool executeJumpInstruction(const VMInstruction& instruction, int& programCounter);
    bool executeCallInstruction(const VMInstruction& instruction, int& programCounter);
};

namespace vm
{

    // 把四元式翻译成 VMInstruction。
    // functions/parameters 用来确定函数入口位置，以及函数调用时形参如何绑定实参。
    std::vector<VMInstruction> generateVmInstructions(const std::vector<Quadruple>& quads,
    const std::vector<FunctionInfo>& functions,
    const std::vector<ParameterInfo>& parameters);
 
} // namespace vm

#endif
