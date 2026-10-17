#ifndef COMPONENTLIBRARYPANEL_H
#define COMPONENTLIBRARYPANEL_H

#include <QDockWidget>
#include <QHash>
#include <QList>
#include <QListWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QDir>
#include <QFileInfo>
#include <QListWidgetItem>

#include "UiTheme.h"

class QAction;

/**
 * @brief 构件库菜单面板类
 * 用于列出所有参数化和非参数化构件，双击项即可向微服务发起几何请求并在三维视口中渲染展示。
 */
class ComponentLibraryPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit ComponentLibraryPanel(QWidget* parent = nullptr);
    ~ComponentLibraryPanel();

    /**
     * @brief 应用界面配色（切换主题时调用）
     */
    void setUiPalette(const UiPalette& palette);

    /**
     * @brief 重新扫描构件库并刷新非参数化构件列表
     */
    void refreshNonParametricList();

    /**
     * @brief 用真实 QAction 填充参数化构件页
     *
     * 构件项双击时直接触发对应 QAction，保证与 Ribbon 按钮的行为一致，
     * 同时让图标能够随主题切换同步刷新。
     */
    void setParametricActions(const QList<QAction*>& actions);
    int parametricActionCount() const { return m_itemActions.size(); }

signals:
    /**
     * @brief 当选择并请求展示构件时发射的信号
     * @param category 构件大类 ("parametric" 或 "nonparametric")
     * @param name 构件对应名称或 IFC 文件名 (例如 "Girder" 或 "53-15 90 45 30_应答器.ifc")
     */
    void componentSelected(const QString& category, const QString& name);

private slots:
    void onItemDoubleClicked(QListWidgetItem* item);

private:
    void initUi();
    void populateParametricList();

    QTabWidget* m_tabWidget;
    QListWidget* m_parametricListWidget;
    QListWidget* m_nonParametricListWidget;
    QHash<QListWidgetItem*, QAction*> m_itemActions;
};

#endif // COMPONENTLIBRARYPANEL_H
