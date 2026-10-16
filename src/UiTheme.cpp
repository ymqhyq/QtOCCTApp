#include "../include/UiTheme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIconEngine>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QSvgRenderer>

// ---------------------------------------------------------------------------
// 配色表
// ---------------------------------------------------------------------------

static const UiPalette kLightPalette = {
    QColor("#f3f4f6"), // windowBg
    QColor("#ffffff"), // panelBg
    QColor("#e8eef5"), // itemHoverBg
    QColor("#0078d7"), // itemSelectedBg
    QColor("#1f2329"), // textPrimary
    QColor("#5f6b7a"), // textSecondary
    QColor("#c9ced6"), // border
    QColor("#0078d7"), // accent
    QColor("#ffffff"), // editorBg
    QColor("#3c4450"), // iconNormal
    QColor("#0078d7"), // iconActive
    QColor("#a8b0ba")  // iconDisabled
};

static const UiPalette kDarkPalette = {
    QColor("#2b2b2b"), // windowBg
    QColor("#222222"), // panelBg
    QColor("#333333"), // itemHoverBg
    QColor("#00aaff"), // itemSelectedBg
    QColor("#eeeeee"), // textPrimary
    QColor("#999999"), // textSecondary
    QColor("#444444"), // border
    QColor("#00aaff"), // accent
    QColor("#2a2a2a"), // editorBg
    QColor("#d7dde3"), // iconNormal
    QColor("#00aaff"), // iconActive
    QColor("#6b7480")  // iconDisabled
};

const UiPalette &uiPalette(UiColorScheme scheme) {
  return (scheme == UiColorScheme::Dark) ? kDarkPalette : kLightPalette;
}

UiColorScheme uiSchemeForRibbonTheme(SARibbonTheme theme) {
  switch (theme) {
  case SARibbonTheme::RibbonThemeDark:
  case SARibbonTheme::RibbonThemeDark2:
  case SARibbonTheme::RibbonThemeOffice2016Dark:
  case SARibbonTheme::RibbonThemeOffice2021Dark:
    return UiColorScheme::Dark;
  default:
    return UiColorScheme::Light;
  }
}

// ---------------------------------------------------------------------------
// SVG 着色
// ---------------------------------------------------------------------------

static QByteArray recoloredSvg(const QString &svgResourcePath,
                               const QColor &color) {
  QFile file(svgResourcePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return QByteArray();
  }
  QByteArray data = file.readAll();
  file.close();

  // 图标资源统一使用 currentColor 作为描边占位符
  data.replace("currentColor", color.name(QColor::HexRgb).toUtf8());
  return data;
}

/**
 * @brief 按指定颜色渲染 SVG 的图标引擎
 *
 * 相比"一次性把图标渲染成 QPixmap"，引擎方式可以在任意尺寸下保持矢量清晰度，
 * 并且能按 QIcon::Mode 给出悬停/禁用配色。
 */
class RecoloredSvgIconEngine : public QIconEngine {
public:
  RecoloredSvgIconEngine(const QString &svgResourcePath, const QColor &normal,
                         const QColor &active)
      : m_resourcePath(svgResourcePath), m_normal(normal), m_active(active) {
    m_normalRenderer.load(recoloredSvg(svgResourcePath, normal));
    if (active.isValid() && active != normal) {
      m_activeRenderer.load(recoloredSvg(svgResourcePath, active));
    }
  }

  void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode,
             QIcon::State state) override {
    Q_UNUSED(state);
    if (painter == nullptr) {
      return;
    }
    QSvgRenderer *renderer = &m_normalRenderer;
    if ((mode == QIcon::Active || mode == QIcon::Selected) &&
        m_activeRenderer.isValid()) {
      renderer = &m_activeRenderer;
    }
    if (!renderer->isValid()) {
      return;
    }
    painter->save();
    if (mode == QIcon::Disabled) {
      painter->setOpacity(0.45);
    }
    renderer->render(painter, rect);
    painter->restore();
  }

  QPixmap pixmap(const QSize &size, QIcon::Mode mode,
                 QIcon::State state) override {
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    paint(&painter, QRect(QPoint(0, 0), size), mode, state);
    return pixmap;
  }

  QIconEngine *clone() const override {
    return new RecoloredSvgIconEngine(m_resourcePath, m_normal, m_active);
  }

  QString key() const override {
    return QStringLiteral("qtocctapp-svg:%1:%2:%3")
        .arg(m_resourcePath, m_normal.name(QColor::HexArgb),
             m_active.name(QColor::HexArgb));
  }

private:
  QString m_resourcePath;
  QColor m_normal;
  QColor m_active;
  QSvgRenderer m_normalRenderer;
  QSvgRenderer m_activeRenderer;
};

QIcon themedIcon(const QString &svgResourcePath, const QColor &color) {
  return QIcon(new RecoloredSvgIconEngine(svgResourcePath, color, QColor()));
}

QIcon themedIcon(const QString &svgResourcePath, const QColor &normal,
                 const QColor &active) {
  return QIcon(new RecoloredSvgIconEngine(svgResourcePath, normal, active));
}

/**
 * @brief 把 SVG 数据写入临时目录并返回路径（供 QSS 的 url() 使用）
 * @param nameSuffix 同一图标的不同着色版本用不同后缀区分
 */
static QString writeTempSvg(const QString &baseName, const QString &nameSuffix,
                            const QByteArray &data) {
  QDir iconDir(QDir::tempPath() + QStringLiteral("/qtocctapp_icons"));
  if (!iconDir.exists() && !iconDir.mkpath(QStringLiteral("."))) {
    return QString();
  }

  QString safeName = baseName;
  safeName.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_]")),
                   QStringLiteral("_"));
  // 文件名里不能出现 '#'：QSS 的 url() 会把它当成片段分隔符
  QString safeSuffix = nameSuffix;
  safeSuffix.replace(QLatin1Char('#'), QLatin1Char('_'));
  const QString filePath =
      iconDir.filePath(QStringLiteral("%1_%2.svg").arg(safeName, safeSuffix));

  QFile out(filePath);
  if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return QString();
  }
  out.write(data);
  out.close();
  // QSS 的 url() 必须使用正斜杠，反斜杠会被当成转义符
  return QDir::fromNativeSeparators(filePath);
}

QString themedIconFileUrl(const QString &svgResourcePath, const QColor &color) {
  const QByteArray data = recoloredSvg(svgResourcePath, color);
  if (data.isEmpty()) {
    return svgResourcePath;
  }
  const QString path = writeTempSvg(QFileInfo(svgResourcePath).completeBaseName(),
                                    color.name(QColor::HexRgb), data);
  return path.isEmpty() ? svgResourcePath : path;
}

// ---------------------------------------------------------------------------
// 自绘控件样式表
// ---------------------------------------------------------------------------

QString checkBoxStyle(const UiPalette &p) {
  const QString checkUrl =
      themedIconFileUrl(QStringLiteral(":/resources/icons/check.svg"),
                        p.accent);
  QString style =
      QStringLiteral("QCheckBox { color: %1; font-size: 12px; }"
                     "QCheckBox::indicator {"
                     "  width: 14px; height: 14px;"
                     "  border: 1.5px solid %2;"
                     "  background-color: %3;"
                     "  border-radius: 2px;"
                     "}"
                     "QCheckBox::indicator:hover { border: 1.5px solid %4; }"
                     "QCheckBox::indicator:checked { image: url(%5); }");
  style = style.arg(p.textPrimary.name());
  style = style.arg(p.textSecondary.name());
  style = style.arg(p.editorBg.name());
  style = style.arg(p.accent.name());
  style = style.arg(checkUrl);
  return style;
}

QString comboBoxStyle(const UiPalette &p) {
  // 下拉箭头用内联 SVG 生成，跟随主题文字色；否则深色主题下默认黑箭头几乎看不见
  const QByteArray arrowSvg =
      QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\""
                     " viewBox=\"0 0 10 6\">"
                     "<path d=\"M0 0L5 6L10 0Z\" fill=\"%1\"/></svg>")
          .arg(p.textPrimary.name())
          .toUtf8();
  const QString arrowPath = writeTempSvg(
      QStringLiteral("ui_combo_arrow"), p.textPrimary.name(QColor::HexRgb),
      arrowSvg);

  QString style =
      QStringLiteral("QComboBox { color: %1; background-color: %2;"
                     " border: 1px solid %3; border-radius: 2px;"
                     " padding: 2px 6px; min-width: 108px; }"
                     "QComboBox:hover { border: 1px solid %4; }"
                     "QComboBox::drop-down { border: none; width: 20px; }"
                     "QComboBox::down-arrow { image: url(%6);"
                     " width: 10px; height: 6px; }"
                     "QComboBox QAbstractItemView { color: %1;"
                     " background-color: %2; selection-background-color: %4;"
                     " selection-color: %5; }");
  style = style.arg(p.textPrimary.name());
  style = style.arg(p.editorBg.name());
  style = style.arg(p.border.name());
  style = style.arg(p.accent.name());
  style = style.arg(p.panelBg.name());
  style = style.arg(arrowPath);
  return style;
}

QString snippetEditorStyle(const UiPalette &p) {
  QString style = QStringLiteral("QTextEdit { color: %1; background-color: %2;"
                                 " border: none; selection-background-color: %3;"
                                 " }");
  style = style.arg(p.textPrimary.name());
  style = style.arg(p.editorBg.name());
  style = style.arg(p.itemSelectedBg.name());
  return style;
}

QString dockWidgetStyle(const UiPalette &p) {
  QString style =
      QStringLiteral("QDockWidget { color: %1; }"
                     "QDockWidget::title { background-color: %2; color: %1;"
                     " padding: 4px 8px; border-bottom: 1px solid %3; }"
                     "QScrollArea { border: none; background-color: %4; }"
                     "QScrollArea > QWidget > QWidget { background-color: %4; }");
  style = style.arg(p.textPrimary.name());
  style = style.arg(p.windowBg.name());
  style = style.arg(p.border.name());
  style = style.arg(p.panelBg.name());
  return style;
}

QString modelTreeStyle(const UiPalette &p) {
  QString style =
      QStringLiteral("QTreeView { color: %1; background-color: %2; border: none;"
                     " outline: none; }"
                     "QTreeView::item { padding: 3px 2px; }"
                     "QTreeView::item:hover { background-color: %3; }"
                     "QTreeView::item:selected { background-color: %4;"
                     " color: %5; }"
                     "QHeaderView::section { background-color: %6; color: %1;"
                     " border: none; border-bottom: 1px solid %7;"
                     " padding: 4px; }");
  style = style.arg(p.textPrimary.name());
  style = style.arg(p.panelBg.name());
  style = style.arg(p.itemHoverBg.name());
  style = style.arg(p.itemSelectedBg.name());
  style = style.arg(p.panelBg.name());
  style = style.arg(p.windowBg.name());
  style = style.arg(p.border.name());
  return style;
}
