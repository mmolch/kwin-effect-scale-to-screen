#include "scale_to_screen.h"

#include "input_event.h"
#include "effect/effecthandler.h"
#include <KGlobalAccel>
#include <KLocalizedString>
#include <QAction>
#include <QFileInfo>
#include "opengl/glutils.h"

namespace scaleToScreen {

Q_LOGGING_CATEGORY(lcScaleToScreen, "scaleToScreen")

ScaleToScreen::ScaleToScreen()
    : Effect()
    , InputEventFilter(InputFilterOrder::Effects)
    , m_config{"scaletoscreenrc"}
{
    #ifdef NDEBUG
        QLoggingCategory::setFilterRules(QStringLiteral("scaleToScreen.debug=false\n"));
    #endif // NDEBUG

    qCDebug(lcScaleToScreen) << "ScaleToScreen()";

    QAction *a = new QAction(this);
    a->setObjectName(QStringLiteral("Scale to Screen"));
    a->setText(i18n("Scale the active window to fullscreen"));
    KGlobalAccel::self()->setShortcut(a, {Qt::ALT | Qt::SHIFT | Qt::Key_A});

    connect(a, &QAction::triggered, this, &ScaleToScreen::toggleActiveWindow);
    connect(effects, &EffectsHandler::windowAdded, this, &ScaleToScreen::onWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted, this, &ScaleToScreen::onWindowDeleted);
    connect(effects, &EffectsHandler::windowActivated, this, &ScaleToScreen::onWindowActivated);
}

ScaleToScreen::~ScaleToScreen()
{
    qCDebug(lcScaleToScreen) << "~ScaleToScreen()";

}

AppSettings ScaleToScreen::getAppSettings(EffectWindow *w) const
{
    if (!w) {
        return AppSettings{QStringLiteral("Default"), false};
    }

    const QString windowClass = w->windowClass();
    const QString windowTitle = w->caption();

    KConfigGroup appsGroup(&m_config, QStringLiteral("Applications"));

    for (const QString &name : appsGroup.groupList()) {
        KConfigGroup g = appsGroup.group(name);

        const MatchMode wcmm = static_cast<MatchMode>(std::clamp(g.readEntry("WindowClassMatchMode", static_cast<int>(MatchMode::Ignore)), 0, 3));
        const MatchMode wtmm = static_cast<MatchMode>(std::clamp(g.readEntry("WindowTitleMatchMode", static_cast<int>(MatchMode::Ignore)), 0, 3));

        if ((wcmm == MatchMode::Ignore) && (wtmm == MatchMode::Ignore)) {
            continue;
        }

        const QString classPattern = g.readEntry("WindowClass", QString());

        if (!matchStringWithMode(windowClass, classPattern, wcmm)) {
            continue;
        }

        const QString titlePattern = g.readEntry("WindowTitle", QString());

        if (!matchStringWithMode(windowTitle, titlePattern, wtmm)) {
            continue;
        }

        // If both matched (or were ignored), return the settings
        const QString profile = g.readEntry("Profile", QStringLiteral("Default"));
        const bool autoScale = g.readEntry("AutoScale", false);

        return AppSettings{profile, autoScale};
    }

    return AppSettings{QStringLiteral("Default"), false};
}

ScalerSettings ScaleToScreen::getScalerSettings(const QString &profile) const
{
    ScalerSettings settings{};

    KConfigGroup profilesGroup(&m_config, QStringLiteral("Profiles"));
    KConfigGroup g = profilesGroup.group(profile);
    if (g.exists()) {
        settings.aspectRatio = static_cast<Qt::AspectRatioMode>(std::clamp(g.readEntry("AspectRatioMode", static_cast<int>(Qt::AspectRatioMode::KeepAspectRatio)), 0, 1));
        settings.margins = {
            qMax(0, g.readEntry("MarginLeft", 0)),
            qMax(0, g.readEntry("MarginTop", 0)),
            qMax(0, g.readEntry("MarginRight", 0)),
            qMax(0, g.readEntry("MarginBottom", 0)),
        };
    }

    return settings;
}

void ScaleToScreen::toggleActiveWindow()
{
    qCDebug(lcScaleToScreen) << "toggleActiveWindow()";

    EffectWindow *active = effects->activeWindow();
    if (!active) {
        return;
    }

    if (findScaler(active)) {
        removeScaler(active);
    } else {
        auto *window = active->window();

        bool allowedWindowTypes = window->isNormalWindow();

        if (allowedWindowTypes) {
            if (!active->window()->isFullScreen()) {
                addScaler(active);
            }
        }
    }
}

void ScaleToScreen::addScaler(EffectWindow *w, std::optional<ScalerSettings> settings)
{
    qCDebug(lcScaleToScreen) << "addScaler()" << w;

    if (findScaler(w)) {
        return;
    }

    if (!settings) {
        settings = getScalerSettings("Default");
    }

    auto insertResult = m_suspendedScalers.insert({w, ScalerPtr{new Scaler(*this, *w, *settings)}});
    Scaler *scaler = insertResult.first->second.get();
    connect(scaler, &Scaler::isScalingChanged, [this, w](bool isScaling) {
        if (isScaling) {
            qCDebug(lcScaleToScreen) << "Adding to scalers" << w;

            if (m_scalers.empty()) {
                qCDebug(lcScaleToScreen) << "installInputEventFilter";
                input()->installInputEventFilter(this);
            }

            auto it = m_suspendedScalers.find(w);
            m_scalers.insert(m_suspendedScalers.extract(it));

        } else {
            qCDebug(lcScaleToScreen) << "Adding to suspended scalers" << w;

            auto it = m_scalers.find(w);
            m_suspendedScalers.insert(m_scalers.extract(it));

            if (m_scalers.empty()) {
                qCDebug(lcScaleToScreen) << "uninstallInputEventFilter";
                input()->uninstallInputEventFilter(this);
            }
        }
    });
    scaler->setScaling(true);
}

ScalerPtr ScaleToScreen::removeScaler(EffectWindow *w)
{
    qCDebug(lcScaleToScreen) << "removeScaler()" << w;

    if (auto it = m_scalers.find(w); it != m_scalers.end()) {
        it->second->setScaling(false);
    }

    if (auto it = m_suspendedScalers.find(w); it != m_suspendedScalers.end()) {
        auto scalerPtr = std::move(it->second);
        m_suspendedScalers.erase(it);
        return scalerPtr;
    }

    return nullptr;
}

Scaler *ScaleToScreen::findScaler(EffectWindow *w) const
{
    if (m_suspendedScalers.contains(w)) {
        return m_suspendedScalers.at(w).get();
    }

    if (m_scalers.contains(w)) {
        return m_scalers.at(w).get();
    }

    return nullptr;
}

void ScaleToScreen::clearScreen()
{
    //Clear the screen to black (pillarboxing)
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
}

void ScaleToScreen::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
{
    if (!m_scalers.empty()) {
        data.mask |= PAINT_WINDOW_OPAQUE;
        data.mask |= ~PAINT_SCREEN_BACKGROUND_FIRST;
    }
    effects->prePaintScreen(data, presentTime);
}

void ScaleToScreen::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const Region &region, WindowPaintData &data)
{
    if (m_scalers.empty()) {
        effects->paintWindow(renderTarget, viewport, w, mask, region, data);
        return;
    }

    if (auto it = m_scalers.find(w); it != m_scalers.end()) {
        auto *scaler = it->second.get();
        clearScreen();
        scaler->renderScaledWindowItem(renderTarget, viewport, region);
    } else {
        effects->paintWindow(renderTarget, viewport, w, mask, region, data);
    }
}

void ScaleToScreen::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)
    qCDebug(lcScaleToScreen) << "reconfigure()";
    QFileInfo(m_config.name()).refresh();
    m_config.markAsClean();
    m_config.reparseConfiguration();
}

bool ScaleToScreen::shouldBlockInput(QPointF pos) const
{
    // I'm currently only allowing one active scaler in the first place

    // Block if the cursor is outside the target window
    const bool cursorIsOutsideTargetRect = !m_scalers.begin()->second->m_state.targetRect.contains(pos);

    // Block if the cursor isn't inside the actual window
    const bool cursorIsNotInWindow = !m_scalers.begin()->second->m_window.frameGeometry().contains(pos.toPoint());

    return cursorIsOutsideTargetRect || cursorIsNotInWindow;
}

bool ScaleToScreen::handlePointer(const QPointF &pos)
{
    // I'm currently only allowing one active scaler in the first place
    auto *scaler =  m_scalers.begin()->second.get();

    scaler->constrainCursor(pos);
    scaler->syncWindowToCursor(pos);
    return shouldBlockInput(pos);
}

bool ScaleToScreen::pointerMotion(PointerMotionEvent *event)
{
    return handlePointer(event->position);
}

bool ScaleToScreen::pointerButton(PointerButtonEvent *event)
{
 return handlePointer(event->position);
}

bool ScaleToScreen::pointerAxis(PointerAxisEvent *event)
{
    return handlePointer(event->position);
}

void ScaleToScreen::onWindowAdded(EffectWindow *w)
{
    if (!w) {
        return;
    }

    qCDebug(lcScaleToScreen) << "onWindowAdded()" << w;

    if (!w->window()->isNormalWindow()) {
        return;
    }

    auto appSettings = getAppSettings(w);
    if (appSettings.autoScale) {
        addScaler(w);
    }
}

void ScaleToScreen::onWindowDeleted(EffectWindow *w)
{
    if (!w) {
        return;
    }

    qCDebug(lcScaleToScreen) << "onWindowDeleted()" << w;

    if (auto *scaler = findScaler(w)) {
        scaler->m_state.windowDeleted = true;
        removeScaler(w);
    }
}

void ScaleToScreen::onWindowActivated(EffectWindow *w)
{
    if (!w) {
        return;
    }

    qCDebug(lcScaleToScreen) << "onWindowActivated()" << w;

    // Only scale the active window for now if it wants to be scaled

    for (auto &scaler : m_scalers ) {
        scaler.second->setScaling(false);
    }

    auto *scaler = findScaler(w);
    if (scaler) {
        scaler->setScaling(true);
    }
}

} // namespace scaleToScreen
