#include "MainWindow.h"
#include "CompilerFacade.h"
#include "Token.h"
#include <QApplication>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFrame>
#include <QFile>
#include <QFileDialog>
#include <QColor>
#include <QBrush>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSplitter>
#include <QTextBlock>
#include <QTableWidgetItem>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextStream>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSizePolicy>
#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <vector>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      sourceEdit_(nullptr),
      exampleCombo_(nullptr),
      openFileButton_(nullptr),
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
      pfinflParamTable_(nullptr),
      pfinflContainer_(nullptr),
      pfinflLayout_(nullptr),
      conslTable_(nullptr),
      lenlTable_(nullptr),
      vallTable_(nullptr),
      rawQuadrupleTable_(nullptr),
      quadrupleOptimizeTable_(nullptr),
      targetCodeTable_(nullptr),
      vmResultTable_(nullptr),
      simplePrecedenceTable_(nullptr),
      simplePrecedenceStepTable_(nullptr),
      simplePrecedenceSummaryLabel_(nullptr),
      parserTraceTreeWidget_(nullptr),
      parserStepTable_(nullptr),
      parserActionTable_(nullptr),
      highlightedTokenRow_(-1) {
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
    auto* exampleRow = new QHBoxLayout();
    exampleRow->addWidget(new QLabel("示例代码:", sourceGroup));
    exampleCombo_ = new QComboBox(sourceGroup);
    exampleRow->addWidget(exampleCombo_, 1);
    sourceLayout->addLayout(exampleRow);
    sourceEdit_ = new CodeEditorWithLineNumber(sourceGroup);
    sourceEdit_->setFont(QFont("Consolas", 11));
    sourceLayout->addWidget(sourceEdit_);

    const std::vector<std::pair<QString, QString>> examples = {
        {
            "1) if/while 嵌套",
            "PROGRAM demoifwhile\n"
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
            "END."
        },
        {
            "2) 数组读写（简单数组）",
            "PROGRAM demoarray\n"
            "TYPE arr = ARRAY[1..5] OF INTEGER;\n"
            "VAR a: arr;\n"
            "    i, x: INTEGER;\n"
            "BEGIN\n"
            "    i := 2;\n"
            "    a[i] := 10;\n"
            "    x := a[i] + 3;\n"
            "    a[1] := x + 0\n"
            "END."
        },
        {
            "3) record 字段访问",
            "PROGRAM demorecord\n"
            "TYPE rec = RECORD\n"
            "    a: INTEGER;\n"
            "    b: REAL;\n"
            "END;\n"
            "VAR x: rec;\n"
            "    y: REAL;\n"
            "BEGIN\n"
            "    x.a := 3;\n"
            "    x.b := 2.5;\n"
            "    y := x.b + x.a\n"
            "END."
        },
        {
            "4) 局部优化演示（常量折叠/传播）",
            "PROGRAM demoopt\n"
            "VAR x, y, z: INTEGER;\n"
            "BEGIN\n"
            "    x := 1 + 2;\n"
            "    y := x + 0;\n"
            "    z := (1 + 2) * (1 + 2)\n"
            "END."
        },
        {
            "5) 函数声明 + 主程序",
            "PROGRAM demofunction\n"
            "FUNCTION add(a: INTEGER; b: INTEGER): INTEGER;\n"
            "VAR t: INTEGER;\n"
            "BEGIN\n"
            "    t := a + b;\n"
            "    add := t\n"
            "END;\n"
            "VAR x, y, z: INTEGER;\n"
            "BEGIN\n"
            "    x := 3;\n"
            "    y := 4;\n"
            "    z := add(x, y)\n"
            "END."
        },
        {
            "6) 综合（record + array + 控制流）",
            "PROGRAM demomix\n"
            "TYPE rec = RECORD\n"
            "    a: INTEGER;\n"
            "END;\n"
            "arr = ARRAY[1..4] OF INTEGER;\n"
            "VAR r: rec;\n"
            "    a: arr;\n"
            "    i, s: INTEGER;\n"
            "BEGIN\n"
            "    i := 1;\n"
            "    s := 0;\n"
            "    WHILE i < 4 DO\n"
            "    BEGIN\n"
            "        a[i] := i * 2;\n"
            "        IF a[i] > 2 THEN\n"
            "            s := s + a[i]\n"
            "        ELSE\n"
            "            s := s + 0;\n"
            "        i := i + 1\n"
            "    END;\n"
            "    r.a := s\n"
            "END."
        }
    };
    for (const auto& ex : examples) {
        exampleCombo_->addItem(ex.first);
    }
    sourceEdit_->setPlainText(examples.front().second);
    connect(exampleCombo_, &QComboBox::currentIndexChanged, this, [this, examples](int index) {
        if (index >= 0 && index < static_cast<int>(examples.size())) {
            sourceEdit_->setPlainText(examples[index].second);
        }
    });

    auto* buttonArea = new QWidget(leftPanel);
    auto* buttonLayout = new QGridLayout(buttonArea);
    openFileButton_ = new QPushButton("打开源文件", buttonArea);
    compileButton_ = new QPushButton("编译并运行", buttonArea);
    clearButton_ = new QPushButton("清空输出", buttonArea);
    buttonLayout->addWidget(openFileButton_, 0, 0);
    buttonLayout->addWidget(compileButton_, 0, 1);
    buttonLayout->addWidget(clearButton_, 0, 2);

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

    rawQuadrupleTable_ = new QTableWidget(tabWidget_);
    setupTable(rawQuadrupleTable_, {"序号", "OP", "ARG1", "ARG2", "RESULT"});
    rawQuadrupleTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    rawQuadrupleTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    rawQuadrupleTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    rawQuadrupleTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    rawQuadrupleTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    rawQuadrupleTable_->setColumnWidth(0, 56);
    tabWidget_->addTab(createTablePage("中间代码原始四元式序列（未优化）", rawQuadrupleTable_), "原始四元式");

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
    tabWidget_->addTab(createTablePage("目标代码生成过程", targetCodeTable_), "目标代码");

    vmResultTable_ = new QTableWidget(tabWidget_);
    setupTable(vmResultTable_, {"变量名", "值"});
    vmResultTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    vmResultTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    tabWidget_->addTab(createTablePage("虚拟机执行后的最终变量值", vmResultTable_), "VM运行结果");

    QWidget* simplePrecedencePage = nullptr;
    QWidget* recursiveTracePage = nullptr;

    {
        auto* spPage = new QWidget(tabWidget_);
        simplePrecedencePage = spPage;
        auto* spLayout = new QVBoxLayout(spPage);
        auto* spTitle = new QLabel("简单优先分析过程", spPage);
        spTitle->setStyleSheet("font-weight: 600; color: #334155; padding: 4px 0;");
        spLayout->addWidget(spTitle);

        simplePrecedenceTable_ = new QTableWidget(spPage);
        setupTable(simplePrecedenceTable_, {"", "i", "+", "*", "(", ")", "#"});
        spLayout->addWidget(new QLabel("简单优先分析表", spPage));
        spLayout->addWidget(simplePrecedenceTable_);

        auto* processTitle = new QLabel("简单分析过程", spPage);
        processTitle->setStyleSheet("font-weight: 600; color: #334155; padding: 4px 0;");
        spLayout->addWidget(processTitle);

        simplePrecedenceSummaryLabel_ = new QLabel("表达式：等待编译", spPage);
        simplePrecedenceSummaryLabel_->setStyleSheet("color: #334155;");
        spLayout->addWidget(simplePrecedenceSummaryLabel_);

        simplePrecedenceStepTable_ = new QTableWidget(spPage);
        setupTable(simplePrecedenceStepTable_, {"步骤", "栈", "输入", "关系", "动作"});
        simplePrecedenceStepTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        simplePrecedenceStepTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        simplePrecedenceStepTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        simplePrecedenceStepTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        simplePrecedenceStepTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        spLayout->addWidget(new QLabel("分析步骤", spPage));
        spLayout->addWidget(simplePrecedenceStepTable_);

        tabWidget_->insertTab(3, spPage, "简单优先分析");
    }

    {
        auto* tracePage = new QWidget(tabWidget_);
        recursiveTracePage = tracePage;
        auto* traceLayout = new QVBoxLayout(tracePage);
        traceLayout->setContentsMargins(9, 9, 9, 9);
        traceLayout->setSpacing(6);
        auto* traceTitle = new QLabel("递归子程序分析过程", tracePage);
        traceTitle->setStyleSheet("font-weight: 600; color: #334155; padding: 4px 0;");
        traceTitle->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        traceTitle->setMaximumHeight(28);
        traceLayout->addWidget(traceTitle);

        auto* traceSplitter = new QSplitter(Qt::Horizontal, tracePage);
        parserTraceTreeWidget_ = new QTreeWidget(traceSplitter);
        parserTraceTreeWidget_->setHeaderLabels({"递归调用树"});
        parserTraceTreeWidget_->setAlternatingRowColors(true);

        auto* traceRightPanel = new QWidget(traceSplitter);
        auto* traceRightLayout = new QVBoxLayout(traceRightPanel);
        parserStepTable_ = new QTableWidget(traceRightPanel);
        setupTable(parserStepTable_, {"步骤日志"});
        parserStepTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        parserActionTable_ = new QTableWidget(traceRightPanel);
        setupTable(parserActionTable_, {"语义动作日志"});
        parserActionTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        traceRightLayout->addWidget(new QLabel("Token匹配与预测步骤", traceRightPanel));
        traceRightLayout->addWidget(parserStepTable_);
        traceRightLayout->addWidget(new QLabel("四元式与临时变量动作", traceRightPanel));
        traceRightLayout->addWidget(parserActionTable_);

        traceSplitter->addWidget(parserTraceTreeWidget_);
        traceSplitter->addWidget(traceRightPanel);
        traceSplitter->setStretchFactor(0, 1);
        traceSplitter->setStretchFactor(1, 1);
        traceLayout->addWidget(traceSplitter);
        traceLayout->setStretch(0, 0);
        traceLayout->setStretch(1, 1);
        tabWidget_->insertTab(2, tracePage, "递归分析过程");

        connect(parserTraceTreeWidget_, &QTreeWidget::itemSelectionChanged, this, [this]() {
            auto items = parserTraceTreeWidget_->selectedItems();
            if (items.isEmpty()) {
                highlightParserRowsByRule("");
                return;
            }
            QString text = items.first()->text(0).trimmed();
            if (text.startsWith("入口 子程序 ")) {
                text = text.mid(QString("入口 子程序 ").size());
            } else if (text.startsWith("出口 子程序 ")) {
                text = text.mid(QString("出口 子程序 ").size());
            }
            highlightParserRowsByRule(text.trimmed());
        });
    }

    if (simplePrecedencePage != nullptr && recursiveTracePage != nullptr) {
        int simpleIndex = tabWidget_->indexOf(simplePrecedencePage);
        int traceIndex = tabWidget_->indexOf(recursiveTracePage);
        if (simpleIndex >= 0 && traceIndex >= 0) {
            tabWidget_->removeTab(simpleIndex);
            traceIndex = tabWidget_->indexOf(recursiveTracePage);
            tabWidget_->insertTab(traceIndex + 1, simplePrecedencePage, "简单优先分析");
        }
    }

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
    statusLabel_ = new QPlainTextEdit(statusPanel);
    statusLabel_->setReadOnly(true);
    statusLabel_->setPlainText("就绪");
    statusLabel_->setStyleSheet("color: #333333; font-weight: 600; background: #ffffff;");
    statusLabel_->setMinimumHeight(70);
    statusLayout->addWidget(statusLabel_);

    auto* verticalSplitter = new QSplitter(Qt::Vertical, central);
    verticalSplitter->addWidget(splitter);
    verticalSplitter->addWidget(statusPanel);
    verticalSplitter->setStretchFactor(0, 20);
    verticalSplitter->setStretchFactor(1, 1);
    verticalSplitter->setSizes({660, 120});
    verticalSplitter->setCollapsible(1, false);

    rootLayout->addWidget(verticalSplitter);
    setCentralWidget(central);

    connect(compileButton_, &QPushButton::clicked, this, [this]() { compileAndRun(); });
    connect(clearButton_, &QPushButton::clicked, this, [this]() { clearOutput(); });
    connect(openFileButton_, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(
            this, "打开源程序文件", QString(), "Source Files (*.pas *.txt);;All Files (*)");
        if (path.isEmpty()) {
            return;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "打开失败", "无法读取文件: " + path);
            return;
        }
        QTextStream in(&file);
        sourceEdit_->setPlainText(in.readAll());
        statusLabel_->setPlainText("已加载文件: " + path);
        statusLabel_->setStyleSheet("color: #333333; font-weight: 600;");
    });
    connect(tokenTable_, &QTableWidget::cellClicked, this, [this](int row, int) {
        if (row < 0 || row >= static_cast<int>(lastResult_.tokens.size())) {
            return;
        }
        if (highlightedTokenRow_ == row) {
            clearSourceHighlight();
            highlightedTokenRow_ = -1;
            return;
        }
        const Token& token = lastResult_.tokens[row];
        if (token.lexeme.empty() || token.type == TokenType::END_OF_FILE) {
            clearSourceHighlight();
            highlightedTokenRow_ = -1;
            return;
        }
        highlightTokenAt(token.line, token.column, QString::fromStdString(token.lexeme));
        highlightedTokenRow_ = row;
    });
    connect(identifierTable_, &QTableWidget::cellClicked, this, [this](int row, int) {
        const QTableWidgetItem* nameItem = identifierTable_->item(row, 1);
        if (!nameItem) {
            return;
        }
        highlightLexemeOccurrences(nameItem->text());
    });
    connect(keywordTable_, &QTableWidget::cellClicked, this, [this](int row, int) {
        const QTableWidgetItem* item = keywordTable_->item(row, 1);
        if (!item) {
            return;
        }
        highlightLexemeOccurrences(item->text());
    });
    connect(delimiterTable_, &QTableWidget::cellClicked, this, [this](int row, int) {
        const QTableWidgetItem* item = delimiterTable_->item(row, 1);
        if (!item) {
            return;
        }
        highlightLexemeOccurrences(item->text());
    });
    connect(constantTable_, &QTableWidget::cellClicked, this, [this](int row, int) {
        const QTableWidgetItem* item = constantTable_->item(row, 1);
        if (!item) {
            return;
        }
        highlightLexemeOccurrences(item->text());
    });

    connectTableHighlight(synblTable_);
    connectTableHighlight(typelTable_);
    connectTableHighlight(rinflTable_);
    connectTableHighlight(ainflTable_);
    connectTableHighlight(conslTable_);
    connectTableHighlight(lenlTable_);
    connectTableHighlight(vallTable_);
}

QWidget* MainWindow::createSymbolSystemPage() {
    auto* page = new QWidget(tabWidget_);
    auto* layout = new QVBoxLayout(page);
    auto* label = new QLabel("符号表系统", page);
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

    {
        auto* pPage = new QWidget(symbolTabWidget_);
        auto* pLayout = new QVBoxLayout(pPage);
        pLayout->setSpacing(4);
        pLayout->setContentsMargins(6, 6, 6, 6);
        pfinflContainer_ = new QWidget(pPage);
        pfinflLayout_ = new QVBoxLayout(pfinflContainer_);
        pfinflLayout_->setSpacing(6);
        pfinflLayout_->setContentsMargins(0, 0, 0, 0);
        pLayout->addWidget(pfinflContainer_);
        symbolTabWidget_->addTab(pPage, "PFINFL函数表");
    }

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

void MainWindow::setupTable(QTableWidget* table, const QStringList& headers) {
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    for (int i = 0; i < headers.size(); ++i) {
        if (headers[i] == "序号") {
            table->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Fixed);
            table->setColumnWidth(i, 48);
        }
    }
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
        const QString stage = result.errorStage.empty()
            ? QString("未知阶段")
            : QString::fromStdString(result.errorStage);

        QString position = "未知位置";
        if (result.errorLine > 0 && result.errorColumn > 0) {
            position = QString("line %1, column %2").arg(result.errorLine).arg(result.errorColumn);
        }

        QString reason = QString::fromStdString(result.errorMessage);
        const QStringList lines = reason.split('\n', Qt::SkipEmptyParts);
        for (const QString& one : lines) {
            if (one.startsWith("原因:")) {
                reason = one.mid(QString("原因:").size()).trimmed();
                break;
            }
        }
        if (reason.isEmpty()) {
            reason = "未知错误";
        }

        if (reason.contains("优先关系未定义")) {
            reason = "表达式语法不完整：相邻记号之间缺少合法运算关系。";
        } else if (reason.contains("复合语句缺少 end")) {
            reason = "复合语句结构不完整：缺少结束符 end。";
        } else if (reason.contains("else 无法匹配到 if")) {
            reason = "条件分支结构错误：ELSE 未与 IF 正确匹配。";
        } else if (reason.contains("条件表达式缺少关系运算符")) {
            reason = "条件表达式不完整：缺少关系运算符（<、>、=、!=）。";
        } else if (reason.contains("赋值类型不兼容")) {
            reason = "类型检查失败：赋值左值类型与右侧表达式类型不兼容。";
        }

        QStringList issues;
        issues << QString("line %1, column %2: %3")
                      .arg(result.errorLine > 0 ? QString::number(result.errorLine) : "?")
                      .arg(result.errorColumn > 0 ? QString::number(result.errorColumn) : "?")
                      .arg(reason);
        for (const std::string& err : result.additionalErrors) {
            issues << QString::fromStdString(err);
        }

        QString message = QString("阶段: %1\n问题列表:").arg(stage);
        for (int i = 0; i < issues.size(); ++i) {
            message += QString("\n%1. %2").arg(i + 1).arg(issues[i]);
        }
        setStatusError(QString("[%1] %2").arg(stage, message));
        tabWidget_->setCurrentIndex(0);
        QMessageBox::critical(this, QString("编译错误 - %1").arg(stage), message);
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
    if (pfinflLayout_ != nullptr) {
        QLayoutItem* child = nullptr;
        while ((child = pfinflLayout_->takeAt(0)) != nullptr) {
            if (child->widget() != nullptr) {
                child->widget()->deleteLater();
            }
            delete child;
        }
    }
    conslTable_->setRowCount(0);
    lenlTable_->setRowCount(0);
    vallTable_->setRowCount(0);
    rawQuadrupleTable_->setRowCount(0);
    quadrupleOptimizeTable_->setRowCount(0);
    targetCodeTable_->setRowCount(0);
    vmResultTable_->setRowCount(0);
    if (simplePrecedenceTable_ != nullptr) {
        simplePrecedenceTable_->setRowCount(0);
    }
    if (simplePrecedenceStepTable_ != nullptr) {
        simplePrecedenceStepTable_->setRowCount(0);
    }
    if (simplePrecedenceSummaryLabel_ != nullptr) {
        simplePrecedenceSummaryLabel_->setText("等待编译");
    }
    if (parserTraceTreeWidget_ != nullptr) {
        parserTraceTreeWidget_->clear();
    }
    if (parserStepTable_ != nullptr) {
        parserStepTable_->setRowCount(0);
    }
    if (parserActionTable_ != nullptr) {
        parserActionTable_->setRowCount(0);
    }
    clearSourceHighlight();
    highlightedTokenRow_ = -1;
    statusLabel_->setPlainText("输出已清空");
    statusLabel_->setStyleSheet("color: #333333; font-weight: 600;");
}

void MainWindow::fillAllTables(const CompileResult& result) {
    lastResult_ = result;
    fillTokenTable(result);
    fillKeywordAndDelimiterTables(result);
    fillIdentifierAndConstantTables(result);
    fillSymbolTable(result);
    fillRawQuadrupleTable(result);
    fillQuadrupleOptimizeTable(result);
    fillTargetCodeTable(result);
    fillVmResultTable(result);
    fillSimplePrecedenceView(result);
    fillParserTraceView(result);
}

void MainWindow::fillTokenTable(const CompileResult& result) {
    tokenTable_->setRowCount(static_cast<int>(result.tokens.size()));
    for (int row = 0; row < static_cast<int>(result.tokens.size()); ++row) {
        const Token& token = result.tokens[row];
        tokenTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
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
        identifierTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
        identifierTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(result.identifierTable[row])));
    }

    constantTable_->setRowCount(static_cast<int>(result.constantEntries.size()));
    for (int row = 0; row < static_cast<int>(result.constantEntries.size()); ++row) {
        const auto& entry = result.constantEntries[row];
        constantTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
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
    fillConslTable(result);
    fillLenlTable(result);
    fillVallTable(result);
}

void MainWindow::fillSynblTable(const CompileResult& result) {
    synblTable_->setRowCount(static_cast<int>(result.symbols.size()));
    for (int row = 0; row < static_cast<int>(result.symbols.size()); ++row) {
        const Symbol& symbol = result.symbols[row];
        synblTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
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
    typelTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
    typelTable_->setItem(row, 1, new QTableWidgetItem("ircb"));
    typelTable_->setItem(row, 2, new QTableWidgetItem("basic"));
    typelTable_->setItem(row, 3, new QTableWidgetItem(""));
    ++row;

    int rinflStart = 0;
    for (const auto& record : result.recordTypes) {
        typelTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
        typelTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(record.name)));
        typelTable_->setItem(row, 2, new QTableWidgetItem("record"));
        typelTable_->setItem(row, 3, new QTableWidgetItem(QString("RINFL+%1").arg(rinflStart)));
        rinflStart += static_cast<int>(record.fields.size());
        ++row;
    }

    for (int i = 0; i < static_cast<int>(result.arrayTypes.size()); ++i) {
        const auto& array = result.arrayTypes[i];
        typelTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
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
            rinflTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
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
        ainflTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
        ainflTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(array.name)));
        ainflTable_->setItem(row, 2, new QTableWidgetItem(QString::number(array.low)));
        ainflTable_->setItem(row, 3, new QTableWidgetItem(QString::number(array.high)));
        ainflTable_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(array.elementType)));
        ainflTable_->setItem(row, 5, new QTableWidgetItem(QString::number(array.elementSize)));
    }
}

void MainWindow::fillPfinflTable(const CompileResult& result) {
    if (pfinflLayout_ == nullptr) {
        return;
    }

    QLayoutItem* child = nullptr;
    while ((child = pfinflLayout_->takeAt(0)) != nullptr) {
        if (child->widget() != nullptr) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    for (int fi = 0; fi < static_cast<int>(result.functionTable.size()); ++fi) {
        const auto& function = result.functionTable[fi];

        auto* title = new QLabel(QString("函数 %1").arg(QString::fromStdString(function.name)), pfinflContainer_);
        title->setStyleSheet("font-weight: 600; color: #334155; padding: 1px 0;");
        pfinflLayout_->addWidget(title);

        auto* funcTable = new QTableWidget(pfinflContainer_);
        setupTable(funcTable, {"层次", "区距", "参数个数", "ENTRY", "指针"});
        funcTable->setRowCount(1);
        funcTable->setItem(0, 0, new QTableWidgetItem(QString::number(function.level)));
        funcTable->setItem(0, 1, new QTableWidgetItem(QString::number(function.offset)));
        funcTable->setItem(0, 2, new QTableWidgetItem(QString::number(function.paramCount)));
        funcTable->setItem(0, 3, new QTableWidgetItem(QString::number(function.entryQuad)));
        funcTable->setItem(0, 4, new QTableWidgetItem(QString("PARAM+%1").arg(function.paramStart)));
        connectTableHighlight(funcTable);
        pfinflLayout_->addWidget(funcTable);

        auto* paramTable = new QTableWidget(pfinflContainer_);
        setupTable(paramTable, {"序号", "NAME", "TYPEL", "CAT", "INFO"});
        paramTable->setRowCount(function.paramCount);
        for (int i = 0; i < function.paramCount; ++i) {
            const auto& param = result.parameterTable[function.paramStart + i];
            Symbol pseudo;
            pseudo.name = param.name;
            pseudo.typeName = param.type;
            pseudo.kind = (param.passMode == "vn") ? "var parameter" : "parameter";
            pseudo.address = param.offset;
            pseudo.size = param.size;

            paramTable->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
            paramTable->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(param.name)));
            paramTable->setItem(i, 2, new QTableWidgetItem(synblTypeRef(pseudo)));
            paramTable->setItem(i, 3, new QTableWidgetItem(synblCategory(pseudo)));
            paramTable->setItem(i, 4, new QTableWidgetItem(QString("(%1,%2)").arg(2).arg(param.offset)));
        }
        connectTableHighlight(paramTable);
        pfinflLayout_->addWidget(paramTable);

        if (fi + 1 < static_cast<int>(result.functionTable.size())) {
            auto* sep = new QFrame(pfinflContainer_);
            sep->setFrameShape(QFrame::HLine);
            sep->setFrameShadow(QFrame::Sunken);
            pfinflLayout_->addWidget(sep);
        }
    }

    pfinflLayout_->addStretch(1);
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
        lenlTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
        lenlTable_->setItem(row, 1, new QTableWidgetItem(basicTypes[row][0]));
        lenlTable_->setItem(row, 2, new QTableWidgetItem(basicTypes[row][1]));
    }

    for (const auto& record : result.recordTypes) {
        lenlTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
        lenlTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(record.name)));
        lenlTable_->setItem(row, 2, new QTableWidgetItem(QString::number(record.totalSize)));
        ++row;
    }

    for (const auto& array : result.arrayTypes) {
        lenlTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
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

void MainWindow::fillTargetCodeTable(const CompileResult& result) {
    targetCodeTable_->setRowCount(static_cast<int>(result.targetTrace.size()));
    targetCodeTable_->clearSpans();
    for (int row = 0; row < static_cast<int>(result.targetTrace.size()); ++row) {
        const TargetTraceItem& t = result.targetTrace[row];
        targetCodeTable_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(t.basicBlock)));
        targetCodeTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(t.quadruple)));
        targetCodeTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(t.targetCode)));
        targetCodeTable_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(t.rdl)));
        targetCodeTable_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(t.sem)));
    }

    // 合并“基本块”列中连续相同的块号（例如全是 B1 时合并成一个单元格）。
    int start = 0;
    while (start < targetCodeTable_->rowCount()) {
        const QTableWidgetItem* startItem = targetCodeTable_->item(start, 0);
        const QString block = startItem ? startItem->text() : QString();
        int end = start + 1;
        while (end < targetCodeTable_->rowCount()) {
            const QTableWidgetItem* cur = targetCodeTable_->item(end, 0);
            const QString curBlock = cur ? cur->text() : QString();
            if (curBlock != block) {
                break;
            }
            ++end;
        }
        const int spanRows = end - start;
        if (spanRows > 1) {
            targetCodeTable_->setSpan(start, 0, spanRows, 1);
            for (int r = start + 1; r < end; ++r) {
                targetCodeTable_->setItem(r, 0, new QTableWidgetItem(""));
            }
        }
        start = end;
    }
}

void MainWindow::fillVmResultTable(const CompileResult& result) {
    const int summaryRows = 6;
    vmResultTable_->setRowCount(summaryRows + static_cast<int>(result.runtimeValues.size()));

    vmResultTable_->setItem(0, 0, new QTableWidgetItem("执行状态"));
    vmResultTable_->setItem(0, 1, new QTableWidgetItem(result.vmFallbackUsed ? "已回退解释器" : "VM执行成功"));

    vmResultTable_->setItem(1, 0, new QTableWidgetItem("执行器"));
    vmResultTable_->setItem(1, 1, new QTableWidgetItem(result.vmFallbackUsed ? "Interpreter(回退)" : "VirtualMachine"));

    vmResultTable_->setItem(2, 0, new QTableWidgetItem("运行时长(ms)"));
    vmResultTable_->setItem(2, 1, new QTableWidgetItem(QString::number(result.vmRuntimeMs, 'f', 3)));

    vmResultTable_->setItem(3, 0, new QTableWidgetItem("报错信息"));
    vmResultTable_->setItem(3, 1, new QTableWidgetItem(
        result.vmErrorMessage.empty() ? "无报错" : QString::fromStdString(result.vmErrorMessage)));

    vmResultTable_->setItem(4, 0, new QTableWidgetItem("VM结果有效性"));
    auto* validItem = new QTableWidgetItem(result.vmFallbackUsed
        ? "无效（已回退解释器）"
        : "有效");
    if (result.vmFallbackUsed) {
        validItem->setForeground(QBrush(QColor(200, 30, 30)));
    } else {
        validItem->setForeground(QBrush(QColor(20, 120, 40)));
    }
    vmResultTable_->setItem(4, 1, validItem);

    vmResultTable_->setItem(5, 0, new QTableWidgetItem("---- 变量结果 ----"));
    vmResultTable_->setItem(5, 1, new QTableWidgetItem(""));

    int row = summaryRows;
    for (const auto& item : result.runtimeValues) {
        vmResultTable_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(item.first)));
        vmResultTable_->setItem(row, 1, new QTableWidgetItem(QString::number(item.second, 'g', 12)));
        ++row;
    }
}

void MainWindow::fillSimplePrecedenceView(const CompileResult& result) {
    std::vector<SimplePrecedenceResult> all;
    if (result.simplePrecedenceAll.empty()) {
        all.push_back(result.simplePrecedence);
    } else {
        all = result.simplePrecedenceAll;
    }
    const SimplePrecedenceResult& sp = all.front();

    if (simplePrecedenceSummaryLabel_ != nullptr) {
        if (!sp.enabled || all.empty()) {
            simplePrecedenceSummaryLabel_->setText("表达式：未启用");
        } else {
            QStringList lines;
            for (int i = 0; i < static_cast<int>(all.size()); ++i) {
                const auto& one = all[i];
                if (one.success) {
                    lines << QString("表达式%1：%2    结果：成功")
                                .arg(i + 1)
                                .arg(QString::fromStdString(one.expressionText));
                } else {
                    lines << QString("表达式%1：%2    结果：失败    原因：%3")
                                .arg(i + 1)
                                .arg(QString::fromStdString(one.expressionText))
                                .arg(QString::fromStdString(one.errorMessage));
                }
            }
            simplePrecedenceSummaryLabel_->setText(lines.join("\n"));
        }
    }

    if (simplePrecedenceTable_ != nullptr) {
        int n = static_cast<int>(sp.symbols.size());
        simplePrecedenceTable_->setColumnCount(n + 1);
        QStringList headers;
        headers << "";
        for (const auto& t : sp.symbols) {
            headers << QString::fromStdString(t);
        }
        simplePrecedenceTable_->setHorizontalHeaderLabels(headers);
        simplePrecedenceTable_->setRowCount(n);

        for (int i = 0; i < n; ++i) {
            simplePrecedenceTable_->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(sp.symbols[i])));
            for (int j = 0; j < n; ++j) {
                QString rel;
                if (i < static_cast<int>(sp.table.size()) && j < static_cast<int>(sp.table[i].size())) {
                    rel = QString::fromStdString(sp.table[i][j]);
                }
                simplePrecedenceTable_->setItem(i, j + 1, new QTableWidgetItem(rel));
            }
        }
    }

    if (simplePrecedenceStepTable_ != nullptr) {
        int rows = 0;
        for (const auto& one : all) {
            rows += 1 + static_cast<int>(one.steps.size());
        }
        simplePrecedenceStepTable_->setRowCount(rows);
        int row = 0;
        for (int exprIndex = 0; exprIndex < static_cast<int>(all.size()); ++exprIndex) {
            const auto& one = all[exprIndex];
            simplePrecedenceStepTable_->setItem(row, 0, new QTableWidgetItem(QString("表达式%1").arg(exprIndex + 1)));
            simplePrecedenceStepTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(one.expressionText)));
            simplePrecedenceStepTable_->setItem(row, 2, new QTableWidgetItem(""));
            simplePrecedenceStepTable_->setItem(row, 3, new QTableWidgetItem(""));
            simplePrecedenceStepTable_->setItem(row, 4, new QTableWidgetItem(""));
            ++row;
            for (int i = 0; i < static_cast<int>(one.steps.size()); ++i) {
                const auto& step = one.steps[i];
                simplePrecedenceStepTable_->setItem(row, 0, new QTableWidgetItem(QString::number(i + 1)));
                simplePrecedenceStepTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(step.stackText)));
                simplePrecedenceStepTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(step.inputText)));
                simplePrecedenceStepTable_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(step.relationText)));
                simplePrecedenceStepTable_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(step.actionText)));
                ++row;
            }
        }
    }
}

void MainWindow::fillParserTraceView(const CompileResult& result) {
    if (parserTraceTreeWidget_ != nullptr) {
        parserTraceTreeWidget_->clear();
        std::vector<QTreeWidgetItem*> stack;
        for (const auto& lineStd : result.parserTraceTree) {
            QString line = QString::fromStdString(lineStd);
            int spaces = 0;
            while (spaces < line.size() && line[spaces] == ' ') {
                ++spaces;
            }
            int depth = spaces / 2;
            QString text = line.mid(spaces);
            auto* item = new QTreeWidgetItem(QStringList{text});
            QString ruleName = text;
            if (ruleName.startsWith("入口 子程序 ")) {
                ruleName = ruleName.mid(QString("入口 子程序 ").size()).trimmed();
            } else if (ruleName.startsWith("出口 子程序 ")) {
                ruleName = ruleName.mid(QString("出口 子程序 ").size()).trimmed();
            }
            item->setData(0, Qt::UserRole, ruleName);
            while (static_cast<int>(stack.size()) > depth) {
                stack.pop_back();
            }
            if (stack.empty()) {
                parserTraceTreeWidget_->addTopLevelItem(item);
            } else {
                stack.back()->addChild(item);
            }
            stack.push_back(item);
        }
        parserTraceTreeWidget_->expandAll();
    }

    if (parserStepTable_ != nullptr) {
        parserStepTable_->setRowCount(static_cast<int>(result.parserStepLog.size()));
        for (int row = 0; row < static_cast<int>(result.parserStepLog.size()); ++row) {
            auto* item = new QTableWidgetItem(QString::fromStdString(result.parserStepLog[row]));
            if (row < static_cast<int>(result.parserStepRuleNames.size())) {
                item->setData(Qt::UserRole, QString::fromStdString(result.parserStepRuleNames[row]));
            }
            parserStepTable_->setItem(row, 0, item);
        }
    }

    if (parserActionTable_ != nullptr) {
        parserActionTable_->setRowCount(static_cast<int>(result.parserActionLog.size()));
        for (int row = 0; row < static_cast<int>(result.parserActionLog.size()); ++row) {
            auto* item = new QTableWidgetItem(QString::fromStdString(result.parserActionLog[row]));
            if (row < static_cast<int>(result.parserActionRuleNames.size())) {
                item->setData(Qt::UserRole, QString::fromStdString(result.parserActionRuleNames[row]));
            }
            parserActionTable_->setItem(row, 0, item);
        }
    }

    highlightParserRowsByRule("");
}

void MainWindow::highlightParserRowsByRule(const QString& ruleName) {
    QString firstMatchedStepText;
    auto paintTable = [&](QTableWidget* table) {
        if (table == nullptr) {
            return;
        }
        for (int row = 0; row < table->rowCount(); ++row) {
            QTableWidgetItem* item = table->item(row, 0);
            if (item == nullptr) {
                continue;
            }
            QString itemRule = item->data(Qt::UserRole).toString().trimmed();
            bool hit = !ruleName.isEmpty() && itemRule == ruleName;
            item->setBackground(hit ? QColor(255, 248, 220) : QColor(Qt::white));
            if (hit && table == parserStepTable_ && firstMatchedStepText.isEmpty()) {
                firstMatchedStepText = item->text();
            }
        }
    };

    paintTable(parserStepTable_);
    paintTable(parserActionTable_);

    if (ruleName.isEmpty()) {
        return;
    }

    QRegularExpression posRe("@\\((\\d+),(\\d+)\\)");
    QRegularExpressionMatch match = posRe.match(firstMatchedStepText);
    if (!match.hasMatch()) {
        return;
    }

    int line = match.captured(1).toInt();
    int column = match.captured(2).toInt();
    highlightTokenAt(line, column, "x");
}

void MainWindow::fillRawQuadrupleTable(const CompileResult& result) {
    rawQuadrupleTable_->setRowCount(static_cast<int>(result.quadruples.size()));
    for (int i = 0; i < static_cast<int>(result.quadruples.size()); ++i) {
        const Quadruple& q = result.quadruples[i];
        rawQuadrupleTable_->setItem(i, 0, new QTableWidgetItem(QString::number(i)));
        rawQuadrupleTable_->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(q.op)));
        rawQuadrupleTable_->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(q.arg1)));
        rawQuadrupleTable_->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(q.arg2)));
        rawQuadrupleTable_->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(q.result)));
    }
}

void MainWindow::setStatusSuccess(const QString& message) {
    statusLabel_->setPlainText(message);
    statusLabel_->setStyleSheet("color: #16833a; font-weight: 700;");
}

void MainWindow::setStatusError(const QString& message) {
    statusLabel_->setPlainText(message);
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
    QTextCursor viewCursor = cursor;
    viewCursor.clearSelection();
    sourceEdit_->setTextCursor(viewCursor);
    highlightedLexeme_.clear();
}

void MainWindow::highlightLexemeOccurrences(const QString& lexeme) {
    if (lexeme.isEmpty()) {
        clearSourceHighlight();
        return;
    }

    // 同一词素再次点击时取消高亮。
    if (highlightedLexeme_ == lexeme) {
        clearSourceHighlight();
        return;
    }

    clearSourceHighlight();
    QList<QTextEdit::ExtraSelection> selections;
    QTextDocument* doc = sourceEdit_->document();
    QTextCursor cursor(doc);

    const bool isWordLike = QRegularExpression("^[A-Za-z][A-Za-z0-9]*$").match(lexeme).hasMatch();
    QRegularExpression re(
        isWordLike
            ? QString("\\b%1\\b").arg(QRegularExpression::escape(lexeme))
            : QRegularExpression::escape(lexeme)
    );

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
        QTextCursor viewCursor = selections.first().cursor;
        viewCursor.clearSelection();
        sourceEdit_->setTextCursor(viewCursor);
        highlightedLexeme_ = lexeme;
    } else {
        highlightedLexeme_.clear();
    }
}

void MainWindow::connectTableHighlight(QTableWidget* table) {
    if (table == nullptr) {
        return;
    }
    connect(table, &QTableWidget::cellClicked, this, [this, table](int row, int column) {
        onGenericTableCellClicked(table, row, column);
    });
}

void MainWindow::onGenericTableCellClicked(QTableWidget* table, int row, int column) {
    if (table == nullptr || row < 0 || column < 0) {
        return;
    }
    const QTableWidgetItem* item = table->item(row, column);
    if (item == nullptr) {
        return;
    }
    const QString text = item->text().trimmed();
    if (text.isEmpty()) {
        return;
    }

    const QString key = QString("%1|%2|%3|%4")
        .arg(reinterpret_cast<quintptr>(table))
        .arg(row)
        .arg(column)
        .arg(text);
    if (highlightedCellKey_ == key) {
        clearSourceHighlight();
        return;
    }

    if (highlightFromCellText(text)) {
        highlightedCellKey_ = key;
    } else {
        clearSourceHighlight();
    }
}

bool MainWindow::highlightFromCellText(const QString& text) {
    const QString source = sourceEdit_->toPlainText();
    if (source.isEmpty()) {
        return false;
    }

    auto hasWordMatch = [&source](const QString& lexeme) {
        QRegularExpression re(QString("\\b%1\\b").arg(QRegularExpression::escape(lexeme)));
        return re.match(source).hasMatch();
    };
    auto hasExactMatch = [&source](const QString& lexeme) {
        QRegularExpression re(QRegularExpression::escape(lexeme));
        return re.match(source).hasMatch();
    };

    const QRegularExpression tokenRe("([A-Za-z][A-Za-z0-9]*|\\d+(?:\\.\\d+)?)");
    QRegularExpressionMatchIterator it = tokenRe.globalMatch(text);
    while (it.hasNext()) {
        const QString candidate = it.next().captured(1);
        if (candidate.isEmpty()) {
            continue;
        }
        if (hasWordMatch(candidate)) {
            highlightedLexeme_.clear();
            highlightLexemeOccurrences(candidate);
            return !sourceEdit_->extraSelections().isEmpty();
        }
    }

    if (text.size() <= 24 && hasExactMatch(text)) {
        highlightedLexeme_.clear();
        highlightLexemeOccurrences(text);
        return !sourceEdit_->extraSelections().isEmpty();
    }
    return false;
}

void MainWindow::clearSourceHighlight() {
    sourceEdit_->setExtraSelections({});
    QTextCursor cursor = sourceEdit_->textCursor();
    cursor.clearSelection();
    sourceEdit_->setTextCursor(cursor);
    highlightedTokenRow_ = -1;
    highlightedLexeme_.clear();
    highlightedCellKey_.clear();
}
