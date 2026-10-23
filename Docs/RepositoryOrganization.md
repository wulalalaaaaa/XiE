# 仓库目录规范

本规范约定 XiE 主仓库的源码、文档、依赖和本地产物放置位置。独立项目的 Git 仓库仍由各自仓库维护。

## 源码与依赖

- `Engine/`：XiE 引擎模块。
- `Runtime/`：运行时入口和宿主能力。
- `XiE_Projects/<项目名>/`：XiE 仓库内维护的应用、示例和测试项目。此目录中的源码、CMake 文件和测试不能通过 `.gitignore` 整体屏蔽。
- `XiE_Projects/EmoExec/EmoExec/`：EmoExec 的独立本地克隆，属于单独的 GitHub 仓库，不是 XiE 子模块，也不由 XiE 跟踪。EmoExec 的源码、测试和项目文档在 EmoExec 仓库中单独提交和推送。
- `ThirdLibraries/`：第三方依赖。使用 Git 子模块的依赖记录在 `.gitmodules`；CMake 下载或生成的依赖放在 `Engine/External/`。
- 克隆 XiE 后，若需要 EmoExec，可将其独立克隆到 `XiE_Projects/EmoExec/EmoExec/`。没有该克隆时，XiE 的 CMake 配置会跳过 EmoExec 项目。
- `git submodule update --init --recursive` 仅用于初始化 XiE 实际登记的第三方子模块，例如 spdlog。

## 文档

- `Docs/Architecture/`：模块边界、运行时和架构说明。
- `Docs/Planning/`：仍在维护的路线分析和项目交接资料。
- `Docs/DeliveryReports/`：XiE 引擎功能的交付报告和验证指南。
- `Docs/` 根目录：规范、索引和跨模块说明。
- EmoExec 专属报告放入其独立克隆的 `Docs/`，并由 EmoExec 仓库维护。
- 个人代理上下文和编辑器规则放在 `.codex/`、`.agents/` 或 `.cursor/`，不作为产品文档提交。

## 构建输出与验证产物

- `build*/`、`cmake-build*/`、`out/`、`vs_build*/` 和 `Binaries/` 只放本地构建输出。
- `artifacts/<项目>/<阶段或用途>/` 放本地截图、日志、数据库、性能采样和打包暂存文件；这些目录整体由 `.gitignore` 排除。EmoExec 的验证产物放在 EmoExec 自己的 `artifacts/` 下，由独立仓库的忽略规则管理。
- 被自动化测试或文档长期依赖的输入样例放在对应项目的 `Tests/Fixtures/`，作为源码资产检查和提交。
- 不把截图、数据库、日志或构建目录散落在仓库根目录。

## `.gitignore` 原则

只忽略可再生成的本地状态、构建输出、缓存和验证产物。不要用目录白名单屏蔽 `XiE_Projects/` 或 `Docs/`；新增源码和正式文档应默认进入 Git 的可见范围。独立仓库通过子模块建立明确边界。
