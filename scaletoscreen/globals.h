#pragma once

#include <QObject>
#include <QMargins>

namespace scaleToScreen {
    Q_NAMESPACE

struct ScalerSettings {
    //IgnoreAspectRatio, KeepAspectRatio, KeepAspectRatioByExpanding
    Qt::AspectRatioMode aspectRatio{Qt::KeepAspectRatio};
    QMargins margins{};
};

enum class MatchMode {
    Ignore = 0,
    Exact,
    Substring,
    Regex
};
Q_ENUM_NS(MatchMode)

struct AppSettings {
    QString profile;
    bool autoScale;
};

} // namespace scaleToScreen
