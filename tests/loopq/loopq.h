#pragma once
#include <QThread>
#include <QObject>
#include <cstdio>
#include <cstdlib>
class Svc : public QObject {
    Q_OBJECT
public slots:
    void work(int *out) {
        std::printf("slot: start\n"); std::fflush(stdout);
        void *p = std::malloc(64);
        std::printf("slot: malloc=%p\n", p); std::fflush(stdout);
        int *ip = new int(7);
        std::printf("slot: new int=%p\n", (void *)ip); std::fflush(stdout);
        *out = 42;
        std::printf("slot: done\n"); std::fflush(stdout);
    }
};
