// Qt widgets + FreeType text, rendered offscreen, copied to the BlockOS display framebuffer.
#include <QApplication>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include <QFontInfo>
#include <QImage>
#include <QPixmap>
#include <QPalette>
#include <QColor>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define BLOCKOS_FB_MAGIC 0x424F5346u
typedef struct {
    uint32_t magic, version;
    uint64_t framebuffer_phys, framebuffer_size;
    uint32_t width, height, stride, bpp, depth;
} BlockOSDisplayInfo;

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    QWidget root;
    root.setFixedSize(1024, 300);
    QPalette rp = root.palette();
    rp.setColor(QPalette::Window, QColor(0x2b, 0x3a, 0x4a));
    root.setPalette(rp);
    root.setAutoFillBackground(true);

    QLabel label(QStringLiteral("BlockOS: Árvíztűrő tükörfúrógép"));
    QFont f = label.font();
    f.setPointSize(28);
    label.setFont(f);
    QPalette lp = label.palette();
    lp.setColor(QPalette::WindowText, QColor(255, 255, 255));
    label.setPalette(lp);
    label.setAlignment(Qt::AlignCenter);

    QVBoxLayout *vl = new QVBoxLayout(&root);
    vl->addWidget(&label);

    root.show();
    for (int i = 0; i < 20; i++) app.processEvents();

    QFontInfo fi(label.font());
    std::printf("fttest: family=%s pixelSize=%d\n", qPrintable(fi.family()), fi.pixelSize());
    std::fflush(stdout);

    QImage img = root.grab().toImage().convertToFormat(QImage::Format_RGB32);
    std::printf("fttest: grabbed %dx%d\n", img.width(), img.height());

    BlockOSDisplayInfo d;
    std::memset(&d, 0, sizeof(d));
    int ifd = open("/system/display.info", O_RDONLY);
    if (ifd < 0 || read(ifd, &d, sizeof(d)) != (ssize_t)sizeof(d) || d.magic != BLOCKOS_FB_MAGIC) {
        std::printf("fttest: no display info\n");
        return 1;
    }
    close(ifd);
    int ffd = open("/devices/display", O_RDWR);
    if (ffd < 0) { std::printf("fttest: no display device\n"); return 2; }
    size_t bytes = (size_t)d.stride * 4 * d.height;
    uint8_t *fb = (uint8_t *)mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, ffd, 0);
    if (fb == MAP_FAILED) { std::printf("fttest: mmap failed\n"); return 3; }

    int rows = img.height() < (int)d.height ? img.height() : (int)d.height;
    int cols = img.width() < (int)d.width ? img.width() : (int)d.width;
    for (int y = 0; y < rows; y++) {
        // stride is in PIXELS: one row is stride*4 bytes
        std::memcpy(fb + (size_t)y * d.stride * 4, img.constScanLine(y), (size_t)cols * 4);
    }
    std::printf("fttest: blitted %dx%d to %ux%u stride=%u\n", cols, rows, d.width, d.height, d.stride);
    std::fflush(stdout);
    sleep(60);
    return 0;
}
