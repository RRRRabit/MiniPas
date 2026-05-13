#ifndef MINIPASPLUS_MAINWINDOW_H
#define MINIPASPLUS_MAINWINDOW_H

#include "CompileResult.h"
#include <QLabel>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextEdit>

// Qt Widgets 主窗口：只负责界面展示，不包含编译器核心逻辑。
class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    QPlainTextEdit* sourceEdit_;
    QPushButton* compileButton_;
    QPushButton* clearButton_;
    QTabWidget* tabWidget_;
    QLabel* statusLabel_;

    QTableWidget* tokenTable_;
    QTableWidget* identifierTable_;
    QTableWidget* constantTable_;
    QTabWidget* symbolTabWidget_;
    QTableWidget* synblTable_;
    QTableWidget* typelTable_;
    QTableWidget* rinflTable_;
    QTableWidget* ainflTable_;
    QTableWidget* lenlTable_;
    QTableWidget* recordTable_;
    QTableWidget* quadrupleTable_;
    QTextEdit* runtimeText_;

    void buildUi();
    QWidget* createTablePage(const QString& description, QTableWidget* table);
    QWidget* createSymbolSystemPage();
    QWidget* createRuntimePage(const QString& description);
    void setupTable(QTableWidget* table, const QStringList& headers);
    void compileAndRun();
    void clearOutput();
    void fillAllTables(const CompileResult& result);
    void fillTokenTable(const CompileResult& result);
    void fillIdentifierAndConstantTables(const CompileResult& result);
    void fillSymbolTable(const CompileResult& result);
    void fillSynblTable(const CompileResult& result);
    void fillTypelTable(const CompileResult& result);
    void fillRinflTable(const CompileResult& result);
    void fillAinflTable(const CompileResult& result);
    void fillLenlTable(const CompileResult& result);
    void fillRecordTable(const CompileResult& result);
    void fillQuadrupleTable(const CompileResult& result);
    void fillRuntimeText(const CompileResult& result);
    void setStatusSuccess(const QString& message);
    void setStatusError(const QString& message);
};

#endif
