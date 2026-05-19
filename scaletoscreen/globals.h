#pragma once

#include <QMargins>
#include <QObject>

namespace scaleToScreen {
    Q_NAMESPACE

class CropMargins
{
public:
    constexpr CropMargins() = default;

    constexpr CropMargins(int left, int top, int right, int bottom) {
        setLeft(left);
        setTop(top);
        setRight(right);
        setBottom(bottom);
    }

    constexpr void setLeft(int left)     { m_margins.setLeft(std::max(0, left)); }
    constexpr void setTop(int top)       { m_margins.setTop(std::max(0, top)); }
    constexpr void setRight(int right)   { m_margins.setRight(std::max(0, right)); }
    constexpr void setBottom(int bottom) { m_margins.setBottom(std::max(0, bottom)); }

    constexpr int left() const   { return m_margins.left(); }
    constexpr int top() const    { return m_margins.top(); }
    constexpr int right() const  { return m_margins.right(); }
    constexpr int bottom() const { return m_margins.bottom(); }

    constexpr bool isNull() const { return m_margins.isNull(); }

    constexpr operator QMargins() const { return m_margins; }
    constexpr operator QMarginsF() const { return m_margins; }

private:
    QMargins m_margins{0, 0, 0, 0};
};

struct ScalerSettings {
    //IgnoreAspectRatio, KeepAspectRatio, KeepAspectRatioByExpanding
    Qt::AspectRatioMode aspectRatio{Qt::KeepAspectRatio};
    CropMargins cropMargins{};
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
