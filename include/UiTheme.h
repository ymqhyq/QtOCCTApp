#ifndef UITHEME_H
#define UITHEME_H

#include <QColor>
#include <QIcon>
#include <QString>

#include "SARibbonGlobal.h"

/**
 * @brief 界面色调
 *
 * Ribbon 自身的配色由 SARibbon 主题（SARibbonTheme）负责，本枚举描述的是
 * Ribbon 之外、由应用自己绘制的部分（停靠面板、属性面板、脚本编辑器、
 * 构件库列表）以及按钮图标应该采用的明暗基调。
 */
enum class UiColorScheme { Light, Dark };

/**
 * @brief 应用自有控件的配色表
 */
struct UiPalette {
  QColor windowBg;       ///< 停靠面板标题栏/窗口底色
  QColor panelBg;        ///< 面板内容区底色
  QColor itemHoverBg;    ///< 列表项悬停底色
  QColor itemSelectedBg; ///< 列表项选中底色
  QColor textPrimary;    ///< 主要文字
  QColor textSecondary;  ///< 次要/说明文字
  QColor border;         ///< 分隔线、边框
  QColor accent;         ///< 强调色（分组标题、选中态）
  QColor editorBg;       ///< 输入框/只读值底色
  QColor iconNormal;     ///< 按钮图标主色
  QColor iconActive;     ///< 悬停/按下时的图标色
  QColor iconDisabled;   ///< 禁用态图标色
};

/// 取得指定色调的配色表
const UiPalette &uiPalette(UiColorScheme scheme);

/// 由 SARibbon 主题推导界面色调（Office 2016/2021 深色、Dark、Dark2 视为深色）
UiColorScheme uiSchemeForRibbonTheme(SARibbonTheme theme);

/**
 * @brief 生成跟随主题的按钮图标
 *
 * 图标资源为 SVG，描边使用 currentColor 占位；此处把占位符替换为指定颜色后
 * 交给 QSvgRenderer 渲染，因此切换主题时只需重新调用本函数即可换色。
 */
QIcon themedIcon(const QString &svgResourcePath, const QColor &color);

/// 同上，但悬停/选中态使用 active 色
QIcon themedIcon(const QString &svgResourcePath, const QColor &normal,
                 const QColor &active);

/**
 * @brief 同上，但把着色后的 SVG 写入临时文件并返回路径
 * 用于 QSS 的 url()（例如复选框的勾选标记）。
 */
QString themedIconFileUrl(const QString &svgResourcePath, const QColor &color);

// —— 自绘控件样式表（由配色表生成，切换主题时重新应用）——
QString checkBoxStyle(const UiPalette &p);
QString comboBoxStyle(const UiPalette &p);
QString snippetEditorStyle(const UiPalette &p);
QString dockWidgetStyle(const UiPalette &p);
QString modelTreeStyle(const UiPalette &p);

#endif // UITHEME_H
