#include <QCoreApplication>
#include <QThread>
#include <QMetaObject>
#include <cstdio>
#include <cstdlib>
#include "loopq.h"
int main(int argc, char **argv) {
    std::printf("loopq: start\n"); std::fflush(stdout);
    QCoreApplication app(argc, argv);
    void *m0 = std::malloc(64);
    std::printf("loopq: main malloc=%p\n", m0); std::fflush(stdout);
    QThread th;
    Svc svc;
    svc.moveToThread(&th);
    th.start();
    std::printf("loopq: thread started\n"); std::fflush(stdout);
    int x = 0;
    QMetaObject::invokeMethod(&svc, "work", Qt::BlockingQueuedConnection, Q_ARG(int *, &x));
    std::printf("loopq: back x=%d\n", x); std::fflush(stdout);
    th.quit();
    th.wait();
    std::printf("loopq: done\n");
    return 0;
}
