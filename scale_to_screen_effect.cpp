#include "scale_to_screen_effect.h"
#include "effect/effecthandler.h"
#include "effect/effectwindow.h"
#include "input_event.h"
#include "window.h"

namespace KWin {

ScaleToScreenEffect::ScaleToScreenEffect()
    : Effect()
    , InputEventFilter(InputFilterOrder::Effects)
{
    connect(effects, &EffectsHandler::windowAdded, this, &ScaleToScreenEffect::onWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted, this, &ScaleToScreenEffect::onWindowDeleted);
}

ScaleToScreenEffect::~ScaleToScreenEffect()
{
}

void ScaleToScreenEffect::setEnabled(EffectWindow *w, bool enabled)
{
    if (!w) {
        return;
    }

    if (m_scaledWindows.contains(w)) {
        auto &state = m_scaledWindows.at(w).state;
        state.isEnabled = enabled;
        if (enabled) {
            state.originalPosition = w->pos();
        } else {
            w->window()->moveResize(RectF{
                state.originalPosition.x(),
                state.originalPosition.y(),
                w->width(),
                w->height()
            });
        }
        
    } else {
        m_scaledWindows.insert({w, ScaleData{.state{.isEnabled{true},
                                                    .originalPosition{w->pos()}}}});
    }

    if (hasEnabledScalers()) {
        input()->installInputEventFilter(this);
    } else {
        input()->uninstallInputEventFilter(this);
    }
}

QPointF ScaleToScreenEffect::mapWindowToCursor(EffectWindow *w, QPointF cursorPosition) const
{
    if (!m_scaledWindows.contains(w)) {
        return w->pos();
    }

    const auto &data = m_scaledWindows.at(w);
    const QRectF target = data.state.targetRect;
    const QMarginsF margins = data.settings.margins;

    // Calculate window's visual (cropped) dimensions
    qreal visualWidth = w->width() - margins.left() - margins.right();
    qreal visualHeight = w->height() - margins.top() - margins.bottom();

    // Normalize cursor position relative to targetRect (0.0 to 1.0)
    // We clamp to ensure the window doesn't fly off if the cursor leaves the rect
    qreal relX = (cursorPosition.x() - target.left()) / target.width();
    qreal relY = (cursorPosition.y() - target.top()) / target.height();
    relX = std::clamp(relX, 0.0, 1.0);
    relY = std::clamp(relY, 0.0, 1.0);

    // Calculate movement range
    // How much space is left in the targetRect after placing the window?
    qreal rangeX = target.width() - visualWidth;
    qreal rangeY = target.height() - visualHeight;

    // Calculate Top-Left of the VISUAL area
    qreal visualX = target.left() + (relX * rangeX);
    qreal visualY = target.top() + (relY * rangeY);

    // Offset by margins to get the ACTUAL window position
    // If we have a left margin of 10px, the window must be 10px further left
    // than the visual start point.
    return QPointF(visualX - margins.left(), visualY - margins.top());
}

void ScaleToScreenEffect::syncWindowToCursor(QPointF cursorPosition) const
{
    EffectWindow *w = findWindowAtCursor(cursorPosition);
    if (!w || !m_scaledWindows.at(w).state.isEnabled) {
        return;
    }

    QPointF newPosition = mapWindowToCursor(w, cursorPosition);
    w->window()->moveResize(RectF{
        newPosition.x(),
        newPosition.y(),
        w->width(),
        w->height()
    });
}

EffectWindow *ScaleToScreenEffect::findWindowAtCursor(QPointF cursorPosition) const
{
    for (auto &[win, scaleData] : m_scaledWindows) {
        if (scaleData.state.targetRect.contains(cursorPosition)) {
            return win;
        }
    }

    return nullptr;
}

bool ScaleToScreenEffect::hasEnabledScalers() const
{
    for (auto &[win, scaleData] : m_scaledWindows) {
        if (scaleData.state.isEnabled) {
            return true;
        }
    }
    return false;
}

bool ScaleToScreenEffect::pointerMotion(PointerMotionEvent *event)
{
    syncWindowToCursor(event->position);
    return false;
}

bool ScaleToScreenEffect::pointerButton(PointerButtonEvent *event)
{
    syncWindowToCursor(event->position);
    return false;
}

bool ScaleToScreenEffect::pointerAxis(KWin::PointerAxisEvent *event)
{
    syncWindowToCursor(event->position);
    return false;
}

void ScaleToScreenEffect::toggleActiveWindow()
{
    EffectWindow *active = effects->activeWindow();
    if (!active) {
        return;
    }

    if (!m_scaledWindows.contains(active)) {
        setEnabled(active, true);
        return;
    }

    setEnabled(active, !m_scaledWindows.at(active).state.isEnabled);
}

void ScaleToScreenEffect::onWindowAdded(EffectWindow *w)
{
    // TODO: Check against config

    if (w->window()->caption() == "Event Tester") {
        qDebug() << "Enable for Event Tester";
        setEnabled(w, true);
    }
}

void ScaleToScreenEffect::onWindowDeleted(EffectWindow *w)
{
    setEnabled(w, false);
    m_scaledWindows.erase(w);
}

} // namespace KWin
