#include <QApplication>
#include <QMetaObject>
#include "MainWindow.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

static BOOL WINAPI consoleCtrlHandler(DWORD ctrlType) {
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT || ctrlType == CTRL_CLOSE_EVENT) {
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

#ifdef _WIN32
    SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
#endif

    MainWindow window;
    window.show();

    return app.exec();
}