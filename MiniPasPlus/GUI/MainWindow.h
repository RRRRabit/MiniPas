#ifndef MINIPASPLUS_MAINWINDOW_H
#define MINIPASPLUS_MAINWINDOW_H

#include "CompileResult.h"
#include "CodeEditorWithLineNumber.h"
#include <QLabel>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QTreeWidget>

// Qt Widgets 主窗口。
//
// 这个类主要负责“把编译结果显示出来”，不是编译器核心。
// 可以这样理解：
// - CompilerFacade 负责真正编译和运行；
// - MainWindow 负责按钮、输入框、表格、颜色高亮；
// - fillXXXTable 这一组函数只是把 CompileResult 里的数据搬到 Qt 表格里。
class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    // 左侧源代码输入区和顶部按钮。
    CodeEditorWithLineNumber* sourceEdit_;
    QComboBox* exampleCombo_;
    QPushButton* openFileButton_;
    QPushButton* compileButton_;
    QPushButton* clearButton_;
    QTabWidget* tabWidget_;
    QPlainTextEdit* statusLabel_;

    // 下面这些 QTableWidget 都是界面表格。
    // 它们不保存核心数据，只负责显示 CompileResult 中已经算好的内容。
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
    QTableWidget* rawQuadrupleTable_;
    QTableWidget* quadrupleOptimizeTable_;
    QTableWidget* targetCodeTable_;
    QTableWidget* vmResultTable_;
    QTreeWidget* parserTraceTreeWidget_;
    QTableWidget* parserStepTable_;
    QTableWidget* parserActionTable_;
    CompileResult lastResult_;
    int highlightedTokenRow_ = -1;
    QString highlightedLexeme_;
    QString highlightedCellKey_;

    // 创建界面控件、布局和信号槽连接。
    void buildUi();

    // 常用页面构造函数：给一个表格外面套上说明文字和布局。
    QWidget* createTablePage(const QString& description, QTableWidget* table);
    QWidget* createSymbolSystemPage();

    // 设置表头、列宽、选择模式等表格通用属性。
    void setupTable(QTableWidget* table, const QStringList& headers);

    // “编译运行”按钮对应的槽函数。
    // 内部会调用 CompilerFacade::compileAndRun。
    void compileAndRun();

    // 清空界面输出。
    void clearOutput();

    // 总分发函数：把一次编译结果填充到所有表格。
    void fillAllTables(const CompileResult& result);

    // 下面这些 fillXXX 函数都是“展示函数”，不是核心算法。
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
    void fillRawQuadrupleTable(const CompileResult& result);
    void fillQuadrupleOptimizeTable(const CompileResult& result);
    void fillTargetCodeTable(const CompileResult& result);
    void fillVmResultTable(const CompileResult& result);
    void fillParserTraceView(const CompileResult& result);

    // 点击语法分析步骤时，高亮相关行，方便演示。
    void highlightParserRowsByRule(const QString& ruleName);

    // 状态栏显示成功/错误信息。
    void setStatusSuccess(const QString& message);
    void setStatusError(const QString& message);

    // 表格文本格式化辅助函数。
    QString typeRef(const std::string& typeName) const;
    QString synblTypeRef(const Symbol& symbol) const;
    QString synblCategory(const Symbol& symbol) const;
    int symbolLevel(const Symbol& symbol) const;
    QString lenlPointer(const std::string& typeName, const CompileResult& result) const;

    // 源代码高亮相关辅助函数。
    int positionFromLineColumn(int line, int column) const;
    void highlightTokenAt(int line, int column, const QString& lexeme);
    void highlightLexemeOccurrences(const QString& lexeme);
    void connectTableHighlight(QTableWidget* table);
    void onGenericTableCellClicked(QTableWidget* table, int row, int column);
    bool highlightFromCellText(const QString& text);
    void clearSourceHighlight();
};

#endif
