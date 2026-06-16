#include "bridge_utils.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <cstdlib>

namespace lith_bridge {

QString resolve_bridge_binary_path()
{
    if (const auto* env = std::getenv("LITHEDB_BRIDGE_BIN")) {
        return QString::fromLocal8Bit(env);
    }

    const auto appDir = QCoreApplication::applicationDirPath();
#if defined(Q_OS_MACOS)
    const auto macBundleBridge = QDir(appDir).filePath("../Helpers/lithedb-bridge");
    if (QFileInfo::exists(macBundleBridge)) {
        return QFileInfo(macBundleBridge).canonicalFilePath();
    }
    const auto macBundleBridgeExe = QDir(appDir).filePath("../Helpers/lithedb-bridge.exe");
    if (QFileInfo::exists(macBundleBridgeExe)) {
        return QFileInfo(macBundleBridgeExe).canonicalFilePath();
    }
#elif defined(Q_OS_WIN)
    const auto packagedBridge = QDir(appDir).filePath("lithedb-bridge.exe");
    if (QFileInfo::exists(packagedBridge)) {
        return QFileInfo(packagedBridge).canonicalFilePath();
    }
#else
    const auto packagedBridge = QDir(appDir).filePath("../libexec/lithedb-bridge");
    if (QFileInfo::exists(packagedBridge)) {
        return QFileInfo(packagedBridge).canonicalFilePath();
    }
#endif

    const auto repoRoot = QDir::currentPath();
    const auto debugPath = QDir(repoRoot).filePath("target/debug/lithedb-bridge");
    if (QFileInfo::exists(debugPath)) {
        return debugPath;
    }
    return QDir(repoRoot).filePath("target/release/lithedb-bridge");
}

} // namespace lith_bridge

