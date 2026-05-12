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
      symbolTable_(nullptr),
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
        "program test\n"
        "type Student = record\n"
        "    age: integer;\n"
        "    score: integer;\n"
        "end;\n"
        "\n"
        "var s: Student;\n"
        "begin\n"
        "    s.age := 18;\n"
        "    s.score := s.age + 80\n"
        "end.");
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

    symbolTable_ = new QTableWidget(tabWidget_);
    setupTable(symbolTable_, {"Name", "Type", "Category", "Address", "Size"});
    tabWidget_->addTab(createTablePage("语义分析阶段维护的符号表", symbolTable_), "符号表");

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
    symbolTable_->setRowCount(0);
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
    symbolTable_->setRowCount(static_cast<int>(result.symbols.size()));
    for (int row = 0; row < static_cast<int>(result.symbols.size()); ++row) {
        const Symbol& symbol = result.symbols[row];
        symbolTable_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(symbol.name)));
        symbolTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(symbol.typeName)));
        symbolTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(symbol.kind)));
        symbolTable_->setItem(row, 3, new QTableWidgetItem(symbol.address >= 0 ? QString::number(symbol.address) : "_"));
        symbolTable_->setItem(row, 4, new QTableWidgetItem(QString::number(symbol.size)));
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
