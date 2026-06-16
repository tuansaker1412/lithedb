#pragma once

#include <QString>

class QueryEditorTabWidget;

namespace lith_models {

struct QueryTabState {
    QString id;
    QueryEditorTabWidget* widget = nullptr;
};

} // namespace lith_models
