#include "../include/PythonSyntaxHighlighter.h"

PythonSyntaxHighlighter::PythonSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent) {
  HighlightingRule rule;

  // 1. Keywords
  keywordFormat.setForeground(Qt::darkBlue);
  keywordFormat.setFontWeight(QFont::Bold);
  QStringList keywordPatterns;
  keywordPatterns << "\\bclass\\b" << "\\bdef\\b" << "\\breturn\\b"
                  << "\\bif\\b"
                  << "\\belse\\b" << "\\belif\\b" << "\\bwhile\\b"
                  << "\\bfor\\b"
                  << "\\bin\\b" << "\\bimport\\b" << "\\bfrom\\b"
                  << "\\bpass\\b"
                  << "\\btry\\b" << "\\bexcept\\b" << "\\braise\\b"
                  << "\\bfinally\\b"
                  << "\\bas\\b" << "\\bis\\b" << "\\bnot\\b" << "\\band\\b"
                  << "\\bor\\b"
                  << "\\bTrue\\b" << "\\bFalse\\b" << "\\bNone\\b"
                  << "\\blambda\\b";

  for (const QString &pattern : keywordPatterns) {
    rule.pattern = QRegularExpression(pattern);
    rule.format = keywordFormat;
    highlightingRules.append(rule);
  }

  // 2. Class Names (e.g. MyClass) - After 'class' keyword usually
  classFormat.setFontWeight(QFont::Bold);
  classFormat.setForeground(Qt::darkMagenta);
  rule.pattern =
      QRegularExpression("\\bQ[A-Za-z]+\\b"); // Qt classes convention
  rule.format = classFormat;
  highlightingRules.append(rule);

  // 3. Strings (Single and Double Quotes) - Using non-greedy match
  quotationFormat.setForeground(Qt::darkGreen);
  rule.pattern = QRegularExpression("\".*?\"");
  rule.format = quotationFormat;
  highlightingRules.append(rule);

  rule.pattern = QRegularExpression("'.*?'");
  rule.format = quotationFormat;
  highlightingRules.append(rule);

  // 4. Function Calls (e.g. myFunction())
  functionFormat.setFontItalic(true);
  functionFormat.setForeground(QColor(0, 100, 100)); // Teal
  rule.pattern = QRegularExpression("\\b[A-Za-z0-9_]+(?=\\()");
  rule.format = functionFormat;
  highlightingRules.append(rule);

  // 5. Special Variables (cq, result)
  QTextCharFormat specialVarFormat;
  specialVarFormat.setForeground(QColor(200, 100, 0)); // Orange/Brown
  specialVarFormat.setFontWeight(QFont::Bold);
  QStringList specialVars;
  specialVars << "\\bcq\\b" << "\\bresult\\b";
  for (const QString &pattern : specialVars) {
    rule.pattern = QRegularExpression(pattern);
    rule.format = specialVarFormat;
    highlightingRules.append(rule);
  }

  // 6. Numbers
  numberFormat.setForeground(Qt::darkRed);
  rule.pattern = QRegularExpression("\\b\\d+\\b");
  rule.format = numberFormat;
  highlightingRules.append(rule);

  // 7. Comments
  singleLineCommentFormat.setForeground(Qt::gray);
  rule.pattern = QRegularExpression("#[^\n]*");
  rule.format = singleLineCommentFormat;
  highlightingRules.append(rule);
}

void PythonSyntaxHighlighter::highlightBlock(const QString &text) {
  for (const HighlightingRule &rule : highlightingRules) {
    QRegularExpressionMatchIterator matchIterator =
        rule.pattern.globalMatch(text);
    while (matchIterator.hasNext()) {
      QRegularExpressionMatch match = matchIterator.next();
      setFormat(match.capturedStart(), match.capturedLength(), rule.format);
    }
  }

  // Multi-line strings not fully implemented for simplicity, but basic
  // structure is here
}
