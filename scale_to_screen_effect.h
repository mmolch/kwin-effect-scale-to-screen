#pragma once

#include "effect/effect.h"
#include "input.h"
#include <QLoggingCategory>

namespace KWin {

Q_DECLARE_LOGGING_CATEGORY(lcScaleToScreen)

class ScaleToScreenEffect : public Effect, public InputEventFilter
{
    Q_OBJECT
public:
    explicit ScaleToScreenEffect();
    ~ScaleToScreenEffect() override;

private:
    void setEnabled(EffectWindow *w, bool enabled);

    QRectF calculateTargetRect(QRectF screenGeometry, QRectF windowGeometry,
                               QMargins windowMargins, Qt::AspectRatioMode aspectRatio) const;
    void updateTargetRect(EffectWindow *w);

    // Maps the window position to the targetRect so that
    // the window overlaps at the cursor position in the targetRect
    QPointF mapWindowToCursor(EffectWindow *w, QPointF cursorPosition) const;
    bool syncWindowToCursor(QPointF cursorPosition) const;
    EffectWindow *findWindowAtCursor(QPointF cursorPosition) const;
    bool shouldBlockInput(QPointF cursorPosition) const;

    bool hasEnabledScalers() const;

    // InputEventFilter Interface
    bool pointerMotion(PointerMotionEvent *event) override;
    bool pointerButton(PointerButtonEvent *event) override;
    bool pointerAxis(KWin::PointerAxisEvent *event) override;


private slots:
    // Scales / unscales the active window
    void toggleActiveWindow();

    void onWindowAdded(EffectWindow *w);
    void onWindowDeleted(EffectWindow *w);

private:
    enum class Shader: int {
        Simple = 0
    };
    Q_ENUM(Shader)

    struct Settings {
        Shader shader{Shader::Simple};
        //IgnoreAspectRatio, KeepAspectRatio, KeepAspectRatioByExpanding
        Qt::AspectRatioMode aspectRatio{Qt::IgnoreAspectRatio};
        QMargins margins{};
        bool blockScreenInput{false};
        bool confinePointer{false};
        bool noBorder{true};
    };

    struct State {
        bool isEnabled{false};
        QRectF targetRect;
        QPointF originalPosition; // To restore the window position prior to scaling
        bool originalNoBorder;
        bool originalKeepAbove;
    };

    struct ScaleData {
        Settings settings{};
        State state{};
    };

    std::unordered_map<EffectWindow *, ScaleData> m_scaledWindows;
};

} // namespace KWin
