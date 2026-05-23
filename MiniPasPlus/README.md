# README：围绕 include 和 src 讲功能

这份指南只围绕当前项目的两个核心目录讲：

```text
MiniPasPlus/include  头文件：定义数据结构、类、函数接口
MiniPasPlus/src      源文件：实现具体功能逻辑
```

阅读目标不是背代码，而是能对着图形化界面说清楚：

```text
这个功能的数据从哪里来？
哪个类负责？
哪个函数生成？
最后放进 CompileResult 的哪个字段给 GUI 展示？
```

## 0. 总体流程

先记住主流程：

```text
main.cpp
  -> CompilerFacade::compileAndRun
  -> Lexer::tokenize
  -> Parser::parse
  -> BackendOptimization::optimizeByBasicBlocks
  -> TargetCodeDemo::generate
  -> ActivationRecordDemo::build
  -> RuntimeResultDemo::makePlaceholder
  -> CompileResult 返回给界面展示
```

GUI 本身只是展示层。真正的数据都在 `CompileResult` 里面。

## 1. 代码量总览

| 目录        | 文件数 | 作用                                                           |
| ----------- | -----: | -------------------------------------------------------------- |
| `include` |     12 | 定义 Token、符号表、类型表、Parser、总控、展示数据结构         |
| `src`     |      8 | 实现词法、语法、优化、目标代码展示、活动记录展示、运行结果占位 |
| 合计        |     20 | 教学讲解版完整主线                                             |

## 2. include 目录：先看“有什么东西”

`include` 里主要是声明。你可以把它理解成目录表：告诉你项目有哪些类、哪些表、哪些结果结构。

### 2.1 `include/Token.h`

这个文件定义词法分析的基本单位。

包含内容：

- `enum class TokenType`
- `struct Token`
- `struct KeywordEntry`
- `struct DelimiterEntry`
- `struct ConstantEntry`

对应 GUI：

- 词法分析 Token 表
- 关键字表
- 界符表
- 常量表

讲法：

```text
源代码首先会被 Lexer 切成 Token。
Token 记录三件事：类型、文本、行列号。
关键字表、界符表、常量表也用这里的结构保存。
```

### 2.2 `include/Lexer.h`

这个文件声明词法分析器。

核心类：

```cpp
class Lexer
```

关键函数：

- `tokenize()`：生成 Token 表。
- `identifierTable()`：返回标识符表。
- `constantTable()`：返回常量表。
- `keywordTable()`：返回关键字表。
- `delimiterTable()`：返回界符表。
- `readIdentifierOrKeyword()`：识别关键字或标识符。
- `readNumber()`：识别数字常量。
- `readOperatorOrDelimiter()`：识别运算符或界符。
- `rememberIdentifier()`：把标识符加入 I 表。
- `rememberConstant()`：把常量加入 C 表。

对应 GUI：

- 词法分析页
- 标识符表
- 常量表
- 关键字表
- 界符表

讲法：

```text
Lexer 逐字符扫描源代码。
遇到字母开头的单词，判断它是不是关键字；不是关键字就加入标识符表。
遇到数字，加入常量表。
遇到符号，判断是运算符还是界符。
最后返回 Token 序列。
```

### 2.3 `include/SymbolTable.h`

这个文件定义符号表。

包含内容：

- `struct Symbol`
- `class SymbolTable`

`Symbol` 保存：

- `name`：名字，如 `x`、`add`、`person`
- `typeName`：类型名，如 `integer`、`person`
- `kind`：类别，如 `variable`、`function`、`parameter`
- `address`：相对地址
- `size`：占用大小

关键函数：

- `add()`：加入符号。
- `find()`：按名字查符号。
- `exists()`：判断是否声明过。
- `all()`：返回整张符号表给 GUI。

对应 GUI：

- 符号表 SYNBL
- 变量表展示
- 函数/参数相关展示

讲法：

```text
符号表保存所有“有名字的东西”。
变量、函数、参数、临时变量都会进入符号表。
Parser 在声明阶段往符号表里 add，语义检查时用 find 查询。
```

### 2.4 `include/TypeTable.h`

这个文件定义类型表。

包含内容：

- `struct FieldInfo`
- `struct RecordType`
- `struct ArrayType`
- `class TypeTable`

关键函数：

- `addRecord()`：加入 record 类型。
- `addArray()`：加入 array 类型。
- `findRecord()`：查 record 类型。
- `findArray()`：查 array 类型。
- `findField()`：查 record 字段。
- `records()`：返回 record 类型表。
- `arrays()`：返回 array 类型表。

对应 GUI：

- TYPEL 类型表
- RINFL record 字段表
- AINFL array 数组表

讲法：

```text
符号表保存名字，类型表保存类型内部结构。
record 的字段列表保存在 RecordType 里。
array 的上下界和元素类型保存在 ArrayType 里。
Parser 分析 type 声明时填 TypeTable。
```

### 2.5 `include/Quadruple.h`

这个文件定义四元式。

结构：

```cpp
struct Quadruple {
    string op;
    string arg1;
    string arg2;
    string result;
};
```

对应 GUI：

- 原始四元式表
- 优化后四元式表
- 目标代码页

讲法：

```text
四元式是中间代码。
每条四元式用 op、arg1、arg2、result 表示一个操作。
例如 x := 1 生成 (:=, 1, _, x)。
例如 a + b 生成 (+, a, b, t1)。
```

### 2.6 `include/CompileResult.h`

这是最重要的数据汇总文件。

包含内容：

- `FunctionInfo`
- `ParameterInfo`
- `BasicBlock`
- `ParserTraceNode`
- `ParserLogItem`
- `TargetCodeItem`
- `ActivationRecordItem`
- `RuntimeValue`
- `CompileResult`

对应 GUI：几乎所有页面。

`CompileResult` 里保存：

- `tokens`：Token 表。
- `identifiers`：标识符表。
- `constants`：常量表。
- `symbols`：符号表。
- `recordTypes`：record 类型表。
- `arrayTypes`：array 类型表。
- `functions`：函数表。
- `parameters`：参数表。
- `quadruples`：原始四元式。
- `optimizedQuadruples`：优化后四元式。
- `basicBlocks`：基本块。
- `parserTrace`：递归调用树。
- `parserSteps`：Token 匹配日志。
- `parserActions`：语义动作日志。
- `targetCodeItems`：目标代码展示行。
- `activationRecords`：活动记录展示行。
- `runtimeValues`：VM 运行结果展示行。

讲法：

```text
GUI 不直接重新分析源代码。
GUI 调用 CompilerFacade 得到 CompileResult。
所有表格都从 CompileResult 的字段取数据。
```

### 2.7 `include/Parser.h`

这个文件声明语法分析器。

核心类：

```cpp
class Parser
```

主要函数分组：

声明分析：

- `parseProgram()`
- `parseTypeDeclPart()`
- `parseRecordTypeDecl()`
- `parseArrayTypeDecl()`
- `parseFunctionDeclPart()`
- `parseFunctionDecl()`
- `parseParamList()`
- `parseVarDeclPart()`

语句分析：

- `parseCompoundStmt()`
- `parseStmtList()`
- `parseStmt()`
- `parseAssignStmt()`
- `parseCallStmt()`
- `parseIfStmt()`
- `parseWhileStmt()`

表达式分析：

- `parseLeftValue()`
- `parseCondition()`
- `parseExpression()`
- `parseTerm()`
- `parseFactor()`
- `parseFunctionCall()`

语义检查：

- `requireVariableDeclared()`
- `canAssign()`
- `checkArrayIndexAndGetElementType()`
- `checkRecordFieldAndGetType()`
- `findFunctionParams()`
- `checkArgumentList()`

展示日志：

- `enterRule()`
- `leaveRule()`
- `logAction()`

对应 GUI：

- 递归分析过程
- 符号表
- 类型表
- 函数表
- 原始四元式
- 基本块

讲法：

```text
Parser 是递归下降分析器。
每个 parseXXX 函数对应一种语法结构。
它一边检查语法和语义，一边生成符号表、类型表、四元式和递归分析日志。
```

### 2.8 `include/CompilerFacade.h`

这个文件声明总控类。

核心类：

```cpp
class CompilerFacade
```

关键函数：

- `compileAndRun()`
- `compile()`

对应 GUI：

- 点击“编译并运行”后的总入口。

讲法：

```text
CompilerFacade 是总控入口。
界面拿到源代码后，把字符串交给 CompilerFacade。
CompilerFacade 负责调用 Lexer、Parser、优化器和展示数据生成器。
```

### 2.9 `include/BackendOptimization.h`

这个文件声明优化器。

核心类：

```cpp
class BackendOptimization
```

关键函数：

- `optimizeByBasicBlocks()`

对应 GUI：

- 中间代码及优化页
- 优化后四元式表

讲法：

```text
优化器接收原始四元式和基本块。
它按基本块局部优化，输出优化后的四元式。
```

### 2.10 `include/TargetCodeDemo.h`

这个文件声明目标代码展示生成器。

核心类：

```cpp
class TargetCodeDemo
```

关键函数：

- `generate()`

对应 GUI：

- 目标代码页
- RDL 列
- SEM 列

讲法：

```text
TargetCodeDemo 把四元式转换成目标代码展示文本。
它不是真正执行器，只负责生成 GUI 上看到的目标代码行。
```

### 2.11 `include/ActivationRecordDemo.h`

这个文件声明活动记录展示生成器。

核心类：

```cpp
class ActivationRecordDemo
```

关键函数：

- `build()`

对应 GUI：

- VALL 活动记录页

讲法：

```text
活动记录展示由符号表、函数表、参数表整理出来。
主程序和每个函数都有一个展示用栈帧。
```

### 2.12 `include/RuntimeResultDemo.h`

这个文件声明 VM 结果展示生成器。

核心类：

```cpp
class RuntimeResultDemo
```

关键函数：

- `makePlaceholder()`

对应 GUI：

- VM 运行结果页

讲法：

```text
这里不实现 VM 内部。
它只生成运行结果表的数据形态。
真实运行时，变量最终值由 VM 执行后填入。
```

## 3. src 目录：再看“怎么实现”

`src` 里是具体逻辑。建议按下面顺序读。

### 3.1 `src/main.cpp`

这个文件是演示入口。

主要内容：

- 准备一段 MiniPas 示例代码。
- 创建 `CompilerFacade`。
- 调用 `compileAndRun()`。
- 打印各类结果数量。

对应 GUI：

- 相当于“编译并运行”按钮的最小命令行演示。

讲法：

```text
main.cpp 不负责具体编译。
它只准备 source 字符串，然后调用 CompilerFacade。
真正逻辑都在 CompilerFacade 后面。
```

### 3.2 `src/CompilerFacade.cpp`

这是总控实现，最重要。

核心函数：

```cpp
CompileResult CompilerFacade::compileAndRun(const string &sourceCode)
```

执行顺序：

1. 创建 `Lexer`，调用 `tokenize()`。
2. 把 Token、标识符表、常量表保存到 `result`。
3. 创建 `Parser`，调用 `parse()`。
4. 复制 Parser 产生的符号表、类型表、函数表、参数表、四元式、基本块、递归日志。
5. 调用 `BackendOptimization` 生成优化后四元式。
6. 调用 `TargetCodeDemo` 生成目标代码展示行。
7. 调用 `ActivationRecordDemo` 生成活动记录展示行。
8. 调用 `RuntimeResultDemo` 生成 VM 结果展示行。
9. 返回 `CompileResult`。

对应 GUI：全部页面。

讲法：

```text
CompilerFacade 是总调度。
GUI 只需要调用它一次，就能拿到所有表格需要的数据。
```

### 3.3 `src/Lexer.cpp`

这个文件实现词法分析。

关键函数：

- `Lexer::Lexer()`：初始化关键字表和界符表。
- `skipBlankAndComment()`：跳过空白和注释。
- `readIdentifierOrKeyword()`：识别关键字或标识符。
- `readNumber()`：识别常量。
- `readOperatorOrDelimiter()`：识别运算符或界符。
- `tokenize()`：循环扫描并生成 Token 表。
- `rememberIdentifier()`：维护标识符表。
- `rememberConstant()`：维护常量表。

对应 GUI：

- 词法分析 Token 表
- 关键字表
- 界符表
- 标识符表
- 常量表

讲法：

```text
Lexer 从左到右扫描源代码。
字母开头的读成单词，再判断是关键字还是标识符。
数字读成常量。
符号读成运算符或界符。
每识别一个东西，就生成 Token 或加入对应表。
```

### 3.4 `src/Parser.cpp`

这是最大、最核心的文件。

它做四件事：

1. 递归下降语法分析。
2. 语义检查。
3. 生成符号表、类型表、函数表、参数表。
4. 生成四元式、基本块和递归分析日志。

#### A. 基础工具函数

- `peek()`：看当前 Token。
- `check()`：判断当前 Token 是否符合要求。
- `match()`：如果匹配就前进。
- `consume()`：必须匹配，否则报错。

讲法：

```text
Parser 不直接按字符判断，而是按 Token 判断。
check 只看不吃，match 可选吃，consume 必须吃。
```

#### B. 递归日志函数

- `enterRule()`
- `leaveRule()`
- `logAction()`

对应 GUI：

- 递归调用树
- Token 匹配日志
- 语义动作日志

讲法：

```text
Parser 进入某个语法函数时记录 enterRule。
匹配 Token 时记录 parserSteps。
生成四元式时记录 parserActions。
所以 GUI 能展示递归分析过程。
```

#### C. 声明分析函数

- `parseProgram()`：分析整个程序结构。
- `parseTypeDeclPart()`：分析 type 声明区。
- `parseRecordTypeDecl()`：生成 record 类型表。
- `parseArrayTypeDecl()`：生成 array 类型表。
- `parseFunctionDeclPart()`：分析函数声明区。
- `parseFunctionDecl()`：生成函数表。
- `parseParamList()`：生成参数表。
- `parseVarDeclPart()`：生成变量符号表。

对应 GUI：

- SYNBL 符号表
- TYPEL 类型表
- RINFL record 表
- AINFL array 表
- PFINFL 函数/参数表

讲法：

```text
声明区主要负责“建表”。
type 声明填 TypeTable。
var 声明填 SymbolTable。
function 声明填 FunctionInfo 和 ParameterInfo。
```

#### D. 语句分析函数

- `parseCompoundStmt()`：分析 `begin ... end`。
- `parseStmtList()`：分析多条语句。
- `parseStmt()`：判断当前是哪种语句。
- `parseAssignStmt()`：赋值语句。
- `parseCallStmt()`：函数调用语句。
- `parseIfStmt()`：if/else。
- `parseWhileStmt()`：while。

对应 GUI：

- 递归分析过程
- 原始四元式表

讲法：

```text
语句区主要负责“识别结构并生成四元式”。
if 生成 if/el/ie。
while 生成 wh/do/we。
赋值生成 :=。
函数调用生成 param/call。
```

#### E. 表达式分析函数

- `parseExpression()`：处理 `+ -`。
- `parseTerm()`：处理 `* /`。
- `parseFactor()`：处理变量、常量、括号、函数调用、数组、record 字段。
- `parseCondition()`：处理关系运算。
- `parseLeftValue()`：处理赋值左边。

讲法：

```text
表达式优先级靠函数层级实现。
Expression 先调用 Term，Term 先调用 Factor。
所以乘除比加减先生成四元式。
```

#### F. 语义检查函数

- `requireVariableDeclared()`：检查名字是否声明。
- `canAssign()`：检查赋值类型兼容。
- `checkArrayIndexAndGetElementType()`：检查数组下标是 integer，并返回元素类型。
- `checkRecordFieldAndGetType()`：检查 record 字段是否存在。
- `findFunctionParams()`：查函数形参。
- `checkArgumentList()`：检查实参数量和类型。

讲法：

```text
语义检查不是单独一个阶段，而是 Parser 分析到对应结构时马上检查。
用变量时查符号表。
用数组时查类型表。
用 record 字段时查字段表。
函数调用时查参数表。
```

#### G. 四元式和基本块

- `newTemp()`：生成临时变量。
- `emit()`：生成四元式。
- `buildBasicBlocks()`：根据控制流标记划分基本块。

讲法：

```text
表达式中间结果放进 t1、t2 这样的临时变量。
emit 是生成四元式的统一入口。
基本块用于后续优化。
```

### 3.5 `src/BackendOptimization.cpp`

这个文件实现四元式优化。

核心函数：

```cpp
optimizeByBasicBlocks(...)
```

实现的优化：

- 复制传播：`t1 := x` 后面可以用 `x`。
- 代数化简：`x + 0 -> x`，`x * 1 -> x`。
- 公共子表达式：同一个表达式算过就复用。

对应 GUI：

- 中间代码及优化页
- 优化后四元式表

讲法：

```text
优化器按基本块处理四元式。
它不会跨 if/while 随便优化，避免改变程序含义。
```

### 3.6 `src/TargetCodeDemo.cpp`

这个文件实现目标代码展示。

关键函数：

- `quadToText()`：把四元式转成文本。
- `opToTarget()`：把四元式操作符转成目标指令名。
- `pseudoTargetCode()`：生成目标代码展示文本。
- `rdlText()`：生成 RDL 列说明。
- `TargetCodeDemo::generate()`：生成目标代码表全部行。

对应 GUI：

- 目标代码页
- RDL 列
- SEM 列

讲法：

```text
目标代码页不是重新分析源代码。
它遍历优化后的四元式，根据 op 生成 MOV、ADD、JMP、CALL 等展示文本。
RDL 说明本行用到哪些变量。
SEM 说明根据语义动作选择了什么目标指令。
```

### 3.7 `src/ActivationRecordDemo.cpp`

这个文件实现活动记录展示。

核心函数：

```cpp
ActivationRecordDemo::build(...)
```

数据来源：

- 符号表 `symbols`
- 函数表 `functions`
- 参数表 `parameters`

生成内容：

- 主程序活动记录。
- 函数活动记录。
- 参数区。
- 局部变量区。
- 临时变量区。
- 返回地址、Old SP、Display 等展示项。

对应 GUI：

- VALL 活动记录页

讲法：

```text
活动记录不是重新编译出来的。
它根据符号表、函数表和参数表整理出运行时栈帧展示。
主程序一帧，每个函数一帧。
```

### 3.8 `src/RuntimeResultDemo.cpp`

这个文件生成 VM 运行结果页的展示数据形态。

核心函数：

```cpp
RuntimeResultDemo::makePlaceholder(...)
```

对应 GUI：

- VM 运行结果页

讲法：

```text
这里不实现 VM。
它只告诉你 VM 结果表长什么样。
真实运行时，VM 执行目标指令后把最终变量值填进这张表。
```

## 4. 按 GUI 页面讲代码

### 4.1 词法分析页

讲这些文件：

- `include/Token.h`
- `include/Lexer.h`
- `src/Lexer.cpp`

讲法：

```text
Token 表来自 Lexer::tokenize。
关键字表和界符表在 Lexer 构造函数中固定生成。
标识符表由 rememberIdentifier 维护。
常量表由 rememberConstant 维护。
```

### 4.2 符号表系统页

讲这些文件：

- `include/SymbolTable.h`
- `include/TypeTable.h`
- `include/CompileResult.h`
- `src/Parser.cpp`

讲法：

```text
Parser 分析声明区时填符号表和类型表。
var 声明进入 SymbolTable。
record 和 array 进入 TypeTable。
function 和 parameter 进入函数表和参数表。
GUI 从 CompileResult 读取这些表。
```

### 4.3 递归分析过程页

讲这些函数：

- `enterRule()`
- `leaveRule()`
- `consume()`
- `emit()`

讲法：

```text
进入语法规则时记录 parserTrace。
匹配 Token 时记录 parserSteps。
生成四元式时记录 parserActions。
GUI 用这三类日志显示递归分析过程。
```

### 4.4 简单优先分析页

当前 `include/src` 主线没有单独实现简单优先分析文件。

讲法建议：

```text
表达式优先级在 Parser 中通过 parseExpression、parseTerm、parseFactor 实现。
GUI 的简单优先分析页是展示表达式移进规约过程，用来辅助理解表达式分析。
```

如果需要单独讲简单优先分析代码，应补充对应展示模块文件。

### 4.5 原始四元式页

讲这些函数：

- `emit()`
- `parseAssignStmt()`
- `parseIfStmt()`
- `parseWhileStmt()`
- `parseFunctionCall()`

讲法：

```text
所有四元式都通过 emit 生成。
赋值生成 :=。
if 生成 if/el/ie。
while 生成 wh/do/we。
函数调用生成 param/call。
```

### 4.6 中间代码及优化页

讲这些文件：

- `include/BackendOptimization.h`
- `src/BackendOptimization.cpp`

讲法：

```text
Parser 先生成原始四元式和基本块。
优化器按基本块优化，得到 optimizedQuadruples。
GUI 同时展示优化前和优化后。
```

### 4.7 目标代码页

讲这些文件：

- `include/TargetCodeDemo.h`
- `src/TargetCodeDemo.cpp`

讲法：

```text
目标代码展示由 TargetCodeDemo 生成。
它遍历四元式，把 op 映射成 MOV、ADD、JMP、CALL 等展示指令。
RDL 和 SEM 是解释目标代码生成过程的辅助列。
```

### 4.8 VM 运行结果页

讲这些文件：

- `include/RuntimeResultDemo.h`
- `src/RuntimeResultDemo.cpp`

讲法：

```text
教学版不实现 VM 内部。
RuntimeResultDemo 只生成运行结果表的数据形态。
真实项目里 VM 执行后会把最终变量值放到结果表。
```

### 4.9 VALL 活动记录页

讲这些文件：

- `include/ActivationRecordDemo.h`
- `src/ActivationRecordDemo.cpp`

讲法：

```text
活动记录展示来自符号表、函数表、参数表。
主程序和每个函数都有一组展示项。
这些展示项说明运行时栈帧大概包含什么。
```

## 5. 最推荐的讲解顺序

如果学生要对你讲整个项目，建议按这个顺序：

1. `src/main.cpp`：程序从哪里开始。
2. `src/CompilerFacade.cpp`：总流程。
3. `src/Lexer.cpp`：词法分析和词法表。
4. `src/Parser.cpp` 的声明部分：符号表、类型表、函数表。
5. `src/Parser.cpp` 的语句部分：if/while/赋值/函数调用。
6. `src/Parser.cpp` 的表达式部分：优先级和临时变量。
7. `src/Parser.cpp` 的语义检查部分：声明、类型、数组、record、函数参数。
8. `src/BackendOptimization.cpp`：优化。
9. `src/TargetCodeDemo.cpp`：目标代码展示。
10. `src/ActivationRecordDemo.cpp`：活动记录展示。
11. `src/RuntimeResultDemo.cpp`：VM 结果页的数据形态。
12. `include/CompileResult.h`：总结所有 GUI 表格数据都放在这里。

## 6. 老师追问时的标准回答

### 问：GUI 表格数据从哪里来？

答：

```text
从 CompileResult 来。
CompilerFacade 调用 Lexer、Parser、优化器、展示生成器，把结果都放进 CompileResult。
GUI 只负责把 CompileResult 的字段填到表格里。
```

### 问：符号表和类型表区别是什么？

答：

```text
符号表保存名字，比如变量名、函数名、参数名。
类型表保存结构，比如 record 有哪些字段，array 的上下界和元素类型。
```

### 问：四元式在哪里生成？

答：

```text
在 Parser.cpp 里统一通过 emit 生成。
赋值、表达式、if、while、函数调用都会调用 emit。
```

### 问：表达式优先级怎么体现？

答：

```text
parseExpression 处理加减。
parseTerm 处理乘除。
parseFactor 处理变量、常量、括号。
因为 Expression 先调用 Term，所以乘除优先级更高。
```

### 问：优化做了什么？

答：

```text
优化器按基本块处理四元式。
包括复制传播、代数化简和公共子表达式复用。
```

### 问：VM 为什么没展开？

答：

```text
这里重点讲编译前端和 GUI 展示数据来源。
VM 运行结果页只展示结果数据形态。
真实项目里 VM 执行目标指令后会填入最终变量值。
```
