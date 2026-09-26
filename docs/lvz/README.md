# LVZ 薄 fork 治理

本目录是 `pvz-agent-lab/avz` 的组织治理入口，落实：

- [pvz-agent-lab/avz#1](https://github.com/pvz-agent-lab/avz/issues/1)（薄 fork 治理：接续既有 AvZ 补丁、上游差异表与版本锁定）
- 旧仓 [guajun/llm-vs-zombies#101](https://github.com/guajun/llm-vs-zombies/issues/101) 中关于 [#72](https://github.com/guajun/llm-vs-zombies/issues/72) 归属项的整改要求
- 架构背景 [guajun/llm-vs-zombies#72](https://github.com/guajun/llm-vs-zombies/issues/72)

本轮只交付治理文档：不迁移补丁代码、不切换任何采用版本、不运行游戏、不发布上游 PR，也不把未经运行的自检写成等价结论。

| 文档 | 内容 |
|---|---|
| [upstream-and-pins.md](upstream-and-pins.md) | 上游来源、固定 SHA、许可与 attribution、当前采用状态、升级/等价校验/回退/同步约定、未来步骤 |
| [patch-registry.md](patch-registry.md) | 逐项差异表：`lvz/l1-determinism-primitives`（F1–F5）与旧仓 `runtime/avz_overlay.cmake`（O1–O11），含理由、真实位置、归属、已有/缺失验证、上游提交状态 |
| [adr-0001-l1-ownership.md](adr-0001-l1-ownership.md) | ADR（**Proposed，合并后生效**）：L1 通用访问原语/生命周期钩子归薄 fork；游戏专属原生采集/审计与窗口/IPC 政策归 env；公共轨迹合同/身份/封存校验/只读加载归 core；编排与策略归 rollout。遵循旧仓 #102 的四仓分工，不追改历史 |

## 状态速览（2026-09-26）

| 项 | 值 |
|---|---|
| 上游 | [`vector-wlc/AsmVsZombies`](https://github.com/vector-wlc/AsmVsZombies)，默认分支 `master` |
| 固定提交 | `c42676c269b5b482a1eb9203a5b979e9d8a2a5c7`（`feat: release 2.9.2`，2026-06-26） |
| 本仓 `master` | 与上游 `master` 逐提交相同（`c42676c`，compare: identical，ahead 0 / behind 0） |
| 本仓 `lvz/l1-determinism-primitives` | `e266e18aa447b2732113ff994aa81f32b70d2214`（2 个提交，基线 `c42676c`）；**fork 已建，未合并进 master；已审查消费方（旧仓 main、org 三个仓）未采用，检查范围见 [upstream-and-pins.md](upstream-and-pins.md) §4** |
| 旧仓采用状态 | `guajun/llm-vs-zombies` `main` @ `389803f10bef8d1b2abdd7b1cee0b9328ac26332`：仍是 pristine 子模块 @ `c42676c` + 构建期 overlay；**未切换到 fork 分支** |
| 上游 PR / issue | 本组织未创建上游 PR：相关补丁（F1–F5、O1–O11）均未提交（`LVZ-FORK-NOTES.md` @ `e266e18` 记录未开 PR；2026-09-26 检索 `vector-wlc/AsmVsZombies` 公开 PR/issue 未发现来自本组织的条目，检索范围仅该仓库公开列表） |

## 图例

- 现状分类：`fork 已建`（分支存在，未合并/未采用）、`主仓采用`（旧仓构建期 overlay 已生效）、`上游 PR`（已向上游创建 PR；本表相关补丁当前为 0 项）。
- 验证标注：`离线`（不运行游戏，可在 CI/本机复跑）、`真机`（运行原版游戏）、`未验证`（证据缺失）、`缺`（尚未建立的验证）。
- 链接约定：所有源码引用使用完整 SHA 永久链接；子模块 pin、守卫哈希与命令证据见 [upstream-and-pins.md](upstream-and-pins.md)。

## 文档口径

- **当前事实**：各仓固定 SHA、文件状态、现有采用方式——可由永久链接直接核对（[upstream-and-pins.md](upstream-and-pins.md) §1–§4）。
- **提案**：ADR（Proposed）与升级/采用/回退程序（§5–§6）——合并前不生效，本轮未执行。
- **历史证据**：既往离线/真机运行与其报告；标“本轮未重跑”不等于“历史未做”，未被重跑也不视为未发生。
- **链接检查边界**：永久链接存在与行号有效只说明引用可复查，不是语义验证；语义结论以被引用报告/运行/提交自身为准（详见 [upstream-and-pins.md](upstream-and-pins.md) §9）。

## 维护提示

上游 `.gitignore` 忽略 `/docs/`。保持上游 `.gitignore` 原文是有意决定，本轮以 `git add -f` 强制跟踪本目录；新增 `docs/lvz/` 下的文件也需要 `git add -f`。是否改为 `.gitignore` 例外（`/docs/*` + `!/docs/lvz/`）留给 avz#1 决定，见 [upstream-and-pins.md](upstream-and-pins.md) §3、§8 第 8 步。
