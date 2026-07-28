# 模块依赖规则

## CMake 目标

| 目标 | 形态 | 直接依赖 |
|---|---|---|
| `xie_foundation` | Interface | 无 |
| `xie_platform_contracts` | Interface | Foundation |
| `xie_input` | Static | Foundation、Platform Contracts（仅强类型 WindowHandle） |
| `xie_renderer2d_contracts` | Static | Foundation |
| `xie_renderer2d_resources` | Static | Renderer2D Contracts |
| `xie_runtime2d` | Static | Foundation |
| `xie_ui2d` | Static | Foundation、Input、Renderer2D Contracts |
| `xie_application` | Static | Foundation、Platform Contracts、Renderer2D Contracts |
| `xie_renderer2d_opengl` | Static | Contracts、Resources、Platform Contracts |
| `xie_renderer2d_software` | Static | Contracts、Resources、Platform Contracts |
| `xie_platform_glfw` / `xie_platform_win32` | Static | Platform Contracts；私有依赖 Input 以产生通用事件；对应系统库 |

`xie_engine` 暂时链接以上目标并承载旧 Core、Renderer、Assets 与 Story。旧 `Core/Application` 仍直接选择 GLFW 具体实现，这是项目装配迁出前的兼容债务，但平台实现本身已经是独立目标。

## 允许的 include

- Foundation 不 include 其他 Engine 模块。
- Platform Contracts 可 include Foundation。
- Input 可 include Foundation 与 `WindowHandle` contract。
- Renderer2D Contracts 可 include Foundation。
- Runtime2D 只 include Foundation/自身类型。
- UI2D 只 include Foundation、Input、Renderer2D Contracts 和自身头文件。
- 平台实现可私有 include Input，将 GLFW callback / Win32 message 转成通用事件，但 Contracts 不反向依赖 Input。
- 渲染后端可 include 对应 Contracts/Resources 与私有平台接口。
- Projects 可组合所有需要的实现目标。

## 禁止方向

```text
Foundation → UI2D
Renderer2D Contracts/Resources → UI2D
Platform Contracts/implementations → UI2D
Runtime2D → UI2D
UI2D → Runtime2D / GLFW / Win32 / OpenGL / Software backend / SQLite / Projects
Engine → EmoExec business
```

公共 Platform Contracts 禁止包含 `windows.h` 或 GLFW 头。UI2D contract test 会递归扫描公共 UI2D 头，拒绝 Win32、GLFW、GLAD 和具体 Renderer backend include。

## 兼容头与类型

`WindowHandle`、`TextureHandle`、`FontHandle`、`TextLayoutHandle`、`UINodeHandle` 共享 `GenerationalHandle<Tag>` 实现，但 Tag 不同，不能隐式互换。`Vec2F`、`RectF`、`InsetsF`、`Color4f` 和 `AlphaMode` 以 Foundation 为唯一事实来源；旧 include 路径继续转发这些类型。
