#include "scale_to_screen_effect.h"
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>
#include <input_event.h>
#include <window.h>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <QAction>


namespace KWin {

Q_LOGGING_CATEGORY(lcScaleToScreen, "scaleToScreen")

ScaleToScreenEffect::ScaleToScreenEffect()
    : Effect()
    , InputEventFilter(InputFilterOrder::Effects)
{

    QAction *a = new QAction(this);
    a->setObjectName(QStringLiteral("scaleToScreen")); 
    a->setText(i18n("Scale the active window to fullscreen"));
    KGlobalAccel::self()->setShortcut(a, {Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_A});

    connect(a, &QAction::triggered, this, &ScaleToScreenEffect::toggleActiveWindow);
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

    if (enabled) {
        updateTargetRect(w);
        syncWindowToCursor(input()->globalPointer());
    }

    if (hasEnabledScalers()) {
        qCDebug(lcScaleToScreen) << "installInputEventFilter";
        input()->installInputEventFilter(this);
    } else {
        qCDebug(lcScaleToScreen) << "uninstallInputEventFilter";
        input()->uninstallInputEventFilter(this);
    }
}

QRectF ScaleToScreenEffect::calculateTargetRect(QRectF screenGeometry, QRectF windowGeometry,
                                               QMargins windowMargins, Qt::AspectRatioMode aspectRatio) const
{
    // Calculate the dimensions of the window's visual area by subtracting 
    // decorative margins from the total window geometry
    const qreal visualWidth = windowGeometry.width() - windowMargins.left() - windowMargins.right();
    const qreal visualHeight = windowGeometry.height() - windowMargins.top() - windowMargins.bottom();
    const QSizeF visualSize(visualWidth, visualHeight);

    // Return the full screen geometry as a fallback if the window has no valid visual size.
    if (visualSize.isEmpty()) {
        return screenGeometry;
    }

    // Determine the size of the target rectangle based on the specified aspect ratio mode.
    // If IgnoreAspectRatio is used, the target rect matches the screen size exactly.
    QSizeF targetSize = screenGeometry.size();
    
    if (aspectRatio != Qt::IgnoreAspectRatio) {
        // Scale the visual dimensions to fit within or fill the screen bounds 
        // while maintaining the window's aspect ratio.
        targetSize = visualSize.scaled(screenGeometry.size(), aspectRatio);
    }

    // Create the final target rectangle and center it within the screen geometry 
    // to provide a balanced interaction area for panning.
    QRectF targetRect(QPointF(0, 0), targetSize);
    targetRect.moveCenter(screenGeometry.center());

    return targetRect;
}

void ScaleToScreenEffect::updateTargetRect(EffectWindow *w)
{
    if (!w || !m_scaledWindows.contains(w)) {
        return;
    }

    ScaleData &data = m_scaledWindows.at(w);

    const QRectF screenGeometry = effects->clientArea(FullScreenArea, w);
    const QRectF windowGeometry = w->frameGeometry();
    data.state.targetRect = calculateTargetRect(screenGeometry, windowGeometry, data.settings.margins, data.settings.aspectRatio);
    qCDebug(lcScaleToScreen) << "new targetRect" << data.state.targetRect;
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

bool ScaleToScreenEffect::syncWindowToCursor(QPointF cursorPosition) const
{
    EffectWindow *w = findWindowAtCursor(cursorPosition);
    if (!w || !m_scaledWindows.at(w).state.isEnabled) {
        return false;
    }

    QPointF newPosition = mapWindowToCursor(w, cursorPosition);
    w->window()->moveResize(RectF{
        newPosition.x(),
        newPosition.y(),
        w->width(),
        w->height()
    });

    return true;
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

bool ScaleToScreenEffect::shouldBlockInput(QPointF pos) const
{
    EffectWindow *w = findWindowAtCursor(pos);

    // If the window isn't being managed by our effect or isn't enabled,
    // we return false so the event passes through to the system normally.
    if (!w || !m_scaledWindows.contains(w) || !m_scaledWindows.at(w).state.isEnabled) {
        return false;
    }

    // If the window IS enabled by us, we only allow (return false) 
    // if the cursor is within the actual frame.
    bool isInside = w->frameGeometry().contains(pos.toPoint());
    
    return !isInside; 
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
    if (!hasEnabledScalers()) {
        return false;
    }
    bool blockEvent = shouldBlockInput(event->position);
    syncWindowToCursor(event->position);
    return blockEvent;
}

bool ScaleToScreenEffect::pointerButton(PointerButtonEvent *event)
{
    if (!hasEnabledScalers()) {
        return false;
    }
    bool blockEvent = shouldBlockInput(event->position);
    syncWindowToCursor(event->position);
    return blockEvent;
}

bool ScaleToScreenEffect::pointerAxis(KWin::PointerAxisEvent *event)
{
    if (!hasEnabledScalers()) {
        return false;
    }
    bool blockEvent = shouldBlockInput(event->position);
    syncWindowToCursor(event->position);
    return blockEvent;
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
{}

void ScaleToScreenEffect::onWindowDeleted(EffectWindow *w)
{
    setEnabled(w, false);
    m_scaledWindows.erase(w);
}

} // namespace KWin
