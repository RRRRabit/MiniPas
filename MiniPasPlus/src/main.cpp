// 文件说明：命令行入口示例。构造示例程序，调用 CompilerFacade，并打印阶段统计结果。

#include "CompilerFacade.h"
#include <iostream>

using std::cout;
using std::string;

// main 只负责组装输入、调用总控并打印结果统计。
int main()
{
    string source =
        "program demo\n"
        "type person = record age: integer; end;\n"
        "nums = array[1..5] of integer;\n"
        "function add(a, b: integer): integer;\n"
        "var t: integer;\n"
        "begin\n"
        "  t := a + b\n"
        "end;\n"
        "var x, y: integer; p: person; arr: nums;\n"
        "begin\n"
        "  x := 1 + 2 * 3;\n"
        "  arr[1] := x;\n"
        "  p.age := arr[1];\n"
        "  if x > 3 then y := add(x, 1) else y := 0;\n"
        "  while y > 0 do y := y - 1\n"
        "end.";

    CompilerFacade compiler; // 创建编译总控对象
    CompileResult result = compiler.compileAndRun(source);

    if (!result.success)
    {
        cout << "编译失败: " << result.errorMessage << "\n";
        return 0;
    }

    cout << "Token 数量: " << result.tokens.size() << "\n";
    cout << "符号数量: " << result.symbols.size() << "\n";
    cout << "record 类型数量: " << result.recordTypes.size() << "\n";
    cout << "array 类型数量: " << result.arrayTypes.size() << "\n";
    cout << "函数数量: " << result.functionTable.size() << "\n";
    cout << "四元式数量: " << result.quadruples.size() << "\n";
    cout << "优化后四元式数量: " << result.optimizedQuadruples.size() << "\n";
    cout << "递归分析节点: " << result.parserTrace.size() << "\n";

    return 0;
}
