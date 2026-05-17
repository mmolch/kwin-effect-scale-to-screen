#pragma once

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

} // namespace scaleToScreen
