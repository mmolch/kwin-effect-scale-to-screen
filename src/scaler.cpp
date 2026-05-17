#include "scale_to_screen.h"
#include "scaler.h"
#include "effect/effecthandler.h"
#include "effect/effectwindow.h"
#include "scene/itemrenderer_opengl.h"
#include "scene/windowitem.h"
#include "scene/workspacescene.h"
#include "compositor.h"

namespace scaleToScreen {

Scaler::Scaler(ScaleToScreen &effect, EffectWindow &window, ScalerSettings settings)
    : QObject()
    , m_effect(effect)
    , m_window(window)
    , m_settings(std::move(settings))
{
    qCDebug(lcScaleToScreen) << "Scaler()";

    /* KWin crashes if the user drags a window with the titlebar and noBorder gets set.
     * Try to make sure that the border will stay hidden while the scaler is set on it. */
    m_state.originalNoBorder = window.window()->noBorder();
    window.window()->setNoBorder(true);
    connect(window.window(), &Window::decorationPolicyChanged, this, &Scaler::onDecorationPolicyChanged);
}

Scaler::~Scaler()
{
    qCDebug(lcScaleToScreen) << "~Scaler()";

    if (!m_state.windowDeleted) {
        disconnect(m_window.window(), &Window::decorationPolicyChanged, this, &Scaler::onDecorationPolicyChanged);
        m_window.window()->setNoBorder(m_state.originalNoBorder);
    }
}

bool Scaler::isScaling() const
{
    return m_state.isScaling;
}

void Scaler::setScaling(bool scaling)
{
    qCDebug(lcScaleToScreen) << "setScaling()" << scaling;

    if (m_state.isScaling == scaling) {
        return;
    }

    m_state.isScaling = scaling;

    if (scaling) {
        m_state.originalPosition = m_window.pos();
        m_state.originalKeepAbove = m_window.window()->keepAbove();
        m_window.window()->setKeepAbove(true);
        m_state.windowSize = m_window.frameGeometry().size();

        connect(m_window.window(), &Window::keepAboveChanged, this, &Scaler::onKeepAboveChanged);
        connect(m_window.window(), &Window::frameGeometryChanged, this, &Scaler::onFrameGeometryChanged);
        connect(m_window.window(), &Window::fullScreenChanged, this, &Scaler::onFullScreenChanged);

        updateTargetRect();
        updatePaintData();
        syncWindowToCursor(input()->globalPointer());

    } else {
        disconnect(m_window.window(), &Window::keepAboveChanged, this, &Scaler::onKeepAboveChanged);
        disconnect(m_window.window(), &Window::frameGeometryChanged, this, &Scaler::onFrameGeometryChanged);
        disconnect(m_window.window(), &Window::fullScreenChanged, this, &Scaler::onFullScreenChanged);

        m_window.window()->setKeepAbove(m_state.originalKeepAbove);
        m_window.window()->moveResize(RectF{m_state.originalPosition.x(), m_state.originalPosition.y(),
                                            m_window.width(),m_window.height()});
    }

    effects->addRepaintFull();

    emit isScalingChanged(scaling);
}

void Scaler::onDecorationPolicyChanged()
{
    m_window.window()->setNoBorder(true);
}

void Scaler::onKeepAboveChanged()
{
    m_window.window()->setKeepAbove(true);
}

void Scaler::onFrameGeometryChanged(RectF geometry)
{
    if (m_state.windowSize == geometry.size()) {
        return;
    }

    m_state.windowSize = geometry.size();

    updateTargetRect();
    updatePaintData();
    syncWindowToCursor(input()->globalPointer());
}

void Scaler::onFullScreenChanged()
{
    if (m_window.window()->isFullScreen()) {
        // Enter real fullscreen mode
        setScaling(false);

        auto it = m_effect.m_suspendedScalers.find(&m_window);
        auto scalerPtr = std::move(it->second);

        m_window.window()->moveResize(RectF{
            0,
            0,
            m_window.width(),
            m_window.height()
        });
    }
}

QRectF Scaler::calculateTargetRect(QRectF screenGeometry, QRectF windowGeometry,
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

void Scaler::updateTargetRect()
{
    const QRectF screenGeometry = effects->clientArea(FullScreenArea, &m_window);
    const QRectF windowGeometry = m_window.frameGeometry();
    m_state.targetRect = calculateTargetRect(screenGeometry, windowGeometry, m_settings.margins, m_settings.aspectRatio);
    qCDebug(lcScaleToScreen) << "new targetRect" << m_state.targetRect;
}

void Scaler::updatePaintData()
{
    const QRectF winGeo = m_window.frameGeometry();
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

QPointF Scaler::mapWindowToCursor(QPointF cursorPosition) const
{
    const QRectF target = m_state.targetRect;
    const QMarginsF margins = m_settings.margins;

    // Calculate window's visual (cropped) dimensions
    qreal visualWidth = m_window.width() - margins.left() - margins.right();
    qreal visualHeight = m_window.height() - margins.top() - margins.bottom();

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

void Scaler::syncWindowToCursor(QPointF cursorPosition) const
{
    QPointF newPosition = mapWindowToCursor(cursorPosition);
    m_window.window()->moveResize(RectF{
        newPosition.x(),
        newPosition.y(),
        m_window.width(),
        m_window.height()
    });
}

void Scaler::renderScaledWindowItem(const RenderTarget &target, const RenderViewport &viewport, const Region &region)
{
    auto renderer = static_cast<KWin::ItemRendererOpenGL *>(KWin::Compositor::self()->scene()->renderer());
    Item *item = m_window.window()->windowItem()->windowContainer();

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


} // namespace scaleToScreen
