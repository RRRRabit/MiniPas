#include "MainWindow.h"
#include "CompilerFacade.h"
#include "Token.h"
#include <QApplication>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSplitter>
#include <QTextBlock>
#include <QTableWidgetItem>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>
#include <set>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      sourceEdit_(nullptr),
      compileButton_(nullptr),
      clearButton_(nullptr),
      tabWidget_(nullptr),
      statusLabel_(nullptr),
      tokenTable_(nullptr),
      keywordTable_(nullptr),
      delimiterTable_(nullptr),
      identifierTable_(nullptr),
      constantTable_(nullptr),
      symbolTabWidget_(nullptr),
      synblTable_(nullptr),
      typelTable_(nullptr),
      rinflTable_(nullptr),
      ainflTable_(nullptr),
      pfinflTable_(nullptr),
      paramTable_(nullptr),
      conslTable_(nullptr),
      lenlTable_(nullptr),
      vallTable_(nullptr),
      recordTable_(nullptr),
      quadrupleTable_(nullptr),
      optimizedQuadrupleTable_(nullptr),
      runtimeText_(nullptr) {
    buildUi();
}

void MainWindow::buildUi() {
    setWindowTitle("MiniPas+ 可视化编译器前端系统");
    resize(1200, 760);

    QFont uiFont("Microsoft YaHei", 10);
    QApplication::setFont(uiFont);

    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);

    auto* splitter = new QSplitter(Qt::Horizontal, central);

    auto* leftPanel = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftPanel);

    auto* sourceGroup = new QGroupBox("源程序输入区", leftPanel);
    auto* sourceLayout = new QVBoxLayout(sourceGroup);
    sourceEdit_ = new QPlainTextEdit(sourceGroup);
    sourceEdit_->setFont(QFont("Consolas", 11));
    sourceEdit_->setPlainText(
        "PROGRAM demo\n"
        "VAR x, y: INTEGER;\n"
        "BEGIN\n"
        "    x := 0;\n"
        "    y := 5;\n"
        "    WHILE x < y DO\n"
        "    BEGIN\n"
        "        IF x = 2 THEN\n"
        "            y := y - 1\n"
        "        ELSE\n"
        "            y := y + 0;\n"
        "        x := x + 1\n"
        "    END\n"
        "END.");
    sourceLayout->addWidget(sourceEdit_);

    auto* buttonLayout = new QGridLayout();
    compileButton_ = new QPushButton("编译并运行", leftPanel);
    clearButton_ = new QPushButton("清空输出", leftPanel);
    buttonLayout->addWidget(compileButton_, 0, 0);
    buttonLayout->addWidget(clearButton_, 0, 1);

    leftLayout->addWidget(sourceGroup);
    leftLayout->addLayout(buttonLayout);

    tabWidget_ = new QTabWidget(splitter);

    tokenTable_ = new QTableWidget(tabWidget_);
    setupTable(tokenTable_, {"序号", "编码", "类型", "单词", "行号", "列号"});
    tabWidget_->addTab(createTablePage("词法分析阶段生成的 Token 序列", tokenTable_), "Token序列");

    auto* idConstPage = new QWidget(tabWidget_);
    auto* idConstLayout = new QGridLayout(idConstPage);
    keywordTable_ = new QTableWidget(idConstPage);
    delimiterTable_ = new QTableWidget(idConstPage);
    identifierTable_ = new QTableWidget(idConstPage);
    constantTable_ = new QTableWidget(idConstPage);
    setupTable(keywordTable_, {"编号", "关键字"});
    setupTable(delimiterTable_, {"编号", "界符"});
    setupTable(identifierTable_, {"序号", "标识符"});
    setupTable(constantTable_, {"序号", "常数", "类型", "数值"});
    idConstLayout->addWidget(new QLabel("关键字表 K", idConstPage), 0, 0);
    idConstLayout->addWidget(new QLabel("界符表 P", idConstPage), 0, 1);
    idConstLayout->addWidget(keywordTable_, 1, 0);
    idConstLayout->addWidget(delimiterTable_, 1, 1);
    idConstLayout->addWidget(new QLabel("标识符表 I", idConstPage), 2, 0);
    idConstLayout->addWidget(new QLabel("常数表 C", idConstPage), 2, 1);
    idConstLayout->addWidget(identifierTable_, 3, 0);
    idConstLayout->addWidget(constantTable_, 3, 1);
    tabWidget_->addTab(idConstPage, "标识符与常数表");

    tabWidget_->addTab(createSymbolSystemPage(), "符号表");

    recordTable_ = new QTableWidget(tabWidget_);
    setupTable(recordTable_, {"RecordName", "FieldName", "FieldType", "Offset", "Size", "TotalSize"});
    tabWidget_->addTab(createTablePage("record 结构体字段偏移信息", recordTable_), "Record类型表");

    quadrupleTable_ = new QTableWidget(tabWidget_);
    setupTable(quadrupleTable_, {"基本块", "序号", "op", "arg1", "arg2", "result"});
    tabWidget_->addTab(createTablePage("中间代码生成阶段得到的四元式序列", quadrupleTable_), "四元式序列");

    optimizedQuadrupleTable_ = new QTableWidget(tabWidget_);
    setupTable(optimizedQuadrupleTable_, {"基本块", "序号", "op", "arg1", "arg2", "result"});
    tabWidget_->addTab(createTablePage("基本块内 DAG 优化后的四元式序列", optimizedQuadrupleTable_), "优化后四元式");

    runtimeText_ = new QTextEdit(tabWidget_);
    runtimeText_->setReadOnly(true);
    runtimeText_->setFont(QFont("Consolas", 11));
    tabWidget_->addTab(createRuntimePage("四元式解释执行后的最终变量值"), "解释执行结果");

    splitter->addWidget(leftPanel);
    splitter->addWidget(tabWidget_);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    statusLabel_ = new QLabel("就绪", central);
    statusLabel_->setMinimumHeight(28);
    statusLabel_->setStyleSheet("color: #333333; font-weight: 600;");

    rootLayout->addWidget(splitter);
    rootLayout->addWidget(statusLabel_);
    setCentralWidget(central);

    connect(compileButton_, &QPushButton::clicked, this, [this]() { compileAndRun(); });
    connect(clearButton_, &QPushButton::clicked, this, [this]() { clearOutput(); });
    connect(tokenTable_, &QTableWidget::cellClicked, this, [this](int row, int) {
        const QTableWidgetItem* lineItem = tokenTable_->item(row, 4);
        const QTableWidgetItem* colItem = tokenTable_->item(row, 5);
        const QTableWidgetItem* lexemeItem = tokenTable_->item(row, 3);
        if (!lineItem || !colItem || !lexemeItem) {
            return;
        }
        highlightTokenAt(lineItem->text().toInt(), colItem->text().toInt(), lexemeItem->text());
    });
    connect(synblTable_, &QTableWidget::cellClicked, this, [this](int row, int) {
        const QTableWidgetItem* nameItem = synblTable_->item(row, 1);
        if (!nameItem) {
            return;
        }
        highlightWordOccurrences(nameItem->text());
    });
    connect(identifierTable_, &QTableWidget::cellClicked, this, [this](int row, int) {
        const QTableWidgetItem* nameItem = identifierTable_->item(row, 1);
        if (!nameItem) {
            return;
        }
        highlightWordOccurrences(nameItem->text());
    });
}

QWidget* MainWindow::createSymbolSystemPage() {
    auto* page = new QWidget(tabWidget_);
    auto* layout = new QVBoxLayout(page);
    auto* label = new QLabel("语义分析阶段维护的符号表系统：SYNBL / TYPEL / RINFL / AINFL / PFINFL / CONSL / LENL / VALL", page);
    label->setStyleSheet("font-weight: 600; color: #334155; padding: 4px 0;");

    symbolTabWidget_ = new QTabWidget(page);

    synblTable_ = new QTableWidget(symbolTabWidget_);
    setupTable(synblTable_, {"序号", "NAME", "TYPEL", "CAT", "INFO"});
    symbolTabWidget_->addTab(createTablePage("SYNBL 总表：保存标识符及其语义信息", synblTable_), "SYNBL总表");

    typelTable_ = new QTableWidget(symbolTabWidget_);
    setupTable(typelTable_, {"序号", "TYPE", "KIND", "POINT/INFO"});
    symbolTabWidget_->addTab(createTablePage("TYPEL 类型表：保存基本类型和 record 类型入口", typelTable_), "TYPEL类型表");

    rinflTable_ = new QTableWidget(symbolTabWidget_);
    setupTable(rinflTable_, {"序号", "RecordName", "ID", "OFF", "TYPE", "SIZE"});
    symbolTabWidget_->addTab(createTablePage("RINFL 结构表：保存 record 字段名、字段偏移和字段类型", rinflTable_), "RINFL结构表");

    ainflTable_ = new QTableWidget(symbolTabWidget_);
    setupTable(ainflTable_, {"序号", "ArrayName", "LOW", "UP", "CTP", "CLEN"});
    symbolTabWidget_->addTab(createTablePage("AINFL 数组表：保存数组上下界、成分类型和成分长度", ainflTable_), "AINFL数组表");

    pfinflTable_ = new QTableWidget(symbolTabWidget_);
    setupTable(pfinflTable_, {"序号", "Name", "LEVEL", "OFF", "FN", "ENTRY", "PARAM"});
    symbolTabWidget_->addTab(createTablePage("PFINFL 函数表：保存函数层次、参数入口和入口四元式", pfinflTable_), "PFINFL函数表");

    paramTable_ = new QTableWidget(symbolTabWidget_);
    setupTable(paramTable_, {"序号", "Function", "NAME", "TYPEL", "CAT", "INFO"});
    symbolTabWidget_->addTab(createTablePage("PARAM 形参表：表项规则与 SYNBL 保持一致", paramTable_), "PARAM形参表");

    conslTable_ = new QTableWidget(symbolTabWidget_);
    setupTable(conslTable_, {"序号", "Text", "Type", "Value"});
    symbolTabWidget_->addTab(createTablePage("CONSL 常量表：保存常量初值", conslTable_), "CONSL常量表");

    lenlTable_ = new QTableWidget(symbolTabWidget_);
    setupTable(lenlTable_, {"序号", "TYPE", "LEN"});
    symbolTabWidget_->addTab(createTablePage("LENL 长度表：保存各类型占用的字节数", lenlTable_), "LENL长度表");

    vallTable_ = new QTableWidget(symbolTabWidget_);
    setupTable(vallTable_, {"Scope", "Name", "Offset", "Size"});
    symbolTabWidget_->addTab(createTablePage("VALL 活动记录：展示运行时值单元的相对布局", vallTable_), "VALL活动记录");

    layout->addWidget(label);
    layout->addWidget(symbolTabWidget_);
    return page;
}

QWidget* MainWindow::createTablePage(const QString& description, QTableWidget* table) {
    auto* page = new QWidget(tabWidget_);
    auto* layout = new QVBoxLayout(page);
    auto* label = new QLabel(description, page);
    label->setStyleSheet("font-weight: 600; color: #334155; padding: 4px 0;");
    layout->addWidget(label);
    layout->addWidget(table);
    return page;
}

QWidget* MainWindow::createRuntimePage(const QString& description) {
    auto* page = new QWidget(tabWidget_);
    auto* layout = new QVBoxLayout(page);
    auto* label = new QLabel(description, page);
    label->setStyleSheet("font-weight: 600; color: #334155; padding: 4px 0;");
    layout->addWidget(label);
    layout->addWidget(runtimeText_);
    return page;
}

void MainWindow::setupTable(QTableWidget* table, const QStringList& headers) {
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->setFont(QFont("Consolas", 10));
}

void MainWindow::compileAndRun() {
    clearOutput();

    CompilerFacade compiler;
    CompileResult result = compiler.compileAndRun(sourceEdit_->toPlainText().toStdString());

    if (!result.success) {
        fillTokenTable(result);
        fillKeywordAndDelimiterTables(result);
        fillIdentifierAndConstantTables(result);
        QString message = QString::fromStdString(result.errorMessage);
        setStatusError(message);
        tabWidget_->setCurrentIndex(0);
        QMessageBox::critical(this, "编译错误", message);
        return;
    }

    fillAllTables(result);
    tabWidget_->setCurrentIndex(0);
    setStatusSuccess("编译成功");
}

void MainWindow::clearOutput() {
    tokenTable_->setRowCount(0);
    keywordTable_->setRowCount(0);
    delimiterTable_->setRowCount(0);
    identifierTable_->setRowCount(0);
    constantTable_->setRowCount(0);
    synblTable_->setRowCount(0);
    typelTable_->setRowCount(0);
    rinflTable_->setRowCount(0);
    ainflTable_->setRowCount(0);
    pfinflTable_->setRowCount(0);
    paramTable_->setRowCount(0);
    conslTable_->setRowCount(0);
    lenlTable_->setRowCount(0);
    vallTable_->setRowCount(0);
    recordTable_->setRowCount(0);
    quadrupleTable_->setRowCount(0);
    optimizedQuadrupleTable_->setRowCount(0);
    runtimeText_->clear();
    clearSourceHighlight();
    statusLabel_->setText("输出已清空");
    statusLabel_->setStyleSheet("color: #333333; font-weight: 600;");
}

void MainWindow::fillAllTables(const CompileResult& result) {
    fillTokenTable(result);
    fillKeywordAndDelimiterTables(result);
    fillIdentifierAndConstantTables(result);
    fillSymbolTable(result);
    fillRecordTable(result);
    fillQuadrupleTable(result);
    fillOptimizedQuadrupleTable(result);
    fillRuntimeText(result);
}

void MainWindow::fillTokenTable(const CompileResult& result) {
    tokenTable_->setRowCount(static_cast<int>(result.tokens.size()));
    for (int row = 0; row < static_cast<int>(result.tokens.size()); ++row) {
        const Token& token = result.tokens[row];
        tokenTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        tokenTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(tokenCodeToString(token))));
        tokenTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(tokenTypeToString(token.type))));
        tokenTable_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(token.lexeme)));
        tokenTable_->setItem(row, 4, new QTableWidgetItem(QString::number(token.line)));
        tokenTable_->setItem(row, 5, new QTableWidgetItem(QString::number(token.column)));
    }
}

void MainWindow::fillKeywordAndDelimiterTables(const CompileResult& result) {
    keywordTable_->setRowCount(static_cast<int>(result.keywordTable.size()));
    for (int row = 0; row < static_cast<int>(result.keywordTable.size()); ++row) {
        const auto& entry = result.keywordTable[row];
        keywordTable_->setItem(row, 0, new QTableWidgetItem(QString::number(entry.index)));
        keywordTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(entry.word)));
    }

    delimiterTable_->setRowCount(static_cast<int>(result.delimiterTable.size()));
    for (int row = 0; row < static_cast<int>(result.delimiterTable.size()); ++row) {
        const auto& entry = result.delimiterTable[row];
        delimiterTable_->setItem(row, 0, new QTableWidgetItem(QString::number(entry.index)));
        delimiterTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(entry.symbol)));
    }
}

void MainWindow::fillIdentifierAndConstantTables(const CompileResult& result) {
    identifierTable_->setRowCount(static_cast<int>(result.identifierTable.size()));
    for (int row = 0; row < static_cast<int>(result.identifierTable.size()); ++row) {
        identifierTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        identifierTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(result.identifierTable[row])));
    }

    constantTable_->setRowCount(static_cast<int>(result.constantEntries.size()));
    for (int row = 0; row < static_cast<int>(result.constantEntries.size()); ++row) {
        const auto& entry = result.constantEntries[row];
        constantTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        constantTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(entry.text)));
        constantTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(entry.type)));
        constantTable_->setItem(row, 3, new QTableWidgetItem(QString::number(entry.numberValue, 'g', 12)));
    }
}

void MainWindow::fillSymbolTable(const CompileResult& result) {
    fillSynblTable(result);
    fillTypelTable(result);
    fillRinflTable(result);
    fillAinflTable(result);
    fillPfinflTable(result);
    fillParamTable(result);
    fillConslTable(result);
    fillLenlTable(result);
    fillVallTable(result);
}

void MainWindow::fillSynblTable(const CompileResult& result) {
    synblTable_->setRowCount(static_cast<int>(result.symbols.size()));
    for (int row = 0; row < static_cast<int>(result.symbols.size()); ++row) {
        const Symbol& symbol = result.symbols[row];
        synblTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        synblTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(symbol.name)));
        synblTable_->setItem(row, 2, new QTableWidgetItem(synblTypeRef(symbol)));
        QString cat = synblCategory(symbol);
        synblTable_->setItem(row, 3, new QTableWidgetItem(cat));

        QString info = "_";
        if (cat == "vn" || cat == "vt" || cat == "v" || cat == "tv") {
            info = QString("(%1,%2)").arg(symbolLevel(symbol)).arg(symbol.address >= 0 ? symbol.address : 0);
        } else if (cat == "t" || cat == "d") {
            info = lenlPointer(symbol.typeName, result);
        } else if (cat == "f" || cat == "p") {
            info = "PFINFL";
        }
        synblTable_->setItem(row, 4, new QTableWidgetItem(info));
    }
}

void MainWindow::fillTypelTable(const CompileResult& result) {
    typelTable_->setRowCount(1 + static_cast<int>(result.recordTypes.size()) + static_cast<int>(result.arrayTypes.size()));

    int row = 0;
    typelTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
    typelTable_->setItem(row, 1, new QTableWidgetItem("ircb"));
    typelTable_->setItem(row, 2, new QTableWidgetItem("basic"));
    typelTable_->setItem(row, 3, new QTableWidgetItem(""));
    ++row;

    int rinflStart = 0;
    for (const auto& record : result.recordTypes) {
        typelTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        typelTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(record.name)));
        typelTable_->setItem(row, 2, new QTableWidgetItem("record"));
        typelTable_->setItem(row, 3, new QTableWidgetItem(QString("RINFL+%1").arg(rinflStart)));
        rinflStart += static_cast<int>(record.fields.size());
        ++row;
    }

    for (int i = 0; i < static_cast<int>(result.arrayTypes.size()); ++i) {
        const auto& array = result.arrayTypes[i];
        typelTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        typelTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(array.name)));
        typelTable_->setItem(row, 2, new QTableWidgetItem("array"));
        typelTable_->setItem(row, 3, new QTableWidgetItem(QString("AINFL+%1").arg(i)));
        ++row;
    }
}

void MainWindow::fillRinflTable(const CompileResult& result) {
    int rowCount = 0;
    for (const auto& record : result.recordTypes) {
        rowCount += static_cast<int>(record.fields.size());
    }

    rinflTable_->setRowCount(rowCount);
    int row = 0;
    for (const auto& record : result.recordTypes) {
        for (const auto& field : record.fields) {
            rinflTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
            rinflTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(record.name)));
            rinflTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(field.name)));
            rinflTable_->setItem(row, 3, new QTableWidgetItem(QString::number(field.offset)));
            rinflTable_->setItem(row, 4, new QTableWidgetItem(typeRef(field.type)));
            rinflTable_->setItem(row, 5, new QTableWidgetItem(QString::number(field.size)));
            ++row;
        }
    }
}

void MainWindow::fillAinflTable(const CompileResult& result) {
    ainflTable_->setRowCount(static_cast<int>(result.arrayTypes.size()));
    for (int row = 0; row < static_cast<int>(result.arrayTypes.size()); ++row) {
        const auto& array = result.arrayTypes[row];
        ainflTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        ainflTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(array.name)));
        ainflTable_->setItem(row, 2, new QTableWidgetItem(QString::number(array.low)));
        ainflTable_->setItem(row, 3, new QTableWidgetItem(QString::number(array.high)));
        ainflTable_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(array.elementType)));
        ainflTable_->setItem(row, 5, new QTableWidgetItem(QString::number(array.elementSize)));
    }
}

void MainWindow::fillPfinflTable(const CompileResult& result) {
    pfinflTable_->setRowCount(static_cast<int>(result.functionTable.size()));
    for (int row = 0; row < static_cast<int>(result.functionTable.size()); ++row) {
        const auto& function = result.functionTable[row];
        pfinflTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        pfinflTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(function.name)));
        pfinflTable_->setItem(row, 2, new QTableWidgetItem(QString::number(function.level)));
        pfinflTable_->setItem(row, 3, new QTableWidgetItem(QString::number(function.offset)));
        pfinflTable_->setItem(row, 4, new QTableWidgetItem(QString::number(function.paramCount)));
        pfinflTable_->setItem(row, 5, new QTableWidgetItem(QString::number(function.entryQuad)));
        pfinflTable_->setItem(row, 6, new QTableWidgetItem(QString("PARAM+%1").arg(function.paramStart)));
    }
}

void MainWindow::fillParamTable(const CompileResult& result) {
    paramTable_->setRowCount(static_cast<int>(result.parameterTable.size()));
    for (int row = 0; row < static_cast<int>(result.parameterTable.size()); ++row) {
        const auto& param = result.parameterTable[row];
        paramTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        paramTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(param.functionName)));
        paramTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(param.name)));
        Symbol pseudo;
        pseudo.name = param.name;
        pseudo.typeName = param.type;
        pseudo.kind = (param.passMode == "vn") ? "var parameter" : "parameter";
        pseudo.address = param.offset;
        pseudo.size = param.size;

        paramTable_->setItem(row, 3, new QTableWidgetItem(synblTypeRef(pseudo)));
        QString cat = synblCategory(pseudo);
        paramTable_->setItem(row, 4, new QTableWidgetItem(cat));
        paramTable_->setItem(row, 5, new QTableWidgetItem(QString("(%1,%2)").arg(2).arg(param.offset)));
    }
}

void MainWindow::fillConslTable(const CompileResult& result) {
    conslTable_->setRowCount(static_cast<int>(result.constantEntries.size()));
    for (int row = 0; row < static_cast<int>(result.constantEntries.size()); ++row) {
        const auto& entry = result.constantEntries[row];
        conslTable_->setItem(row, 0, new QTableWidgetItem(QString::number(entry.index)));
        conslTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(entry.text)));
        conslTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(entry.type)));
        conslTable_->setItem(row, 3, new QTableWidgetItem(QString::number(entry.numberValue, 'g', 12)));
    }
}

void MainWindow::fillLenlTable(const CompileResult& result) {
    lenlTable_->setRowCount(4 + static_cast<int>(result.recordTypes.size()) + static_cast<int>(result.arrayTypes.size()));

    const QString basicTypes[4][2] = {
        {"integer", "4"},
        {"real", "8"},
        {"char", "1"},
        {"boolean", "1"}
    };

    int row = 0;
    for (; row < 4; ++row) {
        lenlTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        lenlTable_->setItem(row, 1, new QTableWidgetItem(basicTypes[row][0]));
        lenlTable_->setItem(row, 2, new QTableWidgetItem(basicTypes[row][1]));
    }

    for (const auto& record : result.recordTypes) {
        lenlTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        lenlTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(record.name)));
        lenlTable_->setItem(row, 2, new QTableWidgetItem(QString::number(record.totalSize)));
        ++row;
    }

    for (const auto& array : result.arrayTypes) {
        lenlTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        lenlTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(array.name)));
        lenlTable_->setItem(row, 2, new QTableWidgetItem(QString::number(array.totalSize)));
        ++row;
    }
}

void MainWindow::fillVallTable(const CompileResult& result) {
    int extraSeparators = 0;
    std::string lastScope;
    for (const auto& item : result.activationRecords) {
        if (!lastScope.empty() && item.scope != lastScope) {
            ++extraSeparators;
        }
        lastScope = item.scope;
    }

    vallTable_->setRowCount(static_cast<int>(result.activationRecords.size()) + extraSeparators);

    int row = 0;
    lastScope.clear();
    for (const auto& item : result.activationRecords) {
        if (!lastScope.empty() && item.scope != lastScope) {
            vallTable_->setItem(row, 0, new QTableWidgetItem("----"));
            vallTable_->setItem(row, 1, new QTableWidgetItem("----"));
            vallTable_->setItem(row, 2, new QTableWidgetItem("----"));
            vallTable_->setItem(row, 3, new QTableWidgetItem("----"));
            ++row;
        }

        vallTable_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(item.scope)));
        vallTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(item.name)));
        vallTable_->setItem(row, 2, new QTableWidgetItem(QString::number(item.offset)));
        vallTable_->setItem(row, 3, new QTableWidgetItem(QString::number(item.size)));
        lastScope = item.scope;
        ++row;
    }
}

void MainWindow::fillRecordTable(const CompileResult& result) {
    int rowCount = 0;
    for (const auto& record : result.recordTypes) {
        rowCount += static_cast<int>(record.fields.size());
    }

    recordTable_->setRowCount(rowCount);
    int row = 0;
    for (const auto& record : result.recordTypes) {
        for (const auto& field : record.fields) {
            recordTable_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(record.name)));
            recordTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(field.name)));
            recordTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(field.type)));
            recordTable_->setItem(row, 3, new QTableWidgetItem(QString::number(field.offset)));
            recordTable_->setItem(row, 4, new QTableWidgetItem(QString::number(field.size)));
            recordTable_->setItem(row, 5, new QTableWidgetItem(QString::number(record.totalSize)));
            ++row;
        }
    }
}

void MainWindow::fillQuadrupleTable(const CompileResult& result) {
    quadrupleTable_->setRowCount(static_cast<int>(result.quadruples.size()));

    std::vector<int> quadToBlock(result.quadruples.size(), -1);
    for (const auto& block : result.basicBlocks) {
        int begin = std::max(0, block.start);
        int end = std::min(static_cast<int>(result.quadruples.size()) - 1, block.end);
        for (int i = begin; i <= end; ++i) {
            quadToBlock[i] = block.id;
        }
    }

    for (int row = 0; row < static_cast<int>(result.quadruples.size()); ++row) {
        const Quadruple& quad = result.quadruples[row];
        QString blockLabel = (row < static_cast<int>(quadToBlock.size()) && quadToBlock[row] >= 0)
            ? QString("B%1").arg(quadToBlock[row] + 1)
            : "_";
        quadrupleTable_->setItem(row, 0, new QTableWidgetItem(blockLabel));
        quadrupleTable_->setItem(row, 1, new QTableWidgetItem(QString::number(row)));
        quadrupleTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(quad.op)));
        quadrupleTable_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(quad.arg1)));
        quadrupleTable_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(quad.arg2)));
        quadrupleTable_->setItem(row, 5, new QTableWidgetItem(QString::fromStdString(quad.result)));
    }
}

void MainWindow::fillRuntimeText(const CompileResult& result) {
    QString text;
    for (const auto& item : result.runtimeValues) {
        text += QString::fromStdString(item.first)
            + " = "
            + QString::number(item.second, 'g', 12)
            + "\n";
    }
    runtimeText_->setPlainText(text);
}

void MainWindow::fillOptimizedQuadrupleTable(const CompileResult& result) {
    optimizedQuadrupleTable_->setRowCount(static_cast<int>(result.optimizedQuadruples.size()));
    std::vector<int> quadToBlock(result.optimizedQuadruples.size(), -1);
    if (!result.optimizedQuadruples.empty()) {
        std::set<int> leaders;
        leaders.insert(0);
        struct IfCtx {
            int ifIndex = -1;
            int elIndex = -1;
        };
        std::map<int, int> ifToIe;
        std::map<int, int> ifToEl;
        std::vector<IfCtx> ifStack;
        std::vector<int> whStack;
        struct DoCtx {
            int doIndex = -1;
            int whIndex = -1;
        };
        std::vector<DoCtx> doStack;
        std::map<int, int> doToWe;
        std::map<int, int> weToWh;

        for (int i = 0; i < static_cast<int>(result.optimizedQuadruples.size()); ++i) {
            const Quadruple& q = result.optimizedQuadruples[i];
            if (q.op == "if") {
                ifStack.push_back({i, -1});
            } else if (q.op == "el") {
                if (!ifStack.empty()) {
                    ifStack.back().elIndex = i;
                }
            } else if (q.op == "ie") {
                if (!ifStack.empty()) {
                    ifToIe[ifStack.back().ifIndex] = i;
                    ifToEl[ifStack.back().ifIndex] = ifStack.back().elIndex;
                    ifStack.pop_back();
                }
            } else if (q.op == "wh") {
                whStack.push_back(i);
            } else if (q.op == "do") {
                int whIndex = whStack.empty() ? -1 : whStack.back();
                doStack.push_back({i, whIndex});
            } else if (q.op == "we") {
                if (!doStack.empty()) {
                    DoCtx ctx = doStack.back();
                    doStack.pop_back();
                    doToWe[ctx.doIndex] = i;
                    if (ctx.whIndex >= 0) {
                        weToWh[i] = ctx.whIndex;
                    }
                }
                if (!whStack.empty()) {
                    whStack.pop_back();
                }
            }
        }

        for (int i = 0; i < static_cast<int>(result.optimizedQuadruples.size()); ++i) {
            const Quadruple& q = result.optimizedQuadruples[i];
            if (q.op == "if") {
                if (i + 1 < static_cast<int>(result.optimizedQuadruples.size())) {
                    leaders.insert(i + 1);
                }
                int falseTarget = -1;
                auto e = ifToEl.find(i);
                if (e != ifToEl.end() && e->second >= 0) {
                    falseTarget = e->second + 1;
                } else {
                    auto f = ifToIe.find(i);
                    if (f != ifToIe.end()) {
                        falseTarget = f->second;
                    }
                }
                if (falseTarget >= 0 && falseTarget < static_cast<int>(result.optimizedQuadruples.size())) {
                    leaders.insert(falseTarget);
                }
            } else if (q.op == "do") {
                if (i + 1 < static_cast<int>(result.optimizedQuadruples.size())) {
                    leaders.insert(i + 1);
                }
                auto f = doToWe.find(i);
                if (f != doToWe.end()) {
                    int falseTarget = f->second + 1;
                    if (falseTarget < static_cast<int>(result.optimizedQuadruples.size())) {
                        leaders.insert(falseTarget);
                    }
                }
            } else if (q.op == "el") {
                for (const auto& pair : ifToEl) {
                    if (pair.second == i) {
                        auto ie = ifToIe.find(pair.first);
                        if (ie != ifToIe.end()) {
                            leaders.insert(ie->second);
                        }
                        break;
                    }
                }
            } else if (q.op == "we") {
                auto w = weToWh.find(i);
                if (w != weToWh.end()) {
                    int condEntry = w->second + 1;
                    if (condEntry < static_cast<int>(result.optimizedQuadruples.size())) {
                        leaders.insert(condEntry);
                    }
                }
                if (i + 1 < static_cast<int>(result.optimizedQuadruples.size())) {
                    leaders.insert(i + 1);
                }
            }
        }

        std::vector<int> starts(leaders.begin(), leaders.end());
        for (int bi = 0; bi < static_cast<int>(starts.size()); ++bi) {
            int begin = starts[bi];
            int end = (bi + 1 < static_cast<int>(starts.size()))
                ? starts[bi + 1] - 1
                : static_cast<int>(result.optimizedQuadruples.size()) - 1;
            for (int i = begin; i <= end; ++i) {
                quadToBlock[i] = bi;
            }
        }
    }

    for (int row = 0; row < static_cast<int>(result.optimizedQuadruples.size()); ++row) {
        const Quadruple& quad = result.optimizedQuadruples[row];
        QString blockLabel = (row < static_cast<int>(quadToBlock.size()) && quadToBlock[row] >= 0)
            ? QString("B%1").arg(quadToBlock[row] + 1)
            : "_";
        optimizedQuadrupleTable_->setItem(row, 0, new QTableWidgetItem(blockLabel));
        optimizedQuadrupleTable_->setItem(row, 1, new QTableWidgetItem(QString::number(row)));
        optimizedQuadrupleTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(quad.op)));
        optimizedQuadrupleTable_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(quad.arg1)));
        optimizedQuadrupleTable_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(quad.arg2)));
        optimizedQuadrupleTable_->setItem(row, 5, new QTableWidgetItem(QString::fromStdString(quad.result)));
    }
}

void MainWindow::setStatusSuccess(const QString& message) {
    statusLabel_->setText(message);
    statusLabel_->setStyleSheet("color: #16833a; font-weight: 700;");
}

void MainWindow::setStatusError(const QString& message) {
    statusLabel_->setText(message);
    statusLabel_->setStyleSheet("color: #c62828; font-weight: 700;");
}

QString MainWindow::typeRef(const std::string& typeName) const {
    if (typeName == "integer") {
        return "itp";
    }
    if (typeName == "real") {
        return "rtp";
    }
    if (typeName == "char") {
        return "ctp";
    }
    if (typeName == "boolean") {
        return "btp";
    }
    return QString::fromStdString(typeName);
}

QString MainWindow::synblTypeRef(const Symbol& symbol) const {
    if (symbol.typeName == "integer" || symbol.typeName == "real"
        || symbol.typeName == "char" || symbol.typeName == "boolean") {
        return typeRef(symbol.typeName);
    }
    if (symbol.kind == "type") {
        if (symbol.size == 0) {
            return "_";
        }
        return QString::fromStdString(symbol.name);
    }
    return QString::fromStdString(symbol.typeName);
}

QString MainWindow::synblCategory(const Symbol& symbol) const {
    if (symbol.kind == "program") {
        return "p";
    }
    if (symbol.kind == "type") {
        return "t";
    }
    if (symbol.kind == "function") {
        return "f";
    }
    if (symbol.kind.find("parameter") != std::string::npos) {
        return symbol.kind.find("var parameter") != std::string::npos ? "vn" : "vt";
    }
    if (symbol.kind == "type") {
        return (symbol.typeName == symbol.name) ? "d" : "t";
    }
    if (symbol.kind.find("record variable") != std::string::npos
        || symbol.kind.find("array variable") != std::string::npos
        || symbol.kind.find("variable") != std::string::npos) {
        return "v";
    }
    if (symbol.kind == "temporary") {
        return "tv";
    }
    return QString::fromStdString(symbol.kind);
}

int MainWindow::symbolLevel(const Symbol& symbol) const {
    if (symbol.kind.find(" of ") != std::string::npos) {
        return 2;
    }
    return 1;
}

QString MainWindow::lenlPointer(const std::string& typeName, const CompileResult& result) const {
    int idx = -1;
    if (typeName == "integer") {
        idx = 0;
    } else if (typeName == "real") {
        idx = 1;
    } else if (typeName == "char") {
        idx = 2;
    } else if (typeName == "boolean") {
        idx = 3;
    } else {
        int base = 4;
        for (int i = 0; i < static_cast<int>(result.recordTypes.size()); ++i) {
            if (result.recordTypes[i].name == typeName) {
                idx = base + i;
                break;
            }
        }
        if (idx == -1) {
            base += static_cast<int>(result.recordTypes.size());
            for (int i = 0; i < static_cast<int>(result.arrayTypes.size()); ++i) {
                if (result.arrayTypes[i].name == typeName) {
                    idx = base + i;
                    break;
                }
            }
        }
    }
    return idx >= 0 ? QString("LENL+%1").arg(idx) : "LENL";
}

int MainWindow::positionFromLineColumn(int line, int column) const {
    if (line <= 0 || column <= 0) {
        return -1;
    }
    QTextDocument* doc = sourceEdit_->document();
    QTextBlock block = doc->findBlockByNumber(line - 1);
    if (!block.isValid()) {
        return -1;
    }
    int start = block.position();
    int length = block.length() - 1;
    int colZero = column - 1;
    if (colZero > length) {
        colZero = length;
    }
    return start + colZero;
}

void MainWindow::highlightTokenAt(int line, int column, const QString& lexeme) {
    clearSourceHighlight();
    int pos = positionFromLineColumn(line, column);
    if (pos < 0) {
        return;
    }

    QTextCursor cursor(sourceEdit_->document());
    cursor.setPosition(pos);
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, lexeme.size());

    QTextEdit::ExtraSelection sel;
    sel.cursor = cursor;
    sel.format.setBackground(QColor("#ffe082"));
    sel.format.setForeground(QColor("#111827"));
    sourceEdit_->setExtraSelections({sel});
    sourceEdit_->setTextCursor(cursor);
}

void MainWindow::highlightWordOccurrences(const QString& word) {
    clearSourceHighlight();
    if (word.isEmpty()) {
        return;
    }

    QList<QTextEdit::ExtraSelection> selections;
    QTextDocument* doc = sourceEdit_->document();
    QTextCursor cursor(doc);
    QRegularExpression re(QString("\\b%1\\b").arg(QRegularExpression::escape(word)));

    while (!cursor.isNull() && !cursor.atEnd()) {
        cursor = doc->find(re, cursor);
        if (!cursor.isNull()) {
            QTextEdit::ExtraSelection sel;
            sel.cursor = cursor;
            sel.format.setBackground(QColor("#c8e6c9"));
            sel.format.setForeground(QColor("#0f172a"));
            selections.append(sel);
        }
    }

    sourceEdit_->setExtraSelections(selections);
    if (!selections.isEmpty()) {
        sourceEdit_->setTextCursor(selections.first().cursor);
    }
}

void MainWindow::clearSourceHighlight() {
    sourceEdit_->setExtraSelections({});
}
