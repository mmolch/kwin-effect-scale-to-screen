#pragma once

#include "effect/effect.h"
#include "input.h"
#include <QLoggingCategory>
#include "opengl/glutils.h"

namespace KWin {

Q_DECLARE_LOGGING_CATEGORY(lcScaleToScreen)

class ScaleToScreenEffect : public Effect, public InputEventFilter
{
    Q_OBJECT
public:
    explicit ScaleToScreenEffect();
    ~ScaleToScreenEffect() override;

private:
    void updateScalingState();
    void startScaling();
    void stopScaling();
    bool shouldScale() const;
    void scaleActiveWindow();

    QRectF calculateTargetRect(QRectF screenGeometry, QRectF windowGeometry,
                               QMargins windowMargins, Qt::AspectRatioMode aspectRatio) const;
    void updateTargetRect();

    // Maps the window position to the targetRect so that
    // the window overlaps at the cursor position in the targetRect
    QPointF mapWindowToCursor(QPointF cursorPosition) const;
    bool syncWindowToCursor(QPointF cursorPosition) const;
    bool shouldBlockInput(QPointF cursorPosition) const;

    // Effect Interface
    void prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &region, LogicalOutput *screen) override;
    void postPaintScreen() override;
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

private:
    enum class Shader: int {
        Builtin = 0
    };
    Q_ENUM(Shader)

    GLShader *getShader(Shader);

    struct Settings {
        Shader shader{Shader::Builtin};
        //IgnoreAspectRatio, KeepAspectRatio, KeepAspectRatioByExpanding
        Qt::AspectRatioMode aspectRatio{Qt::KeepAspectRatio};
        QMargins margins{};
        bool blockScreenInput{false};
        bool confinePointer{false};
        bool noBorder{true};
    };

    struct State {
        EffectWindow *window{nullptr};
        bool isScaling{false};
        QRectF targetRect{};
        QPointF originalPosition{};
        bool originalNoBorder{false};
        bool originalKeepAbove{false};
        std::map<Shader, std::unique_ptr<KWin::GLShader>> shaders;
    };

    Settings m_settings;
    State m_state;

    // Inspired from KWin's zoom effect
    struct OffscreenBuffer {
        std::unique_ptr<GLTexture> texture;
        std::unique_ptr<GLFramebuffer> framebuffer;
    };
    std::unique_ptr<OffscreenBuffer> m_buffer;

    void createBuffer(const QSize &size, const ColorDescription &color);
    void freeBuffer();
};

} // namespace KWin
