#include "main_window.h"
#include "theme.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QPixmap>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("AutoSync"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("autosync.local"));
    QCoreApplication::setApplicationName(QStringLiteral("AutoSync"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    application.setStyle(QStringLiteral("Fusion"));
    application.setStyleSheet(AutoSyncTheme::darkStyleSheet());

    MainWindow window;
    window.show();

    // Hook discreto para testes visuais automatizados em ambientes sem monitor.
    const QString screenshotPath = qEnvironmentVariable("AUTOSYNC_SCREENSHOT");
    if (!screenshotPath.isEmpty()) {
        QTimer::singleShot(250, &application, [&application, &window, screenshotPath] {
            window.grab().save(screenshotPath);
            application.quit();
        });
    }
    return application.exec();
}
