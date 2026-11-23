/* Static import of the offscreen QPA plugin and default platform selection (BlockOS has no X). */
#include <QtPlugin>
#include <cstdlib>
Q_IMPORT_PLUGIN(QOffscreenIntegrationPlugin)
namespace { struct BosQpaEnv { BosQpaEnv() { setenv("QT_QPA_PLATFORM", "offscreen", 0); } } bos_qpa_env; }
