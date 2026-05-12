#ifndef MINIPASPLUS_COMPILERESULT_H
#define MINIPASPLUS_COMPILERESULT_H

#include "Quadruple.h"
#include "SymbolTable.h"
#include "Token.h"
#include "TypeTable.h"
#include <map>
#include <string>
#include <vector>

// 编译结果：把前端各阶段产生的数据统一打包，方便控制台或 GUI 展示。
struct CompileResult {
    bool success = false;
    std::string errorMessage;
    std::vector<Token> tokens;
    std::vector<std::string> identifierTable;
    std::vector<std::string> constantTable;
    std::vector<Symbol> symbols;
    std::vector<RecordType> recordTypes;
    std::vector<Quadruple> quadruples;
    std::map<std::string, double> runtimeValues;
};

#endif
