#include "Application/XiEApplication.h"

#include "Input/InputEvents.h"
#include "UI2D/Core/UIScene.h"
#include "UI2D/Core/UITransform.h"
#include "UI2D/Core/UIVisual.h"
#include "UI2D/Input/UIEventContext.h"
#include "UI2D/Layout/UILayoutTypes.h"
#include "UI2D/Runtime/UIWindowRuntime.h"

#include <cmath>
#include <string_view>
#include <utility>

namespace {

int RunSample() {
    using namespace Engine;
    using namespace Engine::UI2D;

    auto created = ApplicationContext::Create();
    if (!created) {
        return created.Error().code == ApplicationErrorCode::Unsupported ? 0 : 1;
    }
    auto app = std::move(created.Value());

    TransparentWindowOptions options;
    options.title = "XiE Transparent Window Sample";
    options.size = {640, 400};
    options.clickThrough = false;
    options.focusPolicy = WindowFocusPolicy::NoActivate;
    options.opacity = 0.92f;

    auto createdWindow = app->Windows().CreateTransparent(options);
    if (!createdWindow) {
        const auto code = createdWindow.Error().code;
        (void)app->Shutdown();
        return code == ApplicationErrorCode::Unsupported ? 0 : 1;
    }
    auto window = std::move(createdWindow.Value());
    auto bound = window.BindUI();
    if (!bound) {
        (void)app->Shutdown();
        return 1;
    }
    auto* ui = bound.Value();
    if (ui == nullptr) {
        (void)app->Shutdown();
        return 1;
    }

    const auto root = ui->Scene().CreateNode("TransparentSample.Root");
    UILayoutParams rootLayout;
    rootLayout.sizeRule.width = {UISizeMode::Fixed,
        static_cast<float>(options.size.width)};
    rootLayout.sizeRule.height = {UISizeMode::Fixed,
        static_cast<float>(options.size.height)};
    ui->Scene().SetLayout(root, rootLayout);
    ui->Scene().SetVisual(root, UIPanelVisual{
        {0.03f, 0.06f, 0.12f, 0.94f}, 18.0f, true});

    const auto control = ui->Scene().CreateNode("TransparentSample.Control", root);
    UILayoutParams controlLayout;
    controlLayout.sizeRule.width = {UISizeMode::Fixed, 220.0f};
    controlLayout.sizeRule.height = {UISizeMode::Fixed, 88.0f};
    UITransform controlTransform;
    controlTransform.position = {32.0f, 32.0f};
    ui->Scene().SetLayout(control, controlLayout);
    ui->Scene().SetTransform(control, controlTransform);
    ui->Scene().SetVisual(control, UIPanelVisual{
        {0.15f, 0.72f, 0.96f, 0.96f}, 14.0f, true});
    ui->Scene().SetFocusable(control, true);
    ui->Scene().SetHitTestVisible(control, true);

    bool inputSeen = false;
    ui->AddEventListener(control, UIEventType::PointerDown,
        UIEventPhaseMask::All,
        [&inputSeen](UIEventContext&) { inputSeen = true; });

    if (!window.Show() || !window.Hide() || !window.Show()) {
        (void)app->Shutdown();
        return 1;
    }
    const auto dpi = window.QueryDpi();
    const auto focus = window.QueryFocus();
    if (!dpi || !focus || dpi.Value() <= 0.0f ||
        options.focusPolicy != WindowFocusPolicy::NoActivate) {
        (void)app->Shutdown();
        return 1;
    }

    int frameCount = 0;
    ApplicationRunOptions run;
    run.update = [&app, &window, &frameCount](WindowSession&, double) {
        ++frameCount;
        if (frameCount == 1) {
            const auto handle = window.Handle();
            (void)window.UI()->EnqueueInput(InputEvent{PointerMoveEvent{
                handle, {64.0f, 64.0f}, InputModifiers::None, 0, 0.0}});
            (void)window.UI()->EnqueueInput(InputEvent{PointerButtonEvent{
                handle, {64.0f, 64.0f}, PointerButton::Left, true,
                InputModifiers::None, 0, 0.0}});
            (void)window.UI()->EnqueueInput(InputEvent{PointerButtonEvent{
                handle, {64.0f, 64.0f}, PointerButton::Left, false,
                InputModifiers::None, 0, 0.0}});
        }
        if (frameCount >= 2) (void)app->RequestExit();
    };
    const auto ran = app->Run(run);
    if (!ran || !inputSeen) {
        (void)window.Close();
        (void)app->Shutdown();
        return 1;
    }

    if (!window.Close() || !window.IsClosed()) {
        (void)app->Shutdown();
        return 1;
    }
    return app->Shutdown() ? 0 : 1;
}

} // namespace

int main(int argc, char**) {
    // The sample intentionally has no project-business or platform-native dependency.
    // A future sample runner may pass arguments; keep the entry point stable.
    (void)argc;
    return RunSample();
}
