#include "MainWindow.h"
#include "CompilerFacade.h"
#include "Token.h"
#include <QApplication>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFrame>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSplitter>
#include <QTextBlock>
#include <QTableWidgetItem>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>
#include <map>
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
      quadrupleOptimizeTable_(nullptr),
      targetCodeTable_(nullptr),
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
    auto* leftSplitter = new QSplitter(Qt::Vertical, leftPanel);

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

    auto* buttonArea = new QWidget(leftPanel);
    auto* buttonLayout = new QGridLayout(buttonArea);
    compileButton_ = new QPushButton("编译并运行", buttonArea);
    clearButton_ = new QPushButton("清空输出", buttonArea);
    buttonLayout->addWidget(compileButton_, 0, 0);
    buttonLayout->addWidget(clearButton_, 0, 1);

    leftSplitter->addWidget(sourceGroup);
    leftSplitter->addWidget(buttonArea);
    leftSplitter->setStretchFactor(0, 20);
    leftSplitter->setStretchFactor(1, 1);
    leftSplitter->setSizes({620, 80});
    leftSplitter->setCollapsible(1, false);
    leftLayout->addWidget(leftSplitter);

    tabWidget_ = new QTabWidget(splitter);

    auto* lexPage = new QWidget(tabWidget_);
    auto* lexRoot = new QVBoxLayout(lexPage);
    auto* lexTitle = new QLabel("词法分析", lexPage);
    lexTitle->setStyleSheet("font-weight: 700; color: #334155; padding: 4px 0;");
    lexRoot->addWidget(lexTitle);

    auto* lexLayout = new QHBoxLayout();

    auto* tokenPanel = new QWidget(lexPage);
    auto* tokenLayout = new QVBoxLayout(tokenPanel);
    tokenTable_ = new QTableWidget(tokenPanel);
    setupTable(tokenTable_, {"序号", "编码"});
    tokenTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tokenTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    tokenTable_->setColumnWidth(0, 48);
    tokenLayout->addWidget(new QLabel("Token序列表", tokenPanel));
    tokenLayout->addWidget(tokenTable_);

    auto* rightPanel = new QWidget(lexPage);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    keywordTable_ = new QTableWidget(rightPanel);
    delimiterTable_ = new QTableWidget(rightPanel);
    identifierTable_ = new QTableWidget(rightPanel);
    constantTable_ = new QTableWidget(rightPanel);
    setupTable(keywordTable_, {"编号", "关键字"});
    setupTable(delimiterTable_, {"编号", "界符"});
    setupTable(identifierTable_, {"序号", "标识符"});
    setupTable(constantTable_, {"序号", "常数", "类型"});
    rightLayout->addWidget(new QLabel("标识符表 I", rightPanel));
    rightLayout->addWidget(identifierTable_);
    rightLayout->addWidget(new QLabel("常数表 C", rightPanel));
    rightLayout->addWidget(constantTable_);
    rightLayout->addWidget(new QLabel("关键字表 K", rightPanel));
    rightLayout->addWidget(keywordTable_);
    rightLayout->addWidget(new QLabel("界符表 P", rightPanel));
    rightLayout->addWidget(delimiterTable_);

    lexLayout->addWidget(tokenPanel, 1);
    lexLayout->addWidget(rightPanel, 2);
    lexRoot->addLayout(lexLayout);
    tabWidget_->addTab(lexPage, "词法分析");

    tabWidget_->addTab(createSymbolSystemPage(), "符号表");

    quadrupleOptimizeTable_ = new QTableWidget(tabWidget_);
    setupTable(quadrupleOptimizeTable_, {"基本块", "优化前代码", "优化后代码"});
    quadrupleOptimizeTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    quadrupleOptimizeTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    quadrupleOptimizeTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    quadrupleOptimizeTable_->setColumnWidth(0, 70);
    quadrupleOptimizeTable_->setWordWrap(true);
    quadrupleOptimizeTable_->verticalHeader()->setDefaultSectionSize(24);
    tabWidget_->addTab(createTablePage("按基本块对照展示优化前/优化后四元式", quadrupleOptimizeTable_), "中间代码及优化");

    targetCodeTable_ = new QTableWidget(tabWidget_);
    setupTable(targetCodeTable_, {"基本块", "四元式", "目标代码", "RDL", "SEM"});
    targetCodeTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    targetCodeTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    targetCodeTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    targetCodeTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    targetCodeTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    targetCodeTable_->setColumnWidth(1, 240);
    tabWidget_->addTab(createTablePage("目标代码生成过程跟踪（第9章格式）", targetCodeTable_), "目标代码");

    runtimeText_ = new QTextEdit(tabWidget_);
    runtimeText_->setReadOnly(true);
    runtimeText_->setFont(QFont("Consolas", 11));
    tabWidget_->addTab(createRuntimePage("四元式解释执行后的最终变量值"), "解释执行结果");

    splitter->addWidget(leftPanel);
    splitter->addWidget(tabWidget_);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    auto* statusPanel = new QFrame(central);
    statusPanel->setFrameShape(QFrame::StyledPanel);
    statusPanel->setFrameShadow(QFrame::Plain);
    auto* statusLayout = new QVBoxLayout(statusPanel);
    statusLayout->setContentsMargins(8, 2, 8, 2);
    statusLayout->setSpacing(0);
    statusLabel_ = new QLabel("就绪", statusPanel);
    statusLabel_->setStyleSheet("color: #333333; font-weight: 600;");
    statusLabel_->setMinimumHeight(20);
    statusLabel_->setMaximumHeight(24);
    statusLayout->addWidget(statusLabel_);

    auto* verticalSplitter = new QSplitter(Qt::Vertical, central);
    verticalSplitter->addWidget(splitter);
    verticalSplitter->addWidget(statusPanel);
    verticalSplitter->setStretchFactor(0, 20);
    verticalSplitter->setStretchFactor(1, 1);
    verticalSplitter->setSizes({720, 36});
    verticalSplitter->setCollapsible(1, true);

    rootLayout->addWidget(verticalSplitter);
    setCentralWidget(central);

    connect(compileButton_, &QPushButton::clicked, this, [this]() { compileAndRun(); });
    connect(clearButton_, &QPushButton::clicked, this, [this]() { clearOutput(); });
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
    table->verticalHeader()->setDefaultSectionSize(22);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->setFont(QFont("Microsoft YaHei", 10));
    table->setStyleSheet("QTableView::item { padding: 2px 4px; }");
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
    quadrupleOptimizeTable_->setRowCount(0);
    targetCodeTable_->setRowCount(0);
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
    fillQuadrupleOptimizeTable(result);
    fillTargetCodeTable(result);
    fillRuntimeText(result);
}

void MainWindow::fillTokenTable(const CompileResult& result) {
    tokenTable_->setRowCount(static_cast<int>(result.tokens.size()));
    for (int row = 0; row < static_cast<int>(result.tokens.size()); ++row) {
        const Token& token = result.tokens[row];
        tokenTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        tokenTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(tokenCodeToString(token))));
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

void MainWindow::fillQuadrupleOptimizeTable(const CompileResult& result) {
    auto buildBlocks = [](const std::vector<Quadruple>& quads) {
        std::vector<BasicBlock> blocks;
        if (quads.empty()) {
            return blocks;
        }
        std::set<int> leaders;
        leaders.insert(0);
        struct IfCtx {
            int ifIndex = -1;
            int elIndex = -1;
        };
        struct DoCtx {
            int doIndex = -1;
            int whIndex = -1;
        };
        std::map<int, int> ifToIe;
        std::map<int, int> ifToEl;
        std::vector<IfCtx> ifStack;
        std::vector<int> whStack;
        std::vector<DoCtx> doStack;
        std::map<int, int> doToWe;
        std::map<int, int> weToWh;

        for (int i = 0; i < static_cast<int>(quads.size()); ++i) {
            const Quadruple& q = quads[i];
            if (q.op == "if") {
                ifStack.push_back({i, -1});
            } else if (q.op == "el") {
                if (!ifStack.empty()) ifStack.back().elIndex = i;
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
                    if (ctx.whIndex >= 0) weToWh[i] = ctx.whIndex;
                }
                if (!whStack.empty()) whStack.pop_back();
            }
        }

        for (int i = 0; i < static_cast<int>(quads.size()); ++i) {
            const Quadruple& q = quads[i];
            if (q.op == "if") {
                if (i + 1 < static_cast<int>(quads.size())) leaders.insert(i + 1);
                int falseTarget = -1;
                auto e = ifToEl.find(i);
                if (e != ifToEl.end() && e->second >= 0) falseTarget = e->second + 1;
                else {
                    auto f = ifToIe.find(i);
                    if (f != ifToIe.end()) falseTarget = f->second;
                }
                if (falseTarget >= 0 && falseTarget < static_cast<int>(quads.size())) leaders.insert(falseTarget);
            } else if (q.op == "do") {
                if (i + 1 < static_cast<int>(quads.size())) leaders.insert(i + 1);
                auto f = doToWe.find(i);
                if (f != doToWe.end()) {
                    int falseTarget = f->second + 1;
                    if (falseTarget < static_cast<int>(quads.size())) leaders.insert(falseTarget);
                }
            } else if (q.op == "el") {
                for (const auto& pair : ifToEl) {
                    if (pair.second == i) {
                        auto ie = ifToIe.find(pair.first);
                        if (ie != ifToIe.end()) leaders.insert(ie->second);
                        break;
                    }
                }
            } else if (q.op == "we") {
                auto w = weToWh.find(i);
                if (w != weToWh.end()) {
                    int condEntry = w->second + 1;
                    if (condEntry < static_cast<int>(quads.size())) leaders.insert(condEntry);
                }
                if (i + 1 < static_cast<int>(quads.size())) leaders.insert(i + 1);
            }
        }

        std::vector<int> starts(leaders.begin(), leaders.end());
        std::sort(starts.begin(), starts.end());
        for (int i = 0; i < static_cast<int>(starts.size()); ++i) {
            int next = (i + 1 < static_cast<int>(starts.size())) ? starts[i + 1] : static_cast<int>(quads.size());
            blocks.push_back({i, starts[i], next - 1});
        }
        return blocks;
    };

    const std::vector<BasicBlock> optimizedBlocks = buildBlocks(result.optimizedQuadruples);
    int count = std::max(static_cast<int>(result.basicBlocks.size()), static_cast<int>(optimizedBlocks.size()));
    quadrupleOptimizeTable_->setRowCount(count);
    for (int bi = 0; bi < count; ++bi) {
        quadrupleOptimizeTable_->setItem(bi, 0, new QTableWidgetItem(QString("B%1").arg(bi + 1)));

        QString beforeText;
        int beforeLines = 1;
        if (bi < static_cast<int>(result.basicBlocks.size())) {
            const BasicBlock& b = result.basicBlocks[bi];
            for (int i = b.start; i <= b.end && i < static_cast<int>(result.quadruples.size()); ++i) {
                const Quadruple& q = result.quadruples[i];
                if (!beforeText.isEmpty()) {
                    beforeText += "\n";
                    ++beforeLines;
                }
                beforeText += QString("%1: (%2, %3, %4, %5)")
                    .arg(i).arg(QString::fromStdString(q.op)).arg(QString::fromStdString(q.arg1))
                    .arg(QString::fromStdString(q.arg2)).arg(QString::fromStdString(q.result));
            }
        } else {
            beforeText = "-";
        }
        QTableWidgetItem* beforeItem = new QTableWidgetItem(beforeText);
        quadrupleOptimizeTable_->setItem(bi, 1, beforeItem);

        QString afterText;
        int afterLines = 1;
        if (bi < static_cast<int>(optimizedBlocks.size())) {
            const BasicBlock& b = optimizedBlocks[bi];
            for (int i = b.start; i <= b.end && i < static_cast<int>(result.optimizedQuadruples.size()); ++i) {
                const Quadruple& q = result.optimizedQuadruples[i];
                if (!afterText.isEmpty()) {
                    afterText += "\n";
                    ++afterLines;
                }
                afterText += QString("%1: (%2, %3, %4, %5)")
                    .arg(i).arg(QString::fromStdString(q.op)).arg(QString::fromStdString(q.arg1))
                    .arg(QString::fromStdString(q.arg2)).arg(QString::fromStdString(q.result));
            }
        } else {
            afterText = "-";
        }
        QTableWidgetItem* afterItem = new QTableWidgetItem(afterText);
        quadrupleOptimizeTable_->setItem(bi, 2, afterItem);

        int maxLines = std::max(beforeLines, afterLines);
        int rowHeight = std::max(24, maxLines * 18 + 6);
        quadrupleOptimizeTable_->setRowHeight(bi, rowHeight);
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


void MainWindow::fillTargetCodeTable(const CompileResult& result) {
    targetCodeTable_->setRowCount(static_cast<int>(result.targetTrace.size()));
    for (int row = 0; row < static_cast<int>(result.targetTrace.size()); ++row) {
        const TargetTraceItem& t = result.targetTrace[row];
        targetCodeTable_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(t.basicBlock)));
        targetCodeTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(t.quadruple)));
        targetCodeTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(t.targetCode)));
        targetCodeTable_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(t.rdl)));
        targetCodeTable_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(t.sem)));
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
