// 文件说明：定义四元式结构，是中间代码的核心表示。

#pragma once

#include <string>

struct Quadruple {
    std::string op;      // 操作码：:=、+、-、*、/、if、wh、call、ret ...
    std::string arg1;    // 第一个输入
    std::string arg2;    // 第二个输入（无则为 "_"）
    std::string result;  // 输出位置（无则为 "_"）
};



