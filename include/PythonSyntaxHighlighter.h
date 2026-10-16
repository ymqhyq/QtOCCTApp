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

  /// 切换配色方案：深色编辑器背景用浅色系语法色，浅色背景用深色系
  void setDarkScheme(bool dark);
  bool isDarkScheme() const { return m_darkScheme; }

protected:
  void highlightBlock(const QString &text) override;

private:
  void buildRules(bool dark);

  struct HighlightingRule {
    QRegularExpression pattern;
    QTextCharFormat format;
  };
  QVector<HighlightingRule> highlightingRules;
  bool m_darkScheme = false;

  QTextCharFormat keywordFormat;
  QTextCharFormat classFormat;
  QTextCharFormat singleLineCommentFormat;
  QTextCharFormat quotationFormat;
  QTextCharFormat functionFormat;
  QTextCharFormat numberFormat;
  QTextCharFormat operatorFormat;
};

#endif // PYTHONSYNTAXHIGHLIGHTER_H
