#pragma once

#include "globals.h"
#include "util.h"
#include "effect/effect.h"
#include "window.h"
#include <QLoggingCategory>
#include <QMargins>
#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QSizeF>

namespace KWin {
class EffectWindow;
}

using namespace KWin;

namespace scaleToScreen {

Q_DECLARE_LOGGING_CATEGORY(lcScaleToScreen)

class ScaleToScreen;

class Scaler : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Scaler)

public:
    // Whether we draw the scaled image or not (will be a borderless window otherwise)
    Q_PROPERTY(bool isScaling READ isScaling WRITE setScaling NOTIFY isScalingChanged FINAL)

    explicit Scaler(ScaleToScreen &effect, EffectWindow &window, ScalerSettings settings);
    ~Scaler() override;

    bool isScaling() const;
    void constrainCursor(QPointF pos) const;
    void syncWindowToCursor(QPointF cursorPosition) const;
    void renderScaledWindowItem(const RenderTarget &target, const RenderViewport &viewport, const Region &region);

public slots:
    void setScaling(bool scaled);

private slots:
    void onDecorationPolicyChanged();
    void onKeepAboveChanged();
    void onFrameGeometryChanged(RectF geometry);
    void onFullScreenChanged();

signals:
    void isScalingChanged(bool scaled);

private:
    QRectF calculateTargetRect(QRectF screenGeometry, QRectF windowGeometry,
                               QMargins windowMargins, Qt::AspectRatioMode aspectRatio) const;
    void updateTargetRect();
    void updatePaintData();

    QPointF mapWindowToCursor(QPointF cursorPosition) const;

    struct State {
        bool isScaling{false};
        QRectF targetRect{};
        QPointF originalPosition{};
        bool originalNoBorder{false};
        bool originalKeepAbove{false};
        // Use this to determine whether window's size changed when geometry changes
        QSizeF windowSize;
        // Holds the transformation to scale and position the upscaled image
        WindowPaintData paintData;
        bool windowDeleted{false};
    };

    ScaleToScreen &m_effect;
    EffectWindow &m_window;
    const ScalerSettings m_settings;
    State m_state;

    friend class ScaleToScreen;
};

using ScalerPtr = unique_qptr<Scaler>;

} // namespace scaleToScreen
