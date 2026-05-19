#ifndef MINIPASPLUS_CODEEDITORWITHLINENUMBER_H
#define MINIPASPLUS_CODEEDITORWITHLINENUMBER_H

#include <QPlainTextEdit>
#include <QWidget>

class CodeEditorWithLineNumber;

class LineNumberAreaWidget : public QWidget {
public:
    explicit LineNumberAreaWidget(CodeEditorWithLineNumber* editor);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    CodeEditorWithLineNumber* editor_;
};

class CodeEditorWithLineNumber : public QPlainTextEdit {
public:
    explicit CodeEditorWithLineNumber(QWidget* parent = nullptr);

    int lineNumberAreaWidth() const;
    void paintLineNumberArea(QPaintEvent* event);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void updateLineNumberAreaWidth();
    void updateLineNumberArea(const QRect& rect, int dy);

private:
    LineNumberAreaWidget* lineNumberArea_;
};

#endif
