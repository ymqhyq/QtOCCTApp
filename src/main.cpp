#include <QApplication>
#include <QMetaObject>
#include "MainWindow.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static BOOL WINAPI consoleCtrlHandler(DWORD ctrlType) {
    // 只把 Ctrl+C / Ctrl+Break 转成优雅关闭请求。
    // CTRL_CLOSE_EVENT / CTRL_LOGOFF_EVENT / CTRL_SHUTDOWN_EVENT 交回默认处理：
    // 系统只给约 5 秒，排队到主事件循环的关闭请求很可能来不及执行，
    // 一旦吞掉默认行为，进程退出反而会被卡住。
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT) {
        if (qApp) {
            QMetaObject::invokeMethod(qApp, &QApplication::closeAllWindows, Qt::QueuedConnection);
        }
        return TRUE;
    }
    return FALSE;
}
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    // 主题等界面偏好按应用名保存（Windows 下写入 HKCU\\Software\\QtOCCTApp）
    QApplication::setOrganizationName("QtOCCTApp");
    QApplication::setApplicationName("QtOCCTApp");

#ifdef _WIN32
    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
#endif

    MainWindow window;
    window.show();

    return app.exec();
}
