#include "CodeEditorWithLineNumber.h"
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTextBlock>

// 这个文件只做“代码编辑器的显示增强”：
// 1. 左侧显示行号；
// 2. 编辑器滚动时行号同步滚动；
// 3. 每行底部画一条很浅的分隔线。
// 它不参与词法、语法、语义分析。

LineNumberAreaWidget::LineNumberAreaWidget(CodeEditorWithLineNumber* editor)
    : QWidget(editor), editor_(editor) {
}

QSize LineNumberAreaWidget::sizeHint() const {
    // Qt 布局系统会调用 sizeHint，询问这个控件“你希望多大”。
    // 行号区域宽度由编辑器根据当前总行数计算。
    return QSize(editor_->lineNumberAreaWidth(), 0);
}

void LineNumberAreaWidget::paintEvent(QPaintEvent* event) {
    // 真正的绘制交给 CodeEditorWithLineNumber，避免行号控件重复保存编辑器状态。
    editor_->paintLineNumberArea(event);
}

CodeEditorWithLineNumber::CodeEditorWithLineNumber(QWidget* parent)
    : QPlainTextEdit(parent), lineNumberArea_(new LineNumberAreaWidget(this)) {
    // 文本行数变化时，行号区域宽度可能变化。
    connect(this, &QPlainTextEdit::blockCountChanged, this, [this]() {
        updateLineNumberAreaWidth();
    });

    // 编辑器滚动或局部重绘时，同步刷新行号区域。
    connect(this, &QPlainTextEdit::updateRequest, this, [this](const QRect& rect, int dy) {
        updateLineNumberArea(rect, dy);
    });

    updateLineNumberAreaWidth();
}

int CodeEditorWithLineNumber::lineNumberAreaWidth() const {
    // 计算最大行号需要几位数字。
    int digits = 1;
    int maxLine = qMax(1, blockCount());
    while (maxLine >= 10) {
        maxLine /= 10;
        ++digits;
    }

    const int margin = 10;
    return margin + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void CodeEditorWithLineNumber::paintLineNumberArea(QPaintEvent* event) {
    QPainter painter(lineNumberArea_);
    painter.fillRect(event->rect(), QColor(245, 245, 245));

    // QTextBlock 表示一行文本；firstVisibleBlock 是当前可见的第一行。
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            // 只绘制可见区域内的行号，避免无意义重绘。
            const QString lineNumber = QString::number(blockNumber + 1);
            painter.setPen(QColor(120, 120, 120));
            painter.drawText(0,
                             top,
                             lineNumberArea_->width() - 5,
                             fontMetrics().height(),
                             Qt::AlignRight,
                             lineNumber);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void CodeEditorWithLineNumber::resizeEvent(QResizeEvent* event) {
    QPlainTextEdit::resizeEvent(event);

    // 编辑器大小变化时，把行号区域固定到左边。
    const QRect contentRect = contentsRect();
    lineNumberArea_->setGeometry(
        QRect(contentRect.left(),
              contentRect.top(),
              lineNumberAreaWidth(),
              contentRect.height())
    );
}

void CodeEditorWithLineNumber::paintEvent(QPaintEvent* event) {
    // 先让父类把文本内容正常画出来。
    QPlainTextEdit::paintEvent(event);

    // 再额外画每行底部的浅灰分隔线。
    QPainter painter(viewport());
    painter.setPen(QColor(235, 235, 235));

    QTextBlock block = firstVisibleBlock();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            painter.drawLine(0, bottom - 1, viewport()->width(), bottom - 1);
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
    }
}

void CodeEditorWithLineNumber::updateLineNumberAreaWidth() {
    // 设置左边距，让正文从行号右侧开始显示。
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditorWithLineNumber::updateLineNumberArea(const QRect& rect, int dy) {
    if (dy != 0) {
        // dy != 0 表示发生滚动，行号区域跟着滚动即可。
        lineNumberArea_->scroll(0, dy);
    } else {
        // 没滚动时，只刷新被 Qt 标记为需要更新的局部区域。
        lineNumberArea_->update(0, rect.y(), lineNumberArea_->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth();
    }
}
