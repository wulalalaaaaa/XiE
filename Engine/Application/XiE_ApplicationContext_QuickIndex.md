# XiE ApplicationContext 快速索引

这是 XiE 应用组合入口的权威索引。应用代码优先从
`Application/XiEApplication.h` 开始；只有需要特殊后端控制时，才直接引用
`ApplicationRuntime`、`IWindowSystem` 或其他底层契约。

## 1. 创建默认应用

推荐入口：`Engine::ApplicationContext::Create`。

```cpp
#include "Application/XiEApplication.h"

auto created = Engine::ApplicationContext::Create();
if (!created) {
    const Engine::ApplicationError& error = created.Error();
    // error.code / error.operation / error.capability / error.message
    // error.nativeCode / error.backend 可用于日志和诊断。
    return;
}
auto app = std::move(created.Value());
```

`ApplicationContextOptions` 字段：

* `applicationName`：应用显示名，默认 `XiE Application`，长度为 1..256 字节；
* `initializeBackends`：是否让 XiE 初始化默认后端，默认 `true`；
* `runtime`：可选的 XiE `ApplicationRuntime`，用于宿主已经拥有 Runtime 时的适配；
* `desktopBackend`：可选的 `IDesktopBackend`，由平台 target 显式注入；不注入时
  Desktop Services 仍可创建，但相关能力报告为 `Unavailable`。
* `imageServices`：可选的 `ImageServices`，由应用显式注入；不注入时图片 codec/
  clipboard 能力保持 `Unavailable`，不会拉入 Windows 图片库。

默认创建会按需初始化当前构建和平台可用的窗口后端；不会因为创建应用而伪造
Image 或 Media 服务。Desktop Services 由 `app->Desktop().Start()` 按需启动；创建、
窗口、桌面服务和生命周期操作应在创建线程执行。

检索关键词：`ApplicationContext Create ApplicationResult application options`

## 2. 创建标准窗口

推荐入口：`app->Windows().CreateStandard(options)`。

```cpp
Engine::StandardWindowOptions options;
options.title = "Settings";
options.size = {960, 640};
options.visible = false;
options.focusPolicy = Engine::WindowFocusPolicy::Preserve;
options.closePolicy = Engine::WindowClosePolicy::Hide;

auto createdWindow = app->Windows().CreateStandard(options);
if (!createdWindow) {
    // 检查 createdWindow.Error().code == Unsupported 或 BackendFailure。
    return;
}
Engine::WindowSession window = std::move(createdWindow.Value());
```

`StandardWindowOptions` 提供：`title`、`size.width/height`、`visible`、
`resizable`、`decorated`、`alwaysOnTop`、`toolWindow`、`placement.x/y/maximized`、
`focusPolicy`、`closePolicy` 和 `dpiPolicy`。默认值可以直接创建普通窗口。

XiE 在组合实现内部选择标准窗口的具体后端；应用不需要填写 GLFW、OpenGL、Win32
或原生句柄字段。底层 `IWindowSystem` 仍可供高级宿主单独使用。

检索关键词：`StandardWindowOptions CreateStandard WindowSession placement focus close dpi`

### Settings 类标准窗口最短示例

Settings、About、Preferences 等普通应用窗口沿用同一个 Context，并让 XiE
一次性补齐 UI、输入、渲染、文本和 IME 默认服务：

```cpp
auto app = Engine::ApplicationContext::Create();
if (!app) return;
Engine::StandardWindowOptions settingsOptions;
settingsOptions.title = "Settings";
settingsOptions.visible = false;
settingsOptions.focusPolicy = Engine::WindowFocusPolicy::Preserve;
settingsOptions.closePolicy = Engine::WindowClosePolicy::Hide;
auto settings = app.Value()->Windows().CreateStandard(settingsOptions);
if (!settings) return;
auto session = std::move(settings.Value());
auto ui = session.BindUI(); // 空 Options 使用 XiE-owned 默认组合
if (!ui) return;
```

应用只持有 `WindowSession` 和 `UIWindowRuntime*` 借用视图；窗口、InputQueue、
UI2D、Renderer、IME 和关闭顺序仍由 XiE 组合层拥有。检索关键词：
`Settings CreateStandard BindUI WindowClosePolicy::Hide ApplicationContext`。

## 3. 创建透明/悬浮窗口

推荐入口：`app->Windows().CreateTransparent(options)`。

```cpp
Engine::TransparentWindowOptions options;
options.size = {640, 400};
options.alwaysOnTop = true;
options.toolWindow = true;
options.focusPolicy = Engine::WindowFocusPolicy::NoActivate;
options.clickThrough = false;
options.opacity = 0.92f;

auto overlay = app->Windows().CreateTransparent(options);
if (!overlay) {
    // 非 Windows 或可选后端未初始化时，错误码是 Unsupported。
    return;
}
auto window = std::move(overlay.Value());
```

`TransparentWindowOptions` 提供：`title`、`size`、`visible`、`transparent`、
`alwaysOnTop`、`toolWindow`、`focusPolicy`、`clickThrough`、`opacity`、
`placement`、`closePolicy` 和 `dpiPolicy`。`opacity` 必须在 0..1，size 必须为正数。

透明窗口的后端映射只存在于 XiE 组合实现内部；当前默认实现使用可选的 Win32
layered/software 路径。应用代码只使用 Session 和 `ApplicationContext`，不接触
Poll/Present 或平台 API。需要 TextBox、IME 或首击获得焦点的交互悬浮窗，应显式
使用 `focusPolicy = Activate`；只提示、不抢焦点的层才使用 `NoActivate`。
`clickThrough = false` 是可交互默认值；关闭策略通常使用 `Hide`，这样重开复用同一
Session，不会重复创建窗口或回调。

### 交互透明窗口最短示例

```cpp
Engine::TransparentWindowOptions options;
options.visible = false;
options.focusPolicy = Engine::WindowFocusPolicy::Activate;
options.clickThrough = false;
options.alwaysOnTop = true;
options.toolWindow = true;
options.closePolicy = Engine::WindowClosePolicy::Hide;
auto created = app->Windows().CreateTransparent(options);
if (!created) return;
auto overlay = std::move(created.Value());
auto ui = overlay.BindUI(); // XiE-owned InputQueue/UI/Renderer/IME
if (!ui || !overlay.Show()) return;
```

`BindUI()` 后可以直接创建 Button、TextBox 和其他 UI2D widget；业务层只保存
`WindowSession` 与借用的 `UIWindowRuntime*`。在 XiE 自有 `Run()` 中，窗口会自动加入
统一的 update/render/wait/wake 调度；仍由旧宿主持有外层循环时，先用
`PumpWindowEvents()`（仅当 Context 的可选窗口后端需要泵消息），再用
`ProcessWindowEvents()`、`ProcessWindowFrame(session, delta)` 和
`RenderWindow(session, true)`，其中 `true` 表示由桥接方请求这次透明 surface 的
presentation。隐藏的 Session（Hidden）不产生持续帧工作，`Show()` 会重新标记首帧。

透明像素的逐像素 alpha、软件 raster、premultiplied surface、逻辑尺寸到 DPI
framebuffer 的换算都由 XiE 后端负责；跨平台公共接口不暴露 HWND、GLFW、OpenGL、
HDC 或 Layered API。真实视觉透明度、物理鼠标、中文 IME 和多显示器混合 DPI 仍需
在人工桌面验收中单独记录。

检索关键词：`TransparentWindowOptions CreateTransparent BindUI interactive overlay TextBox IME clickThrough focusPolicy Activate NoActivate opacity layered DPI`

### Standard + Transparent 最小组合

两个窗口可以由同一个 `ApplicationContext` 创建、分别绑定 UI，并共享一条
`Run`/`Wait`/`Wake` 生命周期：

```cpp
Engine::StandardWindowOptions standardOptions;
standardOptions.closePolicy = Engine::WindowClosePolicy::RequestApplicationExit;
auto standard = app->Windows().CreateStandard(standardOptions);

Engine::TransparentWindowOptions transparentOptions;
transparentOptions.focusPolicy = Engine::WindowFocusPolicy::NoActivate;
transparentOptions.clickThrough = false;
auto transparent = app->Windows().CreateTransparent(transparentOptions);
if (!standard || !transparent) return;

auto standardUI = standard.Value().BindUI();
auto transparentUI = transparent.Value().BindUI();
if (!standardUI || !transparentUI) return;

Engine::ApplicationRunOptions run;
run.update = [](Engine::WindowSession&, double) {};
run.render = [](Engine::WindowSession&) {};
(void)app->Run(run);
```

`NoActivate` 透明窗口默认不抢标准窗口焦点；`clickThrough=false` 时仍可接收指针。
常见的跨窗口显示控制只需要调用另一个 Session 的 `Show`/`Hide`。隐藏全部窗口时，
宿主可调用 `Wait(timeout)`，Desktop worker 或其他外部事件通过 `Wake()` 让同一条
循环继续处理；不要为每个窗口创建第二条消息循环。仍在迁移中的旧宿主可以注册
`SetExternalWakeCallback([&] { legacyHost.WakeEventLoop(); })`，把同一 Context 的
Desktop/Shortcut/IPC 唤醒转发到现有外层循环；在宿主关闭前清空该 callback。

检索关键词：`ApplicationContext CreateStandard CreateTransparent BindUI Run Wait Wake dual window NoActivate`

## 4. 绑定 UI、输入、文本和渲染

推荐入口：`window.BindUI()`。默认绑定一次性建立：

* `InputEventQueue`；
* UI2D layout、hit-test、input router 和 focus manager；
* `UIWindowRuntime`；
* `TextLayoutService`；
* 当前平台默认 `ITextClipboard`、`ITextInputMethod`（平台不具备时保持为空）；
* `TextureRegistry2D`；
* 对应窗口 RenderPipeline 的纹理和文本资源。

```cpp
auto bound = window.BindUI();
if (!bound) return;
Engine::UI2D::UIWindowRuntime* ui = bound.Value();
auto root = ui->Scene().CreateNode("Root");
```

高级调用者可通过 `UI2DBindingOptions` 注入允许替换的 XiE 服务指针。注入对象是
借用关系，必须长于 WindowSession；组合层不重新实现 Button、TextBox、IME 或
Renderer。重复绑定返回 `ApplicationErrorCode::AlreadyBound`。

UI、输入、Focus、IME 和 DPI 都以同一 WindowSession 为边界。窗口隐藏时沿用
`UIWindowRuntime::SetVisible(false)` 的现有语义，关闭时先释放 UI binding，再释放
RenderPipeline、窗口资源和原生窗口。

检索关键词：`WindowSession BindUI UI2DBindingOptions UIWindowRuntime InputQueue Focus IME`

## 5. 运行、唤醒、退出和关闭

推荐入口：`Run`、`Wake`、`RequestExit`、`Shutdown`。

```cpp
Engine::ApplicationRunOptions run;
run.update = [](Engine::WindowSession& window, double deltaSeconds) {
    (void)window;
    (void)deltaSeconds;
};
run.render = [](Engine::WindowSession& window) { (void)window; };

auto result = app->Run(run);
// Run 返回 ApplicationResult<void>。
(void)app->Shutdown(); // 可重复调用。
```

`Run` 只允许在应用创建线程、且不能同时运行；重复运行返回 `Busy`。所有新的
应用组合操作都返回 `ApplicationResult<T>`，错误包含稳定 `code`、`operation`、
`capability`、`message`、可选 `nativeCode` 和 `backend`。常见错误码：
`InvalidOptions`、`Unsupported`、`WrongThread`、`AlreadyExists`、`AlreadyBound`、
`Busy`、`LifecycleViolation`、`BackendFailure`、`ShuttingDown`、`UnknownFailure`。

`WindowSession` 是 move-only RAII 对象；`Close`、Session 析构和
`ApplicationContext::Shutdown` 都是幂等操作。关闭顺序由 XiE 组合层负责，调用方不
应保存 GLFWwindow、HWND、HDC 或其他原生句柄。

窗口任务方法：`Show`、`Hide`、`Activate`、`SetPosition`、`SetSize`、`QueryDpi`、
`QueryFocus`、`RequestClose`、`Close`、`QueryState`、`QueryPlacement`、`Status`、
`BackendName`、`LastError`。`ProcessWindowEvents()` 用于已经由外层宿主泵入
native 状态的迁移场景；它只同步 XiE Session 并应用 `WindowClosePolicy`，不创建
第二条事件循环或暴露平台句柄。若窗口消息由 Context 自己的可选后端拥有，外层宿主
应先调用 `PumpWindowEvents()`，再调用 `ProcessWindowEvents()`；该泵入口仍只服务
同一 Context，不创建第二条事件循环。标准 Context 自己运行时则由 `Run` 统一处理。
外层宿主若仍负责调度，也可按窗口调用 `ProcessWindowFrame()` 和 `RenderWindow()`；
这两个入口只组合 XiE 的 UI/渲染帧，不接管 native poll。

### 交互式搜索悬浮窗最短组合

搜索框、候选按钮和结果皮肤仍由应用层创建；XiE 只负责透明窗口及其通用 UI
组合。推荐把 `WindowSession` 存在应用对象中，第一次激活时创建，后续只调用
`Show()`/`Activate()`，隐藏时调用 `Hide()`：

```cpp
std::optional<Engine::WindowSession> searchWindow;

Engine::ApplicationResult<void> ShowSearch() {
    if (!searchWindow) {
        Engine::TransparentWindowOptions options;
        options.title = "Search";
        options.size = {900, 720};
        options.visible = false;
        options.transparent = true;
        options.alwaysOnTop = true;
        options.toolWindow = true;
        options.focusPolicy = Engine::WindowFocusPolicy::Activate;
        options.clickThrough = false;
        options.closePolicy = Engine::WindowClosePolicy::Hide;
        auto created = app->Windows().CreateTransparent(options);
        if (!created) return Engine::ApplicationResult<void>::Failure(
            created.Error());
        searchWindow.emplace(std::move(created.Value()));
        auto bound = searchWindow->BindUI();
        if (!bound) return Engine::ApplicationResult<void>::Failure(
            bound.Error());
        // Build SearchBox/Buttons here against bound.Value()->Scene().
    }
    return searchWindow->Activate();
}

Engine::ApplicationResult<void> HideSearch() {
    return searchWindow ? searchWindow->Hide()
                        : Engine::ApplicationResult<void>::Success();
}
```

`Activate` 适合带 TextBox 的交互搜索层；不要把它永久配置成 `NoActivate`。
产品输入策略可在 `WindowFocusLost`/失焦（blur）规则或 Escape 事件中调用 `Hide()`，选中
结果后先完成应用自己的复制/粘贴业务再隐藏。`Hide()` 会清除窗口 UI 的焦点、IME
和隐藏期间的输入；重新 `Activate()` 不会复用旧的按键或焦点请求。透明帧只在
`QueryWindowActivity()` 报告动画、输入、脏帧或外部结果时调度；无动画的隐藏/静态
窗口应进入睡眠。Preview 等 worker 只能通过 `SetExternalWakeCallback()`/`Wake()`
唤醒外层循环，像素和 UI 仍在 owner thread 的 `ProcessWindowFrame()` 中应用。

检索关键词：`QuickSearch CreateTransparent WindowSession Activate Focus WindowFocusLost Hide Wake QueryWindowActivity Preview`

检索关键词：`ApplicationRunOptions Run Wake RequestExit Shutdown WindowSession lifecycle`
以及 `PumpWindowEvents ProcessWindowEvents QueryPlacement BackendName`

## 6. 查询能力和错误

```cpp
const Engine::CapabilitySnapshot& capabilities = app->Capabilities();
if (const auto* standard = capabilities.Find(
        Engine::ApplicationCapability::StandardWindow)) {
    // status: Supported / Unavailable / Degraded
    // compileTimeAvailable / platformAvailable / runtimeInitialized
    // reason / backend
}
```

Desktop 能力的 `CapabilityInfo` 会在查询时从 `DesktopServices` 刷新动态状态；
Desktop 子能力还提供 `CapabilityHealth`：`status`、稳定 `reason`、
`lastSuccessfulOperation`、`lastFailedOperation`、`lastNativeError`、
`consecutiveFailures`、`recovering` 和 `revision`。例如：

```cpp
const auto health = app->Desktop().Health();
if (const auto* tray = capabilities.Find(
        Engine::ApplicationCapability::SystemTray)) {
    // tray->health 与 app->Desktop().Health().systemTray 表示同一状态。
    // Busy/InvalidOptions 是本次调用错误，不会污染服务 health。
    (void)tray;
}
```

`Supported` 表示最近操作正常，`Degraded` 表示服务仍可观察但最近发生了后端故障，
`Unavailable` 表示当前环境、模块或生命周期不具备该能力。成功恢复可以回到
`Supported`，但最近失败操作、native error 和 revision 仍可追溯。调用返回的
`Busy`、`InvalidOptions`、`WrongThread` 与服务健康状态是两条不同维度。

当前快照覆盖：`StandardWindow`、`TransparentWindow`、`UI2D`、
`PointerAndKeyboardInput`、`TextInput`、`TextClipboard`、`OpenGLPresentation`、
`LayeredPresentation`、`Diagnostics`，以及明确标记为未实现的可选
`DesktopIntegration`、`MediaPlayback`。未实现的可选能力不会返回空壳服务；reason
使用可检索 token，例如 `module_not_implemented:*`、
`runtime_init_failed:*`、`platform_or_runtime_unavailable:*`、
`runtime_probe_deferred:*`。

`CapabilitySnapshot::Supports` 只把 `Supported` 和 `Degraded` 视为可用；调用者在
需要严格保证时还应读取 `CapabilityInfo` 的详细状态。

检索关键词：`CapabilitySnapshot CapabilityInfo Supported Unavailable Degraded reason`

## 7. Desktop Services

推荐入口：`app->Desktop()`，按需调用 `Start`，再从 `Tray()`、`Shortcuts()`、
`Instances()` 取得任务导向 API。

```cpp
auto desktopStarted = app->Desktop().Start();
if (!desktopStarted) {
    // 读取 desktopStarted.Error() 的 code/capability/reason/backend。
    return;
}

Engine::TrayOptions trayOptions;
trayOptions.tooltip = "My Application";
trayOptions.menu = {{1, "Open", Engine::TrayMenuItemKind::Command,
    true, false, true}};
trayOptions.onCommand = [](Engine::DesktopCommandId command) {
    (void)command; // 业务映射留在应用层。
};
auto tray = app->Desktop().Tray().Create(trayOptions);

Engine::ShortcutOptions shortcutOptions;
shortcutOptions.key = Engine::InputKeyCode::S;
shortcutOptions.modifiers = Engine::InputModifiers::Shift;
auto shortcut = app->Desktop().Shortcuts().Register(shortcutOptions);

// XiE 统一键值也覆盖数字、功能键和导航键。
shortcutOptions.key = Engine::InputKeyCode::Digit5;
auto digitShortcut = app->Desktop().Shortcuts().Register(shortcutOptions);
shortcutOptions.key = Engine::InputKeyCode::F5;
auto functionShortcut = app->Desktop().Shortcuts().Register(shortcutOptions);
shortcutOptions.key = Engine::InputKeyCode::PageUp;
auto navigationShortcut = app->Desktop().Shortcuts().Register(shortcutOptions);

Engine::SingleInstanceOptions instanceOptions;
instanceOptions.applicationId = "com.example.my_application";
auto instance = app->Desktop().Instances().Acquire(instanceOptions);
```

`DesktopServices` 是可选扩展：`xie_desktop` 只提供跨平台 contracts，Windows
Shell、快捷键、互斥和激活通道位于可选 `xie_desktop_win32` backend。没有注入 backend
时不会链接或调用 Windows API，查询 `DesktopIntegration`、`SystemTray`、
`GlobalShortcut`、`SingleInstance`、`PrimaryInstanceActivation` 的详细
`CapabilityInfo`，并按 `Supported`、`Unavailable`、`Degraded` 及 `reason/backend`
处理结果。

`app->Desktop().Capabilities().environment` 可进一步区分
`Ready`、`ExplorerMissing`、`NotificationAreaUnavailable`、
`ShellRestarting`、`PermissionDenied`、`SessionIsolated`、`InvalidParameters` 和
`BackendFailure`。真实托盘 smoke 是显式 opt-in；交互桌面上可运行：

```powershell
$env:XIE_RUN_DESKTOP_SMOKE="1"
$env:XIE_DESKTOP_SMOKE_WAIT_MS="30000"
.\xie_desktop_native_smoke.exe
```

程序输出 JSONL；每行有 `phase`、`result`、`code`、`environment`、`reason` 和
`detail`。退出码约定为：`0` 表示未启用或 setup/cleanup 完成，`10` Context 创建失败，
`11` Desktop Start 失败，`12` 托盘添加或更新失败，`13` 快捷键注册/冲突/注销失败，
`14` 单实例失败，`15` Pump 失败，`16` Shutdown 失败，`20` 表示
`Environment Blocked / Not Tested`。`XIE_DESKTOP_SMOKE_WAIT_MS` 只控制人工操作窗口，
超过 1 秒时日志会提示可点击托盘、双击和按下 `Shift+Alt+S`；回调计数为零不能解释
成物理交互已验证。预检失败时不会自动结束或重启 Explorer。需要验证
`TaskbarCreated` 恢复时，应在普通桌面会话中由用户手动重启 Shell 后再次运行 smoke，
本工具不执行该动作。

Tray 使用稳定的 `DesktopCommandId`，支持 tooltip、菜单项、分隔符、enabled、checked、
default、更新、双击和幂等移除；快捷键使用 XiE `InputKeyCode`/
`InputModifiers`，注册结果是 move-only `ShortcutRegistration`。单实例返回
`PrimaryAcquired`、`ExistingInstanceActivated`、`AlreadyRunningButUnreachable`、
`InvalidApplicationId` 或 `BackendFailure`；当前 activation 只表达“通知并激活主实例”，
不承载业务命令、文件或任意 payload。激活请求有固定协议、零 payload、长度上限和
有界 timeout；格式错误、权限/会话隔离和 endpoint 不可达都应通过结构化结果或
health reason 处理。安全通道只授予当前用户/登录会话必要主体，并拒绝远程客户端。

Desktop 回调由 backend 入队，最终由 Context owner thread 的 `Pump`/`Run` 消费；
`TraySession`、`ShortcutRegistration`、`SingleInstanceSession` 应在
`DesktopServices::Shutdown()` 前释放，`Start`、`Remove`、`Unregister` 和 `Shutdown`
均为幂等操作。主窗口通常使用 `WindowClosePolicy::RequestApplicationExit`，辅助窗口
通常使用 `WindowClosePolicy::Hide` 或显式 `Destroy`；`RequestClose` 只请求关闭，
`Hide` 保留 Session，`Close`/`Destroy` 释放该窗口资源。发生平台不支持或模块未链接时，调用方应保留应用主体功能并记录结构化
`Unsupported`/能力原因。

检索关键词：`Desktop DesktopServices Tray TraySession Shortcut ShortcutRegistration Instance SingleInstanceSession Activation Start Acquire Create Remove Query Shutdown`

## 8. 图片服务：ImageBuffer、Codec 与 Clipboard

图片交换是 XiE 的可选应用能力。平台无关的 `ImageBuffer` 负责像素描述和拥有
数据；`IImageCodec` 负责内存编解码；`IImageClipboardSource` 负责从剪贴板取得
已经解码的图片。它们通过 `ApplicationContext::Images()` 发现，内部仍是独立模块，
不是一个包含全部实现的图片 God Object。

取得服务并读取剪贴板图片的最短路径：

```cpp
Engine::ImageServices* images = app->Images();
if (images == nullptr || !images->HasClipboard()) return;
const auto clipboard = images->Clipboard();
const auto captured = clipboard->ReadImage();
if (!captured) {
    // captured.Error().code: NoData / Busy / Unsupported / InvalidData /
    // TooLarge / DecodeFailed / BackendUnavailable / WrongThread。
    return;
}
const Engine::ImageBuffer& image = captured.Value().image;
// image owns its bytes; captured.Value() may be retained by the caller.
```

从内存解码的路径不需要文件句柄或平台对象：

```cpp
if (images == nullptr || !images->HasCodec()) return;
Engine::ImageEncodedPayload payload;
payload.format = Engine::ImageEncodedFormat::Png;
payload.bytes.assign(encodedBytes.begin(), encodedBytes.end());
auto decoded = images->Codec()->Decode(payload);
if (!decoded) return;
const auto view = decoded.Value().View(); // borrowed read-only span
```

当前基础契约：`ImageBuffer` 深拷贝或移动拥有连续像素；`ImageView` 和
`MutableBytes()` 都是借用视图，不能跨越其 owner 的移动、赋值或析构。存储方向统一
为 top-down；支持 `BGRA8` 与 `RGBA8`，每行由 `rowStride`（字节）描述；Alpha 明确
区分 `Opaque`、`Straight`、`Premultiplied`，颜色空间可为 `Unknown` 或 `SRGB`。
尺寸、stride、总字节数和编码 payload 在分配/解码前检查上限和溢出。Windows
clipboard source 的同步读取必须在创建它的线程执行，并对 clipboard busy 进行有限重试；
WIC codec 是同步内存 codec，每次调用在实现内部处理 COM apartment，调用者不拥有
WIC buffer 或临时 clipboard memory。

能力快照只回答“服务是否配置、后端是否可用”：查询
`ApplicationCapability::ImageCodec`、`ImageClipboard`、`ImageFile` 与
`ImageAsyncTasks`。单次 `ApplicationResult`
才回答本次操作是没有图片、clipboard busy、格式不支持、payload 损坏、图片过大、
decode/encode 失败、backend unavailable 或 wrong thread；不要用能力快照替代单次
调用结果。

文件图片与异步预览使用同一份 `ImageBuffer`，不另造 decoded image 容器：

```cpp
Engine::ImageServices* images = app->Images();
if (images == nullptr || !images->HasFiles() || !images->HasAsync()) return;
const auto files = images->Files();
const auto tasks = images->Async();
const auto metadata = files->Inspect(sourcePath);       // 不分配像素
Engine::ImageAsyncRequest request;
request.sourcePath = sourcePath;
request.resize = Engine::ImageResizeOptions{
    256, 256, false, Engine::ImageResizeFilter::Linear, {}};
const auto token = tasks->Submit(std::move(request));
// 在 ApplicationContext owner thread 的 Tick/Pump 中：
tasks->DrainResults(16, [](Engine::ImageAsyncResult result) {
    if (result.state != Engine::ImageTaskState::Completed || !result.value)
        return; // error/message 见 result.error
    const Engine::ImageBuffer& owned = result.value->image;
    // ImageBuffer 持有 pixels；这里只借用 View/Bytes，不能越过 owner 生命周期。
});
```

`ImageFileService::ReadEncoded` 返回拥有的 `ImageEncodedPayload`；`Inspect` 只取
`ImageMetadata`；`Decode`/`DecodeThumbnail` 返回拥有的 `ImageDecodedFile`。默认缩放
保持宽高比且不放大，`maxWidth`、`maxHeight`、filter 和 `ImageLimits` 都可显式指定。
`ImageAsyncService` 使用首次 `Submit` 时惰性启动的有界 1～4 个 worker（默认由硬件线程数推导），队列和结果
队列都有上限；worker 只做文件读取、codec 解码和 CPU 缩放，不接触 UI、Texture 或
窗口。`Submit`、`Cancel`、`Shutdown` 可在服务控制线程调用，`DrainResults` 和其
consumer 必须在创建服务时记录的 owner thread 调用；取消、队列满、文件不存在、
权限/读取失败、损坏、解码失败和 shutdown 都通过结构化 `ApplicationResult`/
`ImageAsyncResult` 表达。`Shutdown` 会停止接收、清空未消费结果并 join worker，重复
调用幂等。若要扩展 codec/file provider，应注入 `IImageCodec` 或
`IImageFileService`；Windows WIC/COM 仍只在可选 backend 内部。

Windows 后端按需链接：纯契约和容器使用 `xie_image`；WIC 使用
`xie_image_wic`（`windowscodecs`、`ole32`、`uuid`）；Win32 clipboard 使用
`xie_image_clipboard_win32`（`user32`）；组合工厂使用 `xie_image_win32`。普通
游戏 target 不因包含 `ImageBuffer` 而自动链接这些 Windows 库。高级宿主可以在
`ApplicationContextOptions::imageServices` 注入自有的 `ImageServices`，或直接实现
`IImageCodec`/`IImageClipboardSource`；Windows provider、COM 和句柄只允许停留在
后端实现，不进入跨平台公共接口。

### ImageBuffer 到 Texture2D：owner-thread 桥接

CPU 图片和 Renderer2D 纹理是两个生命周期：`ImageBuffer`/`ImageView` 拥有或借用
`BGRA8`/`RGBA8`、stride、Alpha 和颜色空间；`TextureRegistry2D` 只保存 renderer
所需的紧凑 RGBA8 资源。需要显示图片时，应用在创建窗口的 owner thread 构造
`Engine::ImageTextureBridge`，然后调用：

```cpp
Engine::ImageTextureBridge bridge(textures);
auto created = bridge.CreateTexture(decoded.image); // BGRA/RGBA + padded stride
if (!created) return; // InvalidData / TooLarge / WrongThread / ShuttingDown
auto updated = bridge.UpdateTexture(created.Value(), decoded.image);
auto released = bridge.DestroyTexture(created.Value()); // stale release is idempotent
```

桥接层将非紧凑 row stride 转换为紧凑 RGBA8；BGRA 只交换通道，`Straight` 不再
预乘，`Premultiplied` 不重复预乘，`Opaque` 将 alpha 规范化为 255。worker 只能
返回 owned `ImageBuffer`，不得操作 `TextureRegistry2D`；`CreateTexture`、
`UpdateTexture`、`DestroyTexture` 和 `Shutdown` 必须在 bridge owner thread 调用。
bridge 不拥有图片或 registry，窗口/产品 cache 拥有 `TextureHandle`，并在 owner
thread 释放。一个 handle 不能跨 Registry/WindowSession 生命周期；Shutdown 后上传
返回结构化 `ShuttingDown`。

异步图片到虚拟网格的推荐顺序是：`ApplicationContext::Images()->Async()`
`Submit` -> owner thread `DrainResults` -> 校验 cell 的 ImageId/content key/
generation/revision -> `ImageTextureBridge::CreateTexture` -> 产品自己的
`TextureRegistry2D` cache。cell 回收、快速滚动或库切换时取消请求；晚到结果必须
先通过 identity 校验再上传。XiE 不拥有 Gallery 的 cell、数据库、可见优先级、LRU
或 cache key。

`xie_image_renderer2d` 是可选的桥接 target，只依赖平台无关的 `xie_image` 与
`xie_renderer2d_resources`，不链接 WIC、COM、clipboard、OpenGL、D3D 或 Win32。
因此纯图片契约、纯 Renderer 合约和 bridge 可以用于游戏/工具；只有显式链接
`xie_image_wic`、`xie_image_clipboard_win32` 或 `xie_image_win32` 的应用才取得
对应 Windows 后端。高级 backend/provider 入口仍可直接使用，但不应把平台句柄
加入公共接口。

检索关键词：`ImageTextureBridge ImageBuffer to Texture2D BGRA RGBA stride Alpha owner-thread CreateTexture UpdateTexture DestroyTexture AcquireImage DrainResults stale cell xie_image_renderer2d`

产品侧的 `emoexec_gallery_thumbnail_benchmark` 是独立的图片/纹理边界基准，默认只
生成临时 fixture，不扫描真实图库；要测试只读目录时显式传入 `--root`。例如：

```powershell
.\emoexec_gallery_thumbnail_benchmark.exe --count 256 --workers 2 `
  --queue 32 --thumb 96 --cache-mib 128 --output phase10.jsonl
```

它对同一输入集合输出 `cold`、`warm_worker`、`hot`、`fast_scroll` 和
`selection_churn` 的 JSONL，记录首屏、decode/scale
端到端延迟、owner-thread texture upload、命中/提交/完成/取消/stale/失败、队列峰值、
worker、纹理数量、CPU 图片峰值、进程工作集和 Shutdown。`hot` 的命中是 renderer
texture cache 命中；它不能替代人工 Gallery 视觉验收或真实 2 万+图库压力。

### Inspector 选择、异步性能与旧链路边界

Gallery Inspector 的推荐调用顺序仍是产品层保存选择身份，XiE 只负责图片任务：

```cpp
ProductInspectorPreviewRequest request;
request.identity = {libraryId, generation, imageId, contentKey,
    selectionRevision, 256};
request.sourcePath = sourcePath;
request.contentKey = contentKey;
request.maxEdge = 256;
request.visible = true;
auto token = inspectorPreviewAdapter.Request(std::move(request));

// 在 WindowSession owner thread：
inspectorPreviewAdapter.DrainResults(4, [&](auto result) {
    if (result.status != ProductInspectorPreviewStatus::Ready ||
        !result.image || result.identity != currentSelection ||
        result.contentKey != currentContentKey) return; // stale
    auto texture = textureCache.AcquireImage(
        currentContentKey + "|thumb=256", result.image->image, true);
    if (texture) currentInspectorTexture = *texture;
});
```

`InspectorPreviewAdapter` 是产品层的选择/去重/取消适配器，不是 XiE 公共类型；
它通过同一个 `ApplicationContext::Images()->Async()` 提交任务。dedup key 必须包含
源、内容 revision 和缩略图尺寸；不同尺寸不得误共享。同一 selection 的旧纹理在新
图片成功上传前保持 pinned，上传失败时继续显示旧纹理。库切换、generation、imageId、
contentKey 或 selection revision 任一变化都必须拒绝晚到结果；worker 只返回拥有的
`ImageBuffer`，TextureRegistry 只在 owner thread 操作。隐藏 Gallery 不应上传不可见
纹理，Shutdown 时先取消产品请求，再清理结果、texture 和 async worker。

图片阶段可通过两个可选 timing callback 定位性能问题，不改变业务契约：

```cpp
files->SetTimingCallback([](const Engine::ImageFileTiming& timing) {
    // readUs / formatDetectUs / codecProbeUs / codecDecodeUs /
    // scaleUs / totalUs / nativeThumbnail / success
});
Engine::ImageAsyncOptions options;
options.timingCallback = [](const Engine::ImageAsyncTaskTiming& timing) {
    // queueWaitUs / fileServiceUs / resultEnqueueUs /
    // submittedAt / workerStartedAt / fileStartedAt / fileFinishedAt /
    // resultEnqueuedAt / cancelled / success
};
// 可在 ApplicationContext 已创建后替换或关闭诊断 callback；返回值区分
// 正常安装与 ShuttingDown，callback 仍只在 worker/file 线程执行。
auto timingResult = tasks->SetTimingCallback(
    [](const Engine::ImageAsyncTaskTiming& timing) {
        // 只采集 monotonic 阶段数据；不要访问 UI、Texture 或 WindowSession。
    });
```

callback 在文件服务或 ImageAsync worker 线程执行；不要在 callback 中访问 UI 或
Texture。`ImageFileService::DecodeThumbnail` 优先使用可选 codec 的 bounded thumbnail
路径，不支持时才回退到 decode + `ResizeImage`；因此 `stage_file_read`、codec decode、
scale、owner drain 的 JSONL 分段数据必须和缓存状态、worker 数、输入集合及输出尺寸
一起比较，不能只凭首屏一个数字下结论。`emoexec_gallery_thumbnail_benchmark` 默认
使用临时 fixture；大图集只允许显式传入只读目录，用户真实 2 万+图库和人工 Inspector
视觉检查仍是 `Not Tested`，除非单独执行并记录。

Phase 11 之后仍保留 `ImagePreviewPipeline`、`ThumbnailService`、
`WicImagePreviewGenerator` 和旧 `PreviewPixels/DecodedImage` DTO，供
DesktopPersonalization、QuickSearch fallback、Inspector 以外的 Thumbnail/Preview
消费者和 legacy benchmark 使用。它们是高级/过渡接口，不应被新的 Gallery Inspector
路径引用；只有实际消费者清零、Debug/Release 回归和人工验证完成后，后续阶段才可删除
legacy worker pool。纯 Renderer/图片契约不因 WIC/COM/Win32 可选 backend 而被强制链接。

检索关键词：`InspectorPreviewAdapter selectionRevision generation stale dedup thumb size`
`ImageFileTiming ImageAsyncTaskTiming stage_file_read queueWait owner-thread legacy`
`async decode then owner-thread upload`

### Phase 12：Gallery Preview 单消费者与 Legacy 边界

Phase 12 后，`LibraryBrowserView` 不再注入 `ThumbnailService`。它的可见 tile
和选中项 Preview 都通过同一个 `ApplicationContext::Images()->Async()` 得到
XiE 任务服务；View 只保留 Gallery 的 cell、selection、library generation、
content key、revision、显示尺寸和产品纹理 cache。`ThumbnailService` 的
`Build/Snapshot/active/Shutdown` 兼容字段已经从 View、应用服务集合、
Bootstrap 和 MainWindowController 的生产组合路径移除；旧服务源文件仍保留给
独立旧测试、旧 benchmark、DesktopPersonalization 和 QuickSearch fallback。

选中项 Preview 的推荐顺序是：

```text
selection -> InspectorPreviewAdapter -> ImageAsyncService::Submit(Immediate)
          -> owner-thread DrainResults -> identity/stale check
          -> ImageTextureBridge -> product TextureCache -> Inspector widget
visible tile -> GalleryVisibleThumbnailAdapter -> Submit(Visible)
```

同一 `source/content/maxEdge` 只共享一个 XiE 任务；`maxEdge` 也必须进入产品
Preview identity，不能只用 `ImageId`。因此不同尺寸或 Preview variant 不会
误共用纹理。选中项使用 `Immediate`，Inspector/其他可见产品任务应低于当前
选中项时使用 `Visible` 或 `Background`；XiE 只提供通用三档优先级，不知道
Gallery、Inspector 或 Library 类型。当前 View 在 owner thread 先 drain 选中项
Preview，再 drain visible tile，减少选择到首帧的交互延迟。

选择 A→B→C、清空、隐藏、关闭、重新打开和 library generation 切换时，产品
adapter 取消尚未完成的 subscriber；running task 的晚到结果仍需检查 library、
generation、image、content、selection revision 和 `maxEdge`，不匹配时丢弃，
不能覆盖 C。worker 只创建并返回 owned `ImageBuffer`，不访问 UI、WindowSession
或 TextureRegistry；纹理上传/销毁仍只在 owner thread 进行。新纹理上传成功前
旧纹理保持 pinned，失败时继续显示旧纹理；Shutdown 后不允许新任务、drain 或
上传，所有 pending subscriber、CPU buffer 和 texture handle 都要回到稳定基线。

`selection churn`（工具参数为 `selection_churn`）是
`emoexec_gallery_thumbnail_benchmark` 的独立模式，和
`cold`、`hot`、`fast_scroll` 使用相同数据集、worker、队列、输出尺寸和 cache
预算，并输出 `preview_first_frame_us`、取消/stale、队列峰值、纹理/CPU 内存和
Shutdown 指标。它是自动对照，不是人工 Gallery 视觉验收；真实大图库和人工
滚动仍须单独记录为 `Not Tested` 或人工验证结果。

检索关键词：`LibraryBrowserView ThumbnailService boundary selection_churn
InspectorPreviewAdapter maxEdge Immediate Visible stale generation contentKey
owner-thread DrainResults Preview first frame legacy consumer`

### Phase 13：Gallery 真实环境资格与首帧诊断

Phase 13 只增加可复现的诊断观测，不改变 Gallery、ImageAsyncService 的 worker 数、
队列上限或产品 cache 策略。`ImageAsyncTaskTiming` 的四个 monotonic timestamp
（`submittedAt`、`workerStartedAt`、`fileStartedAt`、`fileFinishedAt`）是平台无关的
可选诊断字段；它们只说明 async worker/file service 的阶段，不代表 UI present 或
人工视觉帧。`ImageFileTiming::codecDecodeUs` 在 WIC 路径中包含 codec 内部的 COM/WIC
初始化与解码/转换时间；当前公共契约没有把 COM apartment 或 WIC 对象暴露出来，因而
报告中必须把“WIC/COM setup 单独分段”标为未单独仪器化，而不能猜测。

产品侧 benchmark 输出 `cold`、`warm worker` 观测（同一进程先让 worker 建立后再测）
和 `hot cache`，并额外记录 `submit_to_worker_us`、`submit_to_first_result_us`、
`stage_queue_wait`、`stage_file_*`、`stage_result_enqueue`、`stage_owner_drain`、
`texture_upload`、滚动帧 P50/P95/P99、Inspector 首帧 P50/P95/P99、工作集/私有字节、
线程/句柄峰值、纹理数量/估算字节、取消/stale/QueueFull 和 Shutdown。旧 preview
链路没有同样的 async timestamp contract，比较时必须保留“不可直接等价”的标记。

大规模边界使用显式参数而不扫描用户目录：

```powershell
.\emoexec_gallery_thumbnail_benchmark.exe --count 20000 --workers 2 `
  --queue 32 --thumb 96 --cache-mib 128 --output phase13-20000.jsonl
```

未指定 `--root` 时只生成少量混合格式 fixture 并扩展 20,000 条 metadata 记录；这是
合成 metadata scale，不是用户真实 20,000 张图片解码。真实 Gallery 必须在普通
Explorer/OpenGL 会话中由人工完成首次打开、冷/热 cache、鼠标滚动、A→B→C、Inspector、
隐藏/关闭/重开、DPI 与视觉检查。自动 adapter、fake provider、Software Renderer、
合成滚动、像素断言或截图都只能写成自动结果；缺少人工桌面时保持 `Not Tested` 或
`Environment Blocked`。

检索关键词：`ImageAsyncTaskTiming submittedAt workerStartedAt fileStartedAt
submit_to_worker_us submit_to_first_result_us scroll_frame_p99_us
preview_first_frame_p99_us 20000 metadata synthetic warm worker hot cache`

### Phase 14：桌面 Gallery qualification 与独立阶段对照

需要真实桌面人工操作时，使用独立的 qualification 进程，不把它混入正式发布
配置或普通 CTest：

```powershell
.\emoexec.exe --gallery-qualification
# 只有在用户明确授权只读图库后才追加：
.\emoexec.exe --gallery-qualification --qualification-root "<已确认的只读图库根目录>"
```

每次运行将已写出的 JSONL 放在项目 `artifacts/qualification` 下的独立 session
文件中；图片路径和内容 key 只记录稳定哈希。`--qualification-root` 只接受包含
`emoexec.db` 的目录，并在隔离 user-data registry 中注册为 read-only；未提供时
不代表真实图库通过。资格模式的关闭、重开、A→B→C、DPI、视觉和残留进程结果必须
由人工单独签收。诊断 JSONL 的 `xie_async_timing`、`xie_file_timing`、
`legacy_preview_timing` 与 `qualification_snapshot` 用于分析，不等于人工通过。

为了避免同一进程前一阶段污染后一阶段，benchmark 还支持单独进程运行：

```powershell
.\emoexec_gallery_thumbnail_benchmark.exe --pipeline xie_async_native `
  --stage cold --count 20000 --workers 2 --queue 32 --thumb 96 --cache-mib 128
.\emoexec_gallery_thumbnail_benchmark.exe --pipeline legacy_pipeline `
  --stage cold --count 20000 --workers 2 --queue 32 --thumb 96 --cache-mib 128
```

可替换 `cold` 为 `warm_worker`、`hot`、`fast_scroll` 或 `selection_churn`；两条
命令必须使用相同只读输入、输出尺寸、worker、队列和 cache 预算后才可比较。该
对照、OpenGL smoke、Software Renderer 和合成滚动均不能替代人工 Gallery 视觉
验收；未获得路径授权时真实图库保持 `Not Tested`。

检索关键词：`GalleryQualificationTrace gallery-*.jsonl gallery-qualification
qualification-root independent benchmark stage legacy_preview_timing`

只读 native clipboard smoke 需要显式 opt-in，且不会写入或清空用户剪贴板：

```powershell
$env:XIE_RUN_IMAGE_CLIPBOARD_SMOKE="1"
.\xie_image_native_clipboard_smoke.exe
```

输出为一行 JSON；未设置变量退出码为 `0` 且 `result=not_run`，`21` 表示当前剪贴板
没有图片（`Not Tested`），`20` 表示后端不可用，`23` 表示 clipboard busy，`24` 表示
payload 过大，其余读取/解码失败为 `22`。该工具是图片后端 smoke，不等同于 QuickSave
界面或真实保存流程；默认 CTest 使用 fake provider、内存 fixture 和 parser，不会修改
系统剪贴板。

检索关键词：`ImageServices Images ImageBuffer ImageDescriptor ImageView IImageCodec ImageEncodedPayload ImageMetadata ImageFileService ReadEncoded Inspect DecodeThumbnail ImageAsyncService ImageAsyncRequest Submit Cancel DrainResults ImageTaskState ImageLimits ResizeImage IImageClipboardSource ReadImage ImageCodec ImageClipboard xie_image_wic xie_image_clipboard_win32`

## 9. Interactive Overlay 原生 smoke

`xie_interactive_overlay_native_smoke` 是 Windows 下的显式 opt-in 验收程序。它不以
Explorer 是否存在作为透明窗口前置条件，而是直接创建真实 XiE Transparent Window，
检查真实 HWND 的 layered/tool/topmost 状态，发送受控的
窗口消息，再验证 `BindUI()`、输入路由、Hide/Show、Close 和 Shutdown。它不会重启或
终止 Explorer，也不会把 fake provider、内存 contract test 或静态检查计为人工桌面通过。

在交互式桌面会话中，从对应配置目录运行：

```powershell
$env:XIE_RUN_INTERACTIVE_OVERLAY_SMOKE="1"
.\xie_interactive_overlay_native_smoke.exe
```

输出为 JSONL；`result=not_run` 且退出码 `0` 表示未显式启用，`20` 表示
`Environment Blocked / Not Tested`（透明后端、窗口站或交互会话不可用），`30` 表示
BindUI/组合失败，`31` 表示真实窗口、消息或 UI 输入验收失败，`32` 表示清理失败。
Explorer/Shell 托盘验收是独立的 Desktop smoke：Explorer 缺失只能阻塞托盘测试，不能
阻塞本工具的透明窗口测试。程序只发送自有测试窗口的消息；这属于 synthetic native
message 验证，不等于物理鼠标。物理鼠标、中文 IME、真实视觉透明度和多显示器混合
DPI 仍需用户在普通桌面中单独操作并记录。

检索关键词：`xie_interactive_overlay_native_smoke XIE_RUN_INTERACTIVE_OVERLAY_SMOKE InteractiveOverlay native smoke Environment Blocked Not Tested`

## 10. 进入底层高级接口

高层入口不是 God Object，也不替代底层模块。需要特殊调度或平台宿主时，可以继续
直接使用：

* `ApplicationRuntime` / `ApplicationServices`：引擎生命周期和窗口 Runtime；
* `WindowRuntimeRegistry` / `WindowContext`：窗口运行时注册和帧状态；
* `IWindowSystem` / `IApplicationHost`：XiE 平台契约；
* `UIWindowRuntime` / `UI2DServices`：UI2D 的细粒度组合；
* `IRenderPipeline` 及 Renderer2D 后端契约：特殊渲染控制。

这些底层入口保持 XiE 内部模块化。新的跨平台公共应用接口只使用 XiE 类型；平台
实现、GLFW、Win32 Layered、OpenGL 和 D3D 差异留在 XiE 后端或宿主适配层。

`IApplicationWindowProvider`、`RegisterWindowProvider` 是高级宿主/测试替换点，
用于特殊后端或外部 Runtime 已经拥有窗口资源的场景；普通应用和 Settings 不应从
provider 桥接开始，而应使用 `InitializeBackends()`（需要延迟初始化时）和
`Windows().CreateStandard()`。高级 provider 不得把平台句柄或业务类型带入公共接口。

检索关键词：`ApplicationRuntime WindowRuntimeRegistry WindowContext IWindowSystem IApplicationHost UI2DServices Renderer2D`

## 当前边界提示

本索引不承诺 Media、图片仓库、Preview Pipeline、WorkerW、壁纸或系统鼠标业务。
Desktop Services 的通用能力已由独立可选模块提供；QuickSave 的正式透明壳已使用
`ApplicationContext -> CreateTransparent -> WindowSession -> BindUI`，但 QuickSave
草稿、图片捕获、命名、标签、保存和 Repository 仍属于应用业务层。QuickSearch、Gallery、
Main、Preview、Library、Tag 等产品菜单和功能映射仍属于项目业务层，不能加入 XiE
Application 公共 API；其中未迁移窗口仍可由高级宿主保留的 WindowHost/backend 路径驱动。
