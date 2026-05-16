#include "scale_to_screen.h"
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>
#include <input_event.h>
#include <window.h>
#include <KGlobalAccel>
#include <KLocalizedString>
#include <QAction>
#include "opengl/glutils.h"
#include "opengl/glframebuffer.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "core/graphicsbuffer.h"
#include "scene/surfaceitem.h"
#include "scene/windowitem.h"
#include "scene/itemrenderer_opengl.h"
#include "compositor.h"
#include "scene/workspacescene.h"

namespace KWin {

Q_LOGGING_CATEGORY(lcScaleToScreen, "scaleToScreen")

ScaleToScreen::ScaleToScreen()
    : Effect()
    , InputEventFilter(InputFilterOrder::Effects)
{
    QLoggingCategory::setFilterRules(QStringLiteral("scaleToScreen.debug=false"));

    QAction *a = new QAction(this);
    a->setObjectName(QStringLiteral("scaleToScreen")); 
    a->setText(i18n("Scale the active window to fullscreen"));
    KGlobalAccel::self()->setShortcut(a, {Qt::CTRL | Qt::ALT | Qt::SHIFT | Qt::Key_A});

    connect(a, &QAction::triggered, this, &ScaleToScreen::toggleEffect);
    connect(effects, &EffectsHandler::windowAdded, this, &ScaleToScreen::onWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted, this, &ScaleToScreen::onWindowDeleted);
    connect(effects, &EffectsHandler::windowActivated, this, &ScaleToScreen::onWindowActivated);
}

ScaleToScreen::~ScaleToScreen()
{
}

void ScaleToScreen::updateScalingState()
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

void ScaleToScreen::startScaling()
{
    qCDebug(lcScaleToScreen) << "startScaling()";
    m_state.originalPosition = m_state.window->pos();
    m_state.originalNoBorder = m_state.window->window()->noBorder();
    m_state.originalKeepAbove = m_state.window->window()->keepAbove();
    m_state.window->window()->setNoBorder(m_settings.noBorder);
    m_state.window->window()->setKeepAbove(true);

    m_state.windowSize = m_state.window->frameGeometry().size();
    connect(m_state.window->window(), &Window::keepAboveChanged, this, &ScaleToScreen::onKeepAboveChanged);
    connect(m_state.window->window(), &Window::frameGeometryChanged, this, &ScaleToScreen::onFrameGeometryChanged);
    connect(m_state.window->window(), &Window::fullScreenChanged, this, &ScaleToScreen::onFullScreenChanged);

    updateTargetRect();
    updatePaintData();
    syncWindowToCursor(input()->globalPointer());

    qCDebug(lcScaleToScreen) << "installInputEventFilter";
    input()->installInputEventFilter(this);

    effects->addRepaintFull();

    m_state.isScaling = true;
}

void ScaleToScreen::stopScaling()
{
    qCDebug(lcScaleToScreen) << "stopScaling()";

    m_state.isScaling = false;

    qCDebug(lcScaleToScreen) << "uninstallInputEventFilter";
    input()->uninstallInputEventFilter(this);

    disconnect(m_state.window->window(), &Window::keepAboveChanged, this, &ScaleToScreen::onKeepAboveChanged);
    disconnect(m_state.window->window(), &Window::frameGeometryChanged, this, &ScaleToScreen::onFrameGeometryChanged);
    disconnect(m_state.window->window(), &Window::fullScreenChanged, this, &ScaleToScreen::onFullScreenChanged);

    m_state.window->window()->setKeepAbove(m_state.originalKeepAbove);
    m_state.window->window()->setNoBorder(m_state.originalNoBorder);
    m_state.window->window()->moveResize(RectF{m_state.originalPosition.x(), m_state.originalPosition.y(),
                                            m_state.window->width(),m_state.window->height()});
    effects->addRepaintFull();
}

bool ScaleToScreen::shouldScale() const
{
    if (m_state.window) {
        const EffectWindow *active = effects->activeWindow();
        if (active == m_state.window) {
            return true;
        }
    }
    return false;
}

void ScaleToScreen::scaleActiveWindow()
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

QRectF ScaleToScreen::calculateTargetRect(QRectF screenGeometry, QRectF windowGeometry,
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

void ScaleToScreen::updateTargetRect()
{
    const QRectF screenGeometry = effects->clientArea(FullScreenArea, m_state.window);
    const QRectF windowGeometry = m_state.window->frameGeometry();
    m_state.targetRect = calculateTargetRect(screenGeometry, windowGeometry, m_settings.margins, m_settings.aspectRatio);
    qCDebug(lcScaleToScreen) << "new targetRect" << m_state.targetRect;
}

void ScaleToScreen::updatePaintData()
{
    const QRectF winGeo = m_state.window->frameGeometry();
    const QMarginsF margins = m_settings.margins;

    const QRectF visualRect = winGeo.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
    const QRectF targetGeo = m_state.targetRect;

    const float scaleX = targetGeo.width() / visualRect.width();
    const float scaleY = targetGeo.height() / visualRect.height();

    m_state.paintData.setXTranslation(targetGeo.x());
    m_state.paintData.setYTranslation(targetGeo.y());
    m_state.paintData.setXScale(scaleX);
    m_state.paintData.setYScale(scaleY);
    
    m_state.paintData.setOpacity(1.0);
}

QPointF ScaleToScreen::mapWindowToCursor(QPointF cursorPosition) const
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

void ScaleToScreen::syncWindowToCursor(QPointF cursorPosition) const
{
    QPointF newPosition = mapWindowToCursor(cursorPosition);
    m_state.window->window()->moveResize(RectF{
        newPosition.x(),
        newPosition.y(),
        m_state.window->width(),
        m_state.window->height()
    });
}

bool ScaleToScreen::shouldBlockInput(QPointF pos) const
{
    // Block if the cursor is outside the target window
    const bool cursorIsOutsideTargetRect = !m_state.targetRect.contains(pos);

    // Block if the cursor isn't inside the actual window
    const bool cursorIsNotInWindow = !m_state.window->frameGeometry().contains(pos.toPoint());
    
    return cursorIsOutsideTargetRect || cursorIsNotInWindow; 
}

void ScaleToScreen::renderScaledWindowItem(const KWin::RenderTarget &target, 
                                           const KWin::RenderViewport &viewport,
                                           const Region &region) 
{
    auto renderer = static_cast<KWin::ItemRendererOpenGL *>(KWin::Compositor::self()->scene()->renderer());
    Item *item = m_state.window->window()->windowItem()->windowContainer();

    renderer->renderItem(target, 
                         viewport, 
                         item, 
                         0, // mask
                         region, // The area to paint
                         m_state.paintData, 
                         nullptr, // No filter (render all children)
                         nullptr  // No hole filter
    );
}

void ScaleToScreen::clearScreen()
{
    //Clear the screen to black (pillarboxing)
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
}

void ScaleToScreen::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (m_state.isScaling) {
        data.mask |= PAINT_WINDOW_OPAQUE;
        data.mask |= ~PAINT_SCREEN_BACKGROUND_FIRST;
    }
    effects->prePaintScreen(data, presentTime);
}

void ScaleToScreen::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &region, WindowPaintData &data)
{
    if (!m_state.isScaling) {
        effects->paintWindow(renderTarget, viewport, w, mask, region, data);
        return;
    }

    if (m_state.window == w) {
        clearScreen();
        renderScaledWindowItem(renderTarget, viewport, region);
    } else {
        effects->paintWindow(renderTarget, viewport, w, mask, region, data);
    }
}

bool ScaleToScreen::pointerMotion(PointerMotionEvent *event)
{
    const QPointF &pos = event->position;
    constrainPointer(pos);
    bool blockEvent = shouldBlockInput(pos);
    syncWindowToCursor(pos);
    return blockEvent;
}

bool ScaleToScreen::pointerButton(PointerButtonEvent *event)
{
    const QPointF &pos = event->position;
    constrainPointer(pos);
    bool blockEvent = shouldBlockInput(pos);
    syncWindowToCursor(pos);
    return blockEvent;
}

bool ScaleToScreen::pointerAxis(KWin::PointerAxisEvent *event)
{
    const QPointF &pos = event->position;
    constrainPointer(pos);
    bool blockEvent = shouldBlockInput(pos);
    syncWindowToCursor(pos);
    return blockEvent;
}

void ScaleToScreen::constrainPointer(QPointF pos)
{
    QRectF constraintRect = m_state.window->frameGeometry();
    if (!constraintRect.contains(pos)) {
        pos.setX(std::clamp(pos.x(), constraintRect.left(), constraintRect.right()-1));
        pos.setY(std::clamp(pos.y(), constraintRect.top(), constraintRect.bottom()-1));
        input()->warpPointer(pos);
    }
}

void ScaleToScreen::toggleEffect()
{
    if (m_state.isScaling) {
        stopScaling();
        m_state.window = nullptr;
    } else {
        scaleActiveWindow();
    }
}

void ScaleToScreen::onWindowAdded(EffectWindow *)
{}

void ScaleToScreen::onWindowDeleted(EffectWindow *w)
{
    if (w && m_state.window == w) {
        if (m_state.isScaling) {
            stopScaling();
        }
        m_state.window = nullptr;
    }
}

void ScaleToScreen::onWindowActivated(KWin::EffectWindow *w)
{
    qCDebug(lcScaleToScreen) << "onWindowActivated" << w;

    if (w && w == m_state.window) {
        startScaling();
    } else {
        if (m_state.isScaling) {
            stopScaling();
        }
    }
}

void ScaleToScreen::onKeepAboveChanged(bool keepAbove)
{
    if (!keepAbove) {
        if (m_state.window) {
            m_state.window->window()->setKeepAbove(true);
        }
    }
}

void ScaleToScreen::onFrameGeometryChanged(RectF geometry)
{
    if (m_state.windowSize == geometry.size()) {
        return;
    }

    m_state.windowSize = geometry.size();

    updateTargetRect();
    updatePaintData();
    syncWindowToCursor(input()->globalPointer());
}

void ScaleToScreen::onFullScreenChanged()
{
    if (m_state.window->window()->isFullScreen()) {
        // Enter real fullscreen mode
        stopScaling();

        m_state.window->window()->moveResize(RectF{
            0,
            0,
            m_state.window->width(),
            m_state.window->height()
        });

        m_state.window = nullptr;
    }
}

} // namespace KWin
