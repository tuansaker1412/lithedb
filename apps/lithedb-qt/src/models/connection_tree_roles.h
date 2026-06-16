#pragma once

#include <Qt>

namespace lith_models {

inline constexpr int RoleKind = Qt::UserRole + 1;
inline constexpr int RoleConnectionId = Qt::UserRole + 2;
inline constexpr int RoleDatabase = Qt::UserRole + 3;
inline constexpr int RoleTable = Qt::UserRole + 4;
inline constexpr int RoleDriver = Qt::UserRole + 5;
inline constexpr int RoleBaseName = Qt::UserRole + 6;
inline constexpr int RoleHost = Qt::UserRole + 7;
inline constexpr int RolePort = Qt::UserRole + 8;
inline constexpr int RoleUsername = Qt::UserRole + 9;
inline constexpr int RoleSsl = Qt::UserRole + 10;

} // namespace lith_models
