/* BOS TEMPORARY: minimal QFreeTypeFontDatabase. No FreeType, no font files on BlockOS yet, so text renders as boxes.
 * Replace with the real freetype + QFontEngineFT build when fonts are in the rootfs. */
#include <QtFontDatabaseSupport/private/qfreetypefontdatabase_p.h>
#include <private/qfontengine_p.h>
void QFreeTypeFontDatabase::populateFontDatabase() {}
QFontEngine *QFreeTypeFontDatabase::fontEngine(const QFontDef &fontDef, void *) { return new QFontEngineBox(int(fontDef.pixelSize)); }
QFontEngine *QFreeTypeFontDatabase::fontEngine(const QByteArray &, qreal pixelSize, QFont::HintingPreference) { return new QFontEngineBox(int(pixelSize)); }
QStringList QFreeTypeFontDatabase::addApplicationFont(const QByteArray &, const QString &) { return QStringList(); }
void QFreeTypeFontDatabase::releaseHandle(void *) {}
QStringList QFreeTypeFontDatabase::addTTFile(const QByteArray &, const QByteArray &) { return QStringList(); }
