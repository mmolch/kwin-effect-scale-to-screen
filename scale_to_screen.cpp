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
#include <QDirIterator>

static void ensureResources()
{
    // Must initialize resources manually because the effect is a static lib.
    Q_INIT_RESOURCE(scale_to_screen);
}

namespace KWin {

Q_LOGGING_CATEGORY(lcScaleToScreen, "scaleToScreen")

ScaleToScreenEffect::ScaleToScreenEffect()
    : Effect()
    , InputEventFilter(InputFilterOrder::Effects)
{
    ensureResources();
    QDirIterator it(":", QDirIterator::Subdirectories);
    while (it.hasNext()) {
        qDebug() << it.next();
    }

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
    updateTargetRect();
    syncWindowToCursor(input()->globalPointer());

    qCDebug(lcScaleToScreen) << "installInputEventFilter";
    input()->installInputEventFilter(this);
    effects->addRepaintFull();
}

void ScaleToScreenEffect::stopScaling()
{
    qCDebug(lcScaleToScreen) << "uninstallInputEventFilter";
    input()->uninstallInputEventFilter(this);

    m_state.isScaling = false;
    freeBuffer();
    m_state.window->window()->setKeepAbove(m_state.originalKeepAbove);
    m_state.window->window()->setNoBorder(m_state.originalNoBorder);
    m_state.window->window()->moveResize(RectF{m_state.originalPosition.x(), m_state.originalPosition.y(),
                                            m_state.window->width(),m_state.window->height()});
    effects->addRepaintFull();
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
    // 1-pixel-off jitter correction test 1: Didn't work
    // Idea: snap to whole pixels at 1.25 (or other) scale display
    const qreal scale = 1.0;

    QPointF newPosition = mapWindowToCursor(cursorPosition);
    m_state.window->window()->moveResize(RectF{
        std::round(newPosition.x() * scale) / scale,
        std::round(newPosition.y() * scale) / scale,
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
    if (m_state.isScaling) {
        data.mask |= PAINT_WINDOW_OPAQUE;
        data.mask |= ~PAINT_SCREEN_BACKGROUND_FIRST;
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

    const auto scale = viewport.scale();
    const QRectF winGeo = m_state.window->frameGeometry();
    const QMarginsF margins = m_settings.margins;
    
    const QRectF visualRect = winGeo.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
    const QSize pixelSize = (visualRect.size() * scale).toSize();

    if (!m_buffer) {
        createBuffer(pixelSize, *renderTarget.colorDescription());
    }

    RenderTarget offscreenTarget(m_buffer->framebuffer.get(), renderTarget.colorDescription());
    RenderViewport offscreenViewport(visualRect, 1.0, offscreenTarget, QPoint(0,0));

    GLFramebuffer::pushFramebuffer(m_buffer->framebuffer.get());
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    effects->paintScreen(offscreenTarget, offscreenViewport, mask, region, screen);
    
    GLFramebuffer::popFramebuffer();

    //saveBufferToDisk();

    //Clear the screen to black (pillarboxing)
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    GLShader *shader = getShader(m_settings.shader);
    ShaderManager::instance()->pushShader(shader);

    const QRectF targetGeo = m_state.targetRect;
    const float scaleX = targetGeo.width() / visualRect.width();
    const float scaleY = targetGeo.height() / visualRect.height();

    QMatrix4x4 matrix;
    matrix.translate(targetGeo.x() * scale, targetGeo.y() * scale);
    matrix.scale(scaleX, scaleY);

    shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, viewport.projectionMatrix() * matrix);
    shader->setUniform(GLShader::IntUniform::TextureWidth, m_buffer->texture->width());
    shader->setUniform(GLShader::IntUniform::TextureHeight, m_buffer->texture->height());
    shader->setColorspaceUniforms(ColorDescription::sRGB, renderTarget.colorDescription(), RenderingIntent::Perceptual);

    m_buffer->texture->render(visualRect.size() * scale);

    ShaderManager::instance()->popShader();
}

void ScaleToScreenEffect::postPaintScreen()
{
    if (!m_state.isScaling) {
        effects->postPaintScreen();
        return;
    }

    effects->postPaintScreen();
}

void ScaleToScreenEffect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &region, WindowPaintData &data)
{
    if (!m_state.isScaling) {
        // Passthrough if we're not scaling
        effects->paintWindow(renderTarget, viewport, w, mask, region, data);
        return;
    }

    // Paint only our single window when we're in scaled mode
    if (m_state.window == w) {
        effects->renderWindow(renderTarget, viewport, w, mask, region, data);
    }
}

bool ScaleToScreenEffect::pointerMotion(PointerMotionEvent *event)
{
    const QPointF &pos = event->position;
    constrainPointer(pos);
    bool blockEvent = shouldBlockInput(pos);
    syncWindowToCursor(pos);
    return blockEvent;
}

bool ScaleToScreenEffect::pointerButton(PointerButtonEvent *event)
{
    const QPointF &pos = event->position;
    constrainPointer(pos);
    bool blockEvent = shouldBlockInput(pos);
    syncWindowToCursor(pos);
    return blockEvent;
}

bool ScaleToScreenEffect::pointerAxis(KWin::PointerAxisEvent *event)
{
    const QPointF &pos = event->position;
    constrainPointer(pos);
    bool blockEvent = shouldBlockInput(pos);
    syncWindowToCursor(pos);
    return blockEvent;
}

void ScaleToScreenEffect::constrainPointer(QPointF pos)
{
    QRectF constraintRect = m_state.window->frameGeometry();
    if (!constraintRect.contains(pos)) {
        pos.setX(qMax(constraintRect.left(), qMin(pos.x(), constraintRect.right())));
        pos.setY(qMax(constraintRect.top(), qMin(pos.y(), constraintRect.bottom())));
        input()->warpPointer(pos);
    }
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

GLShader *ScaleToScreenEffect::getShader(Shader shader)
{
    if (shader == Shader::Builtin) {
        return ShaderManager::instance()->shader(ShaderTrait::MapTexture | ShaderTrait::TransformColorspace);
    }

    if (!m_state.shaders.contains(shader)) {
        switch (shader)
        {
        default:
            return getShader(Shader::Builtin);
        }
        
    }
    return m_state.shaders.at(shader).get();
}

void ScaleToScreenEffect::createBuffer(const QSize &size, const ColorDescription &color)
{
    effects->makeOpenGLContextCurrent();
    m_buffer = std::make_unique<OffscreenBuffer>();
    
    const GLenum format = (color == *ColorDescription::sRGB) ? GL_RGBA8 : GL_RGBA16F;
    m_buffer->texture = GLTexture::allocate(format, size);
    m_buffer->texture->setFilter(GL_LINEAR);
    m_buffer->texture->setWrapMode(GL_CLAMP_TO_EDGE);
    m_buffer->framebuffer = std::make_unique<GLFramebuffer>(m_buffer->texture.get());
}

void ScaleToScreenEffect::freeBuffer()
{
    if (m_buffer) {
        effects->makeOpenGLContextCurrent();
        m_buffer.reset();
    }
}

void ScaleToScreenEffect::saveBufferToDisk()
{
    if (!m_buffer || !m_buffer->texture) {
        return;
    }

    const QSize size = m_buffer->texture->size();
    
    // 1. Prepare a QImage to hold the data
    // Use ARGB32_Premultiplied for standard GL_RGBA8 textures
    QImage img(size, QImage::Format_ARGB32_Premultiplied);

    // 2. Bind the framebuffer to read from it
    GLFramebuffer::pushFramebuffer(m_buffer->framebuffer.get());

    // 3. Read pixels from the GPU to the QImage's internal buffer
    glReadPixels(0, 0, size.width(), size.height(), GL_BGRA, GL_UNSIGNED_BYTE, img.bits());

    GLFramebuffer::popFramebuffer();

    // 4. OpenGL coordinates are bottom-to-top, QImage is top-to-bottom
    img = img.mirrored(false, true);

    // 5. Save to a unique path
    QString path = QStringLiteral("/tmp/kwin_dump_%1.png")
                   .arg(QDateTime::currentMSecsSinceEpoch());
    
    if (img.save(path, "PNG")) {
        qCDebug(lcScaleToScreen) << "Saved texture to:" << path;
    } else {
        qCWarning(lcScaleToScreen) << "Failed to save texture to:" << path;
    }
}

} // namespace KWin
