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

    connect(a, &QAction::triggered, this, &ScaleToScreenEffect::toggleEffect);
    connect(effects, &EffectsHandler::windowAdded, this, &ScaleToScreenEffect::onWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted, this, &ScaleToScreenEffect::onWindowDeleted);
    connect(effects, &EffectsHandler::windowActivated, this, &ScaleToScreenEffect::onWindowActivated);
}

ScaleToScreenEffect::~ScaleToScreenEffect()
{
}


void ScaleToScreenEffect::updateScalingState()
{
    const bool scale = shouldScale();
    if (m_state.isScaling == scale) {
        return;
    }

    m_state.isScaling = scale;

    if (scale) {
        startScaling();
    } else {
        stopScaling();
    }
}

void ScaleToScreenEffect::startScaling()
{
    m_state.originalPosition = m_state.window->pos();
    m_state.originalNoBorder = m_state.window->window()->noBorder();
    m_state.originalKeepAbove = m_state.window->window()->keepAbove();
    m_state.window->window()->setNoBorder(m_settings.noBorder);
    m_state.window->window()->setKeepAbove(true);
    m_state.isScaling = true;
    m_state.window = m_state.window;
    updateTargetRect();
    syncWindowToCursor(input()->globalPointer());

    qCDebug(lcScaleToScreen) << "installInputEventFilter";
    input()->installInputEventFilter(this);
}

void ScaleToScreenEffect::stopScaling()
{
    qCDebug(lcScaleToScreen) << "uninstallInputEventFilter";
    input()->uninstallInputEventFilter(this);

    m_state.isScaling = false;
    m_state.window->window()->setKeepAbove(m_state.originalKeepAbove);
    m_state.window->window()->setNoBorder(m_state.originalNoBorder);
    m_state.window->window()->moveResize(RectF{m_state.originalPosition.x(), m_state.originalPosition.y(),
                                            m_state.window->width(),m_state.window->height()});
}

bool ScaleToScreenEffect::shouldScale() const
{
    if (m_state.window) {
        const EffectWindow *active = effects->activeWindow();
        if (active == m_state.window) {
            return true;
        }
    }
    return false;
}

void ScaleToScreenEffect::scaleActiveWindow()
{
    EffectWindow *active = effects->activeWindow();
    if (!active) {
        return;
    }

    if (m_state.window != active) {
        if (m_state.isScaling) {
            stopScaling();
        }
        m_state.window = active;
        startScaling();
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

void ScaleToScreenEffect::updateTargetRect()
{
    if (!m_state.window) {
        return;
    }

    const QRectF screenGeometry = effects->clientArea(FullScreenArea, m_state.window);
    const QRectF windowGeometry = m_state.window->frameGeometry();
    m_state.targetRect = calculateTargetRect(screenGeometry, windowGeometry, m_settings.margins, m_settings.aspectRatio);
    qCDebug(lcScaleToScreen) << "new targetRect" << m_state.targetRect;
}

QPointF ScaleToScreenEffect::mapWindowToCursor(QPointF cursorPosition) const
{
    const QRectF target = m_state.targetRect;
    const QMarginsF margins = m_settings.margins;

    // Calculate window's visual (cropped) dimensions
    qreal visualWidth = m_state.window->width() - margins.left() - margins.right();
    qreal visualHeight = m_state.window->height() - margins.top() - margins.bottom();

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
    QPointF newPosition = mapWindowToCursor(cursorPosition);
    m_state.window->window()->moveResize(RectF{
        newPosition.x(),
        newPosition.y(),
        m_state.window->width(),
        m_state.window->height()
    });

    return true;
}

bool ScaleToScreenEffect::shouldBlockInput(QPointF pos) const
{
    const bool cursorIsOutsideTargetRect = !m_state.targetRect.contains(pos);
    const bool windowContainsCursor = m_state.window->frameGeometry().contains(pos.toPoint());
    
    return cursorIsOutsideTargetRect || !windowContainsCursor; 
}

void ScaleToScreenEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (!m_state.isScaling) {
        effects->prePaintScreen(data, presentTime);
        return;
    }

    effects->prePaintScreen(data, presentTime);
}

void ScaleToScreenEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport,
                                      int mask, const Region &region, LogicalOutput *screen)
{
    if (!m_state.isScaling) {
        effects->paintScreen(renderTarget, viewport, mask, region, screen);
        return;
    }

    effects->paintScreen(renderTarget, viewport, mask, region, screen);
}

void ScaleToScreenEffect::postPaintScreen()
{
    if (!m_state.isScaling) {
        effects->postPaintScreen();
        return;
    }

    effects->postPaintScreen();
}

bool ScaleToScreenEffect::pointerMotion(PointerMotionEvent *event)
{
    bool blockEvent = shouldBlockInput(event->position);
    syncWindowToCursor(event->position);
    return blockEvent;
}

bool ScaleToScreenEffect::pointerButton(PointerButtonEvent *event)
{
    bool blockEvent = shouldBlockInput(event->position);
    syncWindowToCursor(event->position);
    return blockEvent;
}

bool ScaleToScreenEffect::pointerAxis(KWin::PointerAxisEvent *event)
{
    bool blockEvent = shouldBlockInput(event->position);
    syncWindowToCursor(event->position);
    return blockEvent;
}

void ScaleToScreenEffect::toggleEffect()
{
    if (m_state.isScaling) {
        stopScaling();
        m_state.window = nullptr;
    } else {
        scaleActiveWindow();
    }
}

void ScaleToScreenEffect::onWindowAdded(EffectWindow *)
{}

void ScaleToScreenEffect::onWindowDeleted(EffectWindow *w)
{
    if (w && m_state.window == w) {
        if (m_state.isScaling) {
            stopScaling();
            m_state.window = nullptr;
        }
    }
}

void ScaleToScreenEffect::onWindowActivated(KWin::EffectWindow *w)
{
    qCDebug(lcScaleToScreen) << "onWindowActivated" << w;
    if (m_state.window) {
        if (m_state.isScaling) {
            stopScaling();
            m_state.window = nullptr;
        }
    }
}

} // namespace KWin
