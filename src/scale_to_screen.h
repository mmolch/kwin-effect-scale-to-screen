#pragma once

#include "effect/effect.h"
#include "input.h"
#include <QLoggingCategory>

namespace KWin {

Q_DECLARE_LOGGING_CATEGORY(lcScaleToScreen)

class ScaleToScreen : public Effect, public InputEventFilter
{
    Q_OBJECT
public:
    explicit ScaleToScreen();
    ~ScaleToScreen() override;

private:
    void updateScalingState();
    void startScaling();
    void stopScaling();
    bool shouldScale() const;
    void scaleActiveWindow();

    QRectF calculateTargetRect(QRectF screenGeometry, QRectF windowGeometry,
                               QMargins windowMargins, Qt::AspectRatioMode aspectRatio) const;
    void updateTargetRect();
    void updatePaintData();

    QPointF mapWindowToCursor(QPointF cursorPosition) const;
    void syncWindowToCursor(QPointF cursorPosition) const;
    bool shouldBlockInput(QPointF cursorPosition) const;

    void clearScreen();
    void renderScaledWindowItem(const KWin::RenderTarget &target, const KWin::RenderViewport &viewport, const Region &region);

    // Effect Interface
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &region, WindowPaintData &data) override;

    // InputEventFilter Interface
    bool pointerMotion(PointerMotionEvent *event) override;
    bool pointerButton(PointerButtonEvent *event) override;
    bool pointerAxis(KWin::PointerAxisEvent *event) override;

    void constrainPointer(QPointF pos);

private slots:
    // Scales / unscales the active window
    void toggleEffect();

    void onWindowAdded(EffectWindow *w);
    void onWindowDeleted(EffectWindow *w);
    void onWindowActivated(KWin::EffectWindow *w);

    void onKeepAboveChanged(bool keepAbove);
    void onFrameGeometryChanged(RectF geometry);
    void onFullScreenChanged();

private:
    struct Settings {
        //IgnoreAspectRatio, KeepAspectRatio, KeepAspectRatioByExpanding
        Qt::AspectRatioMode aspectRatio{Qt::KeepAspectRatio};
        QMargins margins{};
        bool noBorder{true};
    };

    struct State {
        EffectWindow *window{nullptr};
        bool isScaling{false};
        QRectF targetRect{};
        QPointF originalPosition{};
        bool originalNoBorder{false};
        bool originalKeepAbove{false};

        // Use this to determine whether window's size changed when geometry changes
        QSizeF windowSize;

        // Holds the transformation to scale and position the upscaled image
        WindowPaintData paintData;
    };

    Settings m_settings;
    State m_state;
};

} // namespace KWin
