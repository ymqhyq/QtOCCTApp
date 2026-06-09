#ifndef COMPONENTLIBRARYPANEL_H
#define COMPONENTLIBRARYPANEL_H

#include <QDockWidget>
#include <QListWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QDir>
#include <QFileInfo>
#include <QListWidgetItem>

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
     * @brief 重新扫描构件库并刷新非参数化构件列表
     */
    void refreshNonParametricList();

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
};

#endif // COMPONENTLIBRARYPANEL_H
