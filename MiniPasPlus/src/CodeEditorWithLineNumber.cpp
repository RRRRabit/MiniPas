#include "CodeEditorWithLineNumber.h"
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTextBlock>

LineNumberAreaWidget::LineNumberAreaWidget(CodeEditorWithLineNumber* editor)
    : QWidget(editor), editor_(editor) {
}

QSize LineNumberAreaWidget::sizeHint() const {
    return QSize(editor_->lineNumberAreaWidth(), 0);
}

void LineNumberAreaWidget::paintEvent(QPaintEvent* event) {
    editor_->paintLineNumberArea(event);
}

CodeEditorWithLineNumber::CodeEditorWithLineNumber(QWidget* parent)
    : QPlainTextEdit(parent), lineNumberArea_(new LineNumberAreaWidget(this)) {
    connect(this, &QPlainTextEdit::blockCountChanged, this, [this]() {
        updateLineNumberAreaWidth();
    });

    connect(this, &QPlainTextEdit::updateRequest, this, [this](const QRect& rect, int dy) {
        updateLineNumberArea(rect, dy);
    });

    updateLineNumberAreaWidth();
}

int CodeEditorWithLineNumber::lineNumberAreaWidth() const {
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

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
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

    const QRect contentRect = contentsRect();
    lineNumberArea_->setGeometry(
        QRect(contentRect.left(),
              contentRect.top(),
              lineNumberAreaWidth(),
              contentRect.height())
    );
}

void CodeEditorWithLineNumber::paintEvent(QPaintEvent* event) {
    QPlainTextEdit::paintEvent(event);

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
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditorWithLineNumber::updateLineNumberArea(const QRect& rect, int dy) {
    if (dy != 0) {
        lineNumberArea_->scroll(0, dy);
    } else {
        lineNumberArea_->update(0, rect.y(), lineNumberArea_->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth();
    }
}
