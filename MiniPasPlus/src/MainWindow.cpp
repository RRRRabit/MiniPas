#include "MainWindow.h"
#include "CompilerFacade.h"
#include "Token.h"
#include <QApplication>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QSplitter>
#include <QTableWidgetItem>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      sourceEdit_(nullptr),
      compileButton_(nullptr),
      clearButton_(nullptr),
      tabWidget_(nullptr),
      statusLabel_(nullptr),
      tokenTable_(nullptr),
      identifierTable_(nullptr),
      constantTable_(nullptr),
      symbolTabWidget_(nullptr),
      synblTable_(nullptr),
      typelTable_(nullptr),
      rinflTable_(nullptr),
      ainflTable_(nullptr),
      lenlTable_(nullptr),
      recordTable_(nullptr),
      quadrupleTable_(nullptr),
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
        "PROGRAM test\n"
        "TYPE re = RECORD\n"
        "    a: REAL;\n"
        "    b: CHAR;\n"
        "END;\n"
        "arr = ARRAY[1..10] OF re;\n"
        "\n"
        "FUNCTION f(VAR m: BOOLEAN): INTEGER;\n"
        "VAR n: INTEGER;\n"
        "BEGIN\n"
        "END;\n"
        "\n"
        "VAR x: re;\n"
        "    z: arr;\n"
        "    y: REAL;\n"
        "BEGIN\n"
        "    x.a := 3.14;\n"
        "    y := x.a + 2\n"
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
    setupTable(tokenTable_, {"序号", "类型", "单词", "行号", "列号"});
    tabWidget_->addTab(createTablePage("词法分析阶段生成的 Token 序列", tokenTable_), "Token序列");

    auto* idConstPage = new QWidget(tabWidget_);
    auto* idConstLayout = new QVBoxLayout(idConstPage);
    identifierTable_ = new QTableWidget(idConstPage);
    constantTable_ = new QTableWidget(idConstPage);
    setupTable(identifierTable_, {"序号", "标识符"});
    setupTable(constantTable_, {"序号", "常数"});
    idConstLayout->addWidget(new QLabel("词法分析阶段维护的标识符表", idConstPage));
    idConstLayout->addWidget(identifierTable_);
    idConstLayout->addWidget(new QLabel("词法分析阶段维护的常数表", idConstPage));
    idConstLayout->addWidget(constantTable_);
    tabWidget_->addTab(idConstPage, "标识符与常数表");

    tabWidget_->addTab(createSymbolSystemPage(), "符号表");

    recordTable_ = new QTableWidget(tabWidget_);
    setupTable(recordTable_, {"RecordName", "FieldName", "FieldType", "Offset", "Size", "TotalSize"});
    tabWidget_->addTab(createTablePage("record 结构体字段偏移信息", recordTable_), "Record类型表");

    quadrupleTable_ = new QTableWidget(tabWidget_);
    setupTable(quadrupleTable_, {"序号", "op", "arg1", "arg2", "result"});
    tabWidget_->addTab(createTablePage("中间代码生成阶段得到的四元式序列", quadrupleTable_), "四元式序列");

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
}

QWidget* MainWindow::createSymbolSystemPage() {
    auto* page = new QWidget(tabWidget_);
    auto* layout = new QVBoxLayout(page);
    auto* label = new QLabel("语义分析阶段维护的符号表系统：总表、类型表、结构表、数组表和长度表", page);
    label->setStyleSheet("font-weight: 600; color: #334155; padding: 4px 0;");

    symbolTabWidget_ = new QTabWidget(page);

    synblTable_ = new QTableWidget(symbolTabWidget_);
    setupTable(synblTable_, {"序号", "NAME", "TYPE", "CAT", "ADDR", "SIZE"});
    symbolTabWidget_->addTab(createTablePage("SYNBL 总表：保存标识符及其语义信息", synblTable_), "SYNBL总表");

    typelTable_ = new QTableWidget(symbolTabWidget_);
    setupTable(typelTable_, {"序号", "TYPE", "KIND", "POINT/INFO"});
    symbolTabWidget_->addTab(createTablePage("TYPEL 类型表：保存基本类型和 record 类型入口", typelTable_), "TYPEL类型表");

    rinflTable_ = new QTableWidget(symbolTabWidget_);
    setupTable(rinflTable_, {"序号", "RecordName", "ID", "OFF", "TYPE", "SIZE"});
    symbolTabWidget_->addTab(createTablePage("RINFL 结构表：保存 record 字段名、字段偏移和字段类型", rinflTable_), "RINFL结构表");

    ainflTable_ = new QTableWidget(symbolTabWidget_);
    setupTable(ainflTable_, {"序号", "ArrayName", "LOW", "UP", "CTP", "CLEN", "TOTAL"});
    symbolTabWidget_->addTab(createTablePage("AINFL 数组表：保存数组上下界、成分类型和成分长度", ainflTable_), "AINFL数组表");

    lenlTable_ = new QTableWidget(symbolTabWidget_);
    setupTable(lenlTable_, {"序号", "TYPE", "LEN"});
    symbolTabWidget_->addTab(createTablePage("LENL 长度表：保存各类型占用的字节数", lenlTable_), "LENL长度表");

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
    identifierTable_->setRowCount(0);
    constantTable_->setRowCount(0);
    synblTable_->setRowCount(0);
    typelTable_->setRowCount(0);
    rinflTable_->setRowCount(0);
    ainflTable_->setRowCount(0);
    lenlTable_->setRowCount(0);
    recordTable_->setRowCount(0);
    quadrupleTable_->setRowCount(0);
    runtimeText_->clear();
    statusLabel_->setText("输出已清空");
    statusLabel_->setStyleSheet("color: #333333; font-weight: 600;");
}

void MainWindow::fillAllTables(const CompileResult& result) {
    fillTokenTable(result);
    fillIdentifierAndConstantTables(result);
    fillSymbolTable(result);
    fillRecordTable(result);
    fillQuadrupleTable(result);
    fillRuntimeText(result);
}

void MainWindow::fillTokenTable(const CompileResult& result) {
    tokenTable_->setRowCount(static_cast<int>(result.tokens.size()));
    for (int row = 0; row < static_cast<int>(result.tokens.size()); ++row) {
        const Token& token = result.tokens[row];
        tokenTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        tokenTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(tokenTypeToString(token.type))));
        tokenTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(token.lexeme)));
        tokenTable_->setItem(row, 3, new QTableWidgetItem(QString::number(token.line)));
        tokenTable_->setItem(row, 4, new QTableWidgetItem(QString::number(token.column)));
    }
}

void MainWindow::fillIdentifierAndConstantTables(const CompileResult& result) {
    identifierTable_->setRowCount(static_cast<int>(result.identifierTable.size()));
    for (int row = 0; row < static_cast<int>(result.identifierTable.size()); ++row) {
        identifierTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        identifierTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(result.identifierTable[row])));
    }

    constantTable_->setRowCount(static_cast<int>(result.constantTable.size()));
    for (int row = 0; row < static_cast<int>(result.constantTable.size()); ++row) {
        constantTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        constantTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(result.constantTable[row])));
    }
}

void MainWindow::fillSymbolTable(const CompileResult& result) {
    fillSynblTable(result);
    fillTypelTable(result);
    fillRinflTable(result);
    fillAinflTable(result);
    fillLenlTable(result);
}

void MainWindow::fillSynblTable(const CompileResult& result) {
    synblTable_->setRowCount(static_cast<int>(result.symbols.size()));
    for (int row = 0; row < static_cast<int>(result.symbols.size()); ++row) {
        const Symbol& symbol = result.symbols[row];
        synblTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        synblTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(symbol.name)));
        synblTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(symbol.typeName)));
        synblTable_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(symbol.kind)));
        synblTable_->setItem(row, 4, new QTableWidgetItem(symbol.address >= 0 ? QString::number(symbol.address) : "_"));
        synblTable_->setItem(row, 5, new QTableWidgetItem(QString::number(symbol.size)));
    }
}

void MainWindow::fillTypelTable(const CompileResult& result) {
    typelTable_->setRowCount(4 + static_cast<int>(result.recordTypes.size()) + static_cast<int>(result.arrayTypes.size()));

    const QString basicTypes[4][3] = {
        {"integer", "basic", "LEN=4"},
        {"real", "basic", "LEN=8"},
        {"char", "basic", "LEN=1"},
        {"boolean", "basic", "LEN=1"}
    };

    int row = 0;
    for (; row < 4; ++row) {
        typelTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        typelTable_->setItem(row, 1, new QTableWidgetItem(basicTypes[row][0]));
        typelTable_->setItem(row, 2, new QTableWidgetItem(basicTypes[row][1]));
        typelTable_->setItem(row, 3, new QTableWidgetItem(basicTypes[row][2]));
    }

    for (const auto& record : result.recordTypes) {
        typelTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        typelTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(record.name)));
        typelTable_->setItem(row, 2, new QTableWidgetItem("record"));
        typelTable_->setItem(row, 3, new QTableWidgetItem(QString("RINFL, totalSize=%1").arg(record.totalSize)));
        ++row;
    }

    for (const auto& array : result.arrayTypes) {
        typelTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        typelTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(array.name)));
        typelTable_->setItem(row, 2, new QTableWidgetItem("array"));
        typelTable_->setItem(row, 3, new QTableWidgetItem(QString("AINFL, totalSize=%1").arg(array.totalSize)));
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
            rinflTable_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(field.type)));
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
        ainflTable_->setItem(row, 6, new QTableWidgetItem(QString::number(array.totalSize)));
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
    for (int row = 0; row < static_cast<int>(result.quadruples.size()); ++row) {
        const Quadruple& quad = result.quadruples[row];
        quadrupleTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row)));
        quadrupleTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(quad.op)));
        quadrupleTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(quad.arg1)));
        quadrupleTable_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(quad.arg2)));
        quadrupleTable_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(quad.result)));
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

void MainWindow::setStatusSuccess(const QString& message) {
    statusLabel_->setText(message);
    statusLabel_->setStyleSheet("color: #16833a; font-weight: 700;");
}

void MainWindow::setStatusError(const QString& message) {
    statusLabel_->setText(message);
    statusLabel_->setStyleSheet("color: #c62828; font-weight: 700;");
}
