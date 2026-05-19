#pragma once

#include "globals.h"
#include <QMargins>
#include <QObject>
#include <memory>

namespace scaleToScreen {

struct QObjectDeleter {
    void operator()(QObject *obj) const {
        if (obj) obj->deleteLater();
    }
};

template <typename T>
using unique_qptr = std::unique_ptr<T, QObjectDeleter>;

bool matchStringWithMode(const QString &text, const QString &pattern, MatchMode mode);

} // namespace scaleToScreen
