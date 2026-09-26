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
| [adr-0001-l1-ownership.md](adr-0001-l1-ownership.md) | ADR：L1 通用访问原语/生命周期钩子归薄 fork；环境专用原生适配/初始化目标/证据判等/IPC/窗口政策归 env。新架构决定，不追改历史 |

## 状态速览（2026-09-26）

| 项 | 值 |
|---|---|
| 上游 | [`vector-wlc/AsmVsZombies`](https://github.com/vector-wlc/AsmVsZombies)，默认分支 `master` |
| 固定提交 | `c42676c269b5b482a1eb9203a5b979e9d8a2a5c7`（`feat: release 2.9.2`，2026-06-26） |
| 本仓 `master` | 与上游 `master` 逐提交相同（`c42676c`，compare: identical，ahead 0 / behind 0） |
| 本仓 `lvz/l1-determinism-primitives` | `e266e18aa447b2732113ff994aa81f32b70d2214`（2 个提交，基线 `c42676c`）；**fork 已建，未合并进 master、无任何消费方采用** |
| 旧仓采用状态 | `guajun/llm-vs-zombies` `main` @ `389803f10bef8d1b2abdd7b1cee0b9328ac26332`：仍是 pristine 子模块 @ `c42676c` + 构建期 overlay；**未切换到 fork 分支** |
| 上游 PR / issue | 本组织未创建：`pvz-agent-lab/avz` 与 `guajun/AsmVsZombies` 无任何 PR；上游仓库无来自本组织的 PR/issue（上游自身有一个无关的 open PR #5，非本仓内容） |

## 图例

- 现状分类：`fork 已建`（分支存在，未合并/未采用）、`主仓采用`（旧仓构建期 overlay 已生效）、`上游 PR`（已向上游创建 PR；当前为 0 项）。
- 验证标注：`离线`（不运行游戏，可在 CI/本机复跑）、`真机`（运行原版游戏）、`未验证`（证据缺失）、`缺`（尚未建立的验证）。
- 链接约定：所有源码引用使用完整 SHA 永久链接；子模块 pin、守卫哈希与命令证据见 [upstream-and-pins.md](upstream-and-pins.md)。

## 维护提示

上游 `.gitignore` 忽略 `/docs/`。保持上游 `.gitignore` 原文是有意决定，本轮以 `git add -f` 强制跟踪本目录；新增 `docs/lvz/` 下的文件也需要 `git add -f`。是否改为 `.gitignore` 例外（`/docs/*` + `!/docs/lvz/`）留给 avz#1 决定，见 [upstream-and-pins.md](upstream-and-pins.md) §3、§8 第 8 步。
