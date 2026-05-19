#pragma once

#include "globals.h"
#include "scaler.h"
#include "effect/effect.h"
#include "input.h"
#include <KConfig>
#include <QLoggingCategory>

using namespace KWin;

namespace scaleToScreen {

Q_DECLARE_LOGGING_CATEGORY(lcScaleToScreen)

class ScaleToScreen : public Effect, public InputEventFilter
{
    Q_OBJECT
public:
    explicit ScaleToScreen();
    ~ScaleToScreen() override;

    AppSettings getAppSettings(EffectWindow *w) const;
    ScalerSettings getScalerSettings(const QString &profile) const;

    void toggleActiveWindow();
    void addScaler(EffectWindow *w, std::optional<ScalerSettings> settings = std::nullopt);
    ScalerPtr removeScaler(EffectWindow *w);
    Scaler *findScaler(EffectWindow *w) const;

    void clearScreen();

    // Effect Interface
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &region, WindowPaintData &data) override;
    void reconfigure(ReconfigureFlags flags) override;

    bool shouldBlockInput(QPointF cursorPosition) const;
    bool handlePointer(const QPointF &pos);

    // InputEventFilter Interface
    bool pointerMotion(PointerMotionEvent *event) override;
    bool pointerButton(PointerButtonEvent *event) override;
    bool pointerAxis(KWin::PointerAxisEvent *event) override;

private slots:
    void onWindowAdded(EffectWindow *w);
    void onWindowDeleted(EffectWindow *w);
    void onWindowActivated(KWin::EffectWindow *w);

private:
    std::unordered_map<EffectWindow*, ScalerPtr> m_suspendedScalers;
    std::unordered_map<EffectWindow*, ScalerPtr> m_scalers;
    KConfig m_config;

    friend class Scaler;
};

} // namespace scaleToScreen
