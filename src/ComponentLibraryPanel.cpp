#include "ComponentLibraryPanel.h"
#include <QLabel>
#include <QMessageBox>

ComponentLibraryPanel::ComponentLibraryPanel(QWidget* parent)
    : QDockWidget(parent)
{
    setWindowTitle(tr("构件库"));
    setObjectName("ComponentLibraryPanel");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    initUi();
}

ComponentLibraryPanel::~ComponentLibraryPanel()
{
}

void ComponentLibraryPanel::initUi()
{
    QWidget* mainWidget = new QWidget(this);
    mainWidget->setStyleSheet("background-color: #2b2b2b; color: #eee;");
    QVBoxLayout* layout = new QVBoxLayout(mainWidget);
    layout->setContentsMargins(5, 5, 5, 5);

    m_tabWidget = new QTabWidget(mainWidget);
    m_tabWidget->setStyleSheet(
        "QTabWidget::panel { border: 1px solid #444; background: #222; }"
        "QTabBar::tab { background: #333; color: #aaa; padding: 8px 12px; border: 1px solid #444; border-bottom: none; }"
        "QTabBar::tab:selected { background: #222; color: #00aaff; border-bottom: 2px solid #00aaff; }"
    );

    // 1. 参数化构件 Tab
    m_parametricListWidget = new QListWidget(this);
    m_parametricListWidget->setStyleSheet(
        "QListWidget { background-color: #222; border: none; padding: 5px; outline: none; }"
        "QListWidget::item { height: 32px; padding-left: 10px; color: #ddd; }"
        "QListWidget::item:hover { background-color: #333; color: #00aaff; }"
        "QListWidget::item:selected { background-color: #00aaff; color: #fff; outline: none; }"
    );
    m_tabWidget->addTab(m_parametricListWidget, tr("参数化构件"));

    // 2. 非参数化构件 Tab
    m_nonParametricListWidget = new QListWidget(this);
    m_nonParametricListWidget->setStyleSheet(
        "QListWidget { background-color: #222; border: none; padding: 5px; outline: none; }"
        "QListWidget::item { height: 32px; padding-left: 10px; color: #ddd; }"
        "QListWidget::item:hover { background-color: #333; color: #00aaff; }"
        "QListWidget::item:selected { background-color: #00aaff; color: #fff; outline: none; }"
    );
    m_tabWidget->addTab(m_nonParametricListWidget, tr("非参数化(IFC)"));

    layout->addWidget(m_tabWidget);
    setWidget(mainWidget);

    // 填充数据
    populateParametricList();
    refreshNonParametricList();

    // 信号槽连接
    connect(m_parametricListWidget, &QListWidget::itemDoubleClicked, this, &ComponentLibraryPanel::onItemDoubleClicked);
    connect(m_nonParametricListWidget, &QListWidget::itemDoubleClicked, this, &ComponentLibraryPanel::onItemDoubleClicked);
}

void ComponentLibraryPanel::populateParametricList()
{
    m_parametricListWidget->clear();

    QStringList list = {
        "Girder",
        "PierBody",
        "PierTray",
        "Pile",
        "PileCap",
        "bearing",
        "BedStone"
    };

    for (const QString& name : list) {
        QListWidgetItem* item = new QListWidgetItem(name, m_parametricListWidget);
        item->setData(Qt::UserRole, name); // 保存标识名
    }
}

void ComponentLibraryPanel::refreshNonParametricList()
{
    m_nonParametricListWidget->clear();

    // 检索构件库目录
    QDir dir("D:/QtOCCTApp/构件库");
    if (!dir.exists()) {
        QListWidgetItem* item = new QListWidgetItem(tr("构件库目录不存在"), m_nonParametricListWidget);
        item->setFlags(Qt::NoItemFlags);
        return;
    }

    QStringList filters;
    filters << "*.ifc";
    dir.setNameFilters(filters);
    dir.setFilter(QDir::Files | QDir::NoSymLinks);
    dir.setSorting(QDir::Name);

    QFileInfoList fileList = dir.entryInfoList();
    for (const QFileInfo& fileInfo : fileList) {
        // 显示纯文件名（不含 .ifc 后缀），美观简洁
        QString displayName = fileInfo.completeBaseName();
        QListWidgetItem* item = new QListWidgetItem(displayName, m_nonParametricListWidget);
        item->setData(Qt::UserRole, fileInfo.fileName()); // 存储完整的带后缀文件名
    }

    if (m_nonParametricListWidget->count() == 0) {
        QListWidgetItem* item = new QListWidgetItem(tr("无可用 IFC 构件"), m_nonParametricListWidget);
        item->setFlags(Qt::NoItemFlags);
    }
}

void ComponentLibraryPanel::onItemDoubleClicked(QListWidgetItem* item)
{
    if (!item) return;

    QString dataVal = item->data(Qt::UserRole).toString();
    if (dataVal.isEmpty()) return;

    // 根据当前所在的 Tab 判断构件类别
    QString category = (m_tabWidget->currentWidget() == m_parametricListWidget) ? "parametric" : "nonparametric";

    emit componentSelected(category, dataVal);
}
