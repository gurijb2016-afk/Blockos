/* Real Qt widget test: QApplication + QLabel on the offscreen platform, quits after 200 ms. */
#include <QApplication>
#include <QLabel>
#include <QTimer>
#include <cstdio>
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    std::printf("platform=%s\n", qPrintable(QGuiApplication::platformName()));
    QLabel label(QStringLiteral("BlockOS Qt offscreen OK"));
    label.resize(400, 100);
    label.show();
    std::printf("label visible=%d\n", int(label.isVisible()));
    QTimer::singleShot(200, &app, &QCoreApplication::quit);
    int rc = app.exec();
    std::printf("exec rc=%d\n", rc);
    return rc;
}
