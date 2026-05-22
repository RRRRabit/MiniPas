// 文件说明：定义四元式结构，是中间代码的核心表示。

#pragma once

#include <string>

using std::string;

struct Quadruple
{
    string op;     // 操作码：:=、+、-、*、/、if、wh、call、ret ...
    string arg1;   // 第一个输入
    string arg2;   // 第二个输入（无则为 "_"）
    string result; // 输出位置（无则为 "_"）
};
