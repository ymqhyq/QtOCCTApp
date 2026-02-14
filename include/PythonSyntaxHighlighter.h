#ifndef PYTHONSYNTAXHIGHLIGHTER_H
#define PYTHONSYNTAXHIGHLIGHTER_H

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>

class PythonSyntaxHighlighter : public QSyntaxHighlighter {
  Q_OBJECT

public:
  explicit PythonSyntaxHighlighter(QTextDocument *parent = nullptr);

protected:
  void highlightBlock(const QString &text) override;

private:
  struct HighlightingRule {
    QRegularExpression pattern;
    QTextCharFormat format;
  };
  QVector<HighlightingRule> highlightingRules;

  QTextCharFormat keywordFormat;
  QTextCharFormat classFormat;
  QTextCharFormat singleLineCommentFormat;
  QTextCharFormat quotationFormat;
  QTextCharFormat functionFormat;
  QTextCharFormat numberFormat;
  QTextCharFormat operatorFormat;
};

#endif // PYTHONSYNTAXHIGHLIGHTER_H
