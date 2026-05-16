#ifndef MINIPASPLUS_MAINWINDOW_H
#define MINIPASPLUS_MAINWINDOW_H

#include "CompileResult.h"
#include <QLabel>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

// Qt Widgets 主窗口：只负责界面展示，不包含编译器核心逻辑。
class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    QPlainTextEdit* sourceEdit_;
    QComboBox* exampleCombo_;
    QPushButton* compileButton_;
    QPushButton* clearButton_;
    QTabWidget* tabWidget_;
    QLabel* statusLabel_;

    QTableWidget* tokenTable_;
    QTableWidget* keywordTable_;
    QTableWidget* delimiterTable_;
    QTableWidget* identifierTable_;
    QTableWidget* constantTable_;
    QTabWidget* symbolTabWidget_;
    QTableWidget* synblTable_;
    QTableWidget* typelTable_;
    QTableWidget* rinflTable_;
    QTableWidget* ainflTable_;
    QTableWidget* pfinflTable_;
    QTableWidget* pfinflParamTable_;
    QWidget* pfinflContainer_;
    QVBoxLayout* pfinflLayout_;
    QTableWidget* conslTable_;
    QTableWidget* lenlTable_;
    QTableWidget* vallTable_;
    QTableWidget* quadrupleOptimizeTable_;
    QTableWidget* targetCodeTable_;
    QTableWidget* vmResultTable_;
    CompileResult lastResult_;
    int highlightedTokenRow_ = -1;
    QString highlightedLexeme_;
    QString highlightedCellKey_;

    void buildUi();
    QWidget* createTablePage(const QString& description, QTableWidget* table);
    QWidget* createSymbolSystemPage();
    void setupTable(QTableWidget* table, const QStringList& headers);
    void compileAndRun();
    void clearOutput();
    void fillAllTables(const CompileResult& result);
    void fillTokenTable(const CompileResult& result);
    void fillIdentifierAndConstantTables(const CompileResult& result);
    void fillKeywordAndDelimiterTables(const CompileResult& result);
    void fillSymbolTable(const CompileResult& result);
    void fillSynblTable(const CompileResult& result);
    void fillTypelTable(const CompileResult& result);
    void fillRinflTable(const CompileResult& result);
    void fillAinflTable(const CompileResult& result);
    void fillPfinflTable(const CompileResult& result);
    void fillConslTable(const CompileResult& result);
    void fillLenlTable(const CompileResult& result);
    void fillVallTable(const CompileResult& result);
    void fillQuadrupleOptimizeTable(const CompileResult& result);
    void fillTargetCodeTable(const CompileResult& result);
    void fillVmResultTable(const CompileResult& result);
    void setStatusSuccess(const QString& message);
    void setStatusError(const QString& message);
    QString typeRef(const std::string& typeName) const;
    QString synblTypeRef(const Symbol& symbol) const;
    QString synblCategory(const Symbol& symbol) const;
    int symbolLevel(const Symbol& symbol) const;
    QString lenlPointer(const std::string& typeName, const CompileResult& result) const;
    int positionFromLineColumn(int line, int column) const;
    void highlightTokenAt(int line, int column, const QString& lexeme);
    void highlightWordOccurrences(const QString& word);
    void highlightLexemeOccurrences(const QString& lexeme);
    void connectTableHighlight(QTableWidget* table);
    void onGenericTableCellClicked(QTableWidget* table, int row, int column);
    bool highlightFromCellText(const QString& text);
    void clearSourceHighlight();
};

#endif
