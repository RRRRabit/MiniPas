#ifndef MINIPASPLUS_CODEEDITORWITHLINENUMBER_H
#define MINIPASPLUS_CODEEDITORWITHLINENUMBER_H

#include <QPlainTextEdit>
#include <QWidget>

class CodeEditorWithLineNumber;

// 行号区域。
// 它是一个很窄的 QWidget，贴在代码编辑器左侧，只负责绘制 1、2、3... 这些行号。
class LineNumberAreaWidget : public QWidget {
public:
    explicit LineNumberAreaWidget(CodeEditorWithLineNumber* editor);

    // 告诉 Qt：这个小控件期望的宽度是多少。
    QSize sizeHint() const override;

protected:
    // Qt 需要重绘行号区域时，会自动调用 paintEvent。
    void paintEvent(QPaintEvent* event) override;

private:
    // 保存对应的代码编辑器指针，因为真正的绘制逻辑放在编辑器类里。
    CodeEditorWithLineNumber* editor_;
};

// 带行号的代码编辑器。
// 它继承 QPlainTextEdit，比普通文本框多了左侧行号和横线辅助显示。
class CodeEditorWithLineNumber : public QPlainTextEdit {
public:
    explicit CodeEditorWithLineNumber(QWidget* parent = nullptr);

    // 计算行号区域需要多宽。
    // 例如 9 行以内只需要 1 位数字，100 行以内需要 3 位数字。
    int lineNumberAreaWidth() const;

    // 绘制左侧行号区域。
    void paintLineNumberArea(QPaintEvent* event);

protected:
    // 编辑器尺寸变化时，同步调整行号区域的位置和高度。
    void resizeEvent(QResizeEvent* event) override;

    // 绘制编辑器本身，额外加上每行底部的浅色分隔线。
    void paintEvent(QPaintEvent* event) override;

private:
    // 根据当前总行数，更新文本区左边距，避免正文盖住行号。
    void updateLineNumberAreaWidth();

    // 文本滚动或局部刷新时，只刷新必要的行号区域。
    void updateLineNumberArea(const QRect& rect, int dy);

private:
    LineNumberAreaWidget* lineNumberArea_;
};

#endif
