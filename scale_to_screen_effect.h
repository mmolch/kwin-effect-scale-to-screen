#pragma once

#include "effect/effect.h"
#include "input.h"

namespace KWin {

class ScaleToScreenEffect : public Effect, public InputEventFilter
{
    Q_OBJECT
public:
    explicit ScaleToScreenEffect();
    ~ScaleToScreenEffect() override;

private:
    void setEnabled(EffectWindow *w, bool enabled);

    // Maps the window position to the targetRect so that
    // the window overlaps at the cursor position in the targetRect
    QPointF mapWindowToCursor(EffectWindow *w, QPointF cursorPosition) const;
    void syncWindowToCursor(QPointF cursorPosition) const;
    EffectWindow *findWindowAtCursor(QPointF cursorPosition) const;

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
        Qt::AspectRatioMode aspectRatio{Qt::KeepAspectRatio};
        QMargins margins{};
    };

    struct State {
        bool isEnabled{false};
        QRectF targetRect{100, 100, 400, 300}; // Testing
    };

    struct ScaleData {
        Settings settings{};
        State state{};
    };

    std::unordered_map<EffectWindow *, ScaleData> m_scaledWindows;
};

} // namespace KWin
