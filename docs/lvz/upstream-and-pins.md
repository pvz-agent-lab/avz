# 上游来源、版本 pin 与同步约定

适用范围：`pvz-agent-lab/avz` 及其消费方（`pvz-agent-lab/pvz-env`、`guajun/llm-vs-zombies` 等）。
本文件只记录治理事实与流程，不改变任何 pin，也不代表任何迁移已经执行。

固定基线（全文使用完整 SHA 永久链接）：

- 上游 `vector-wlc/AsmVsZombies@c42676c269b5b482a1eb9203a5b979e9d8a2a5c7`
- 薄 fork 分支 `pvz-agent-lab/avz@e266e18aa447b2732113ff994aa81f32b70d2214`（`lvz/l1-determinism-primitives`）
- 旧仓现状 `guajun/llm-vs-zombies@389803f10bef8d1b2abdd7b1cee0b9328ac26332`（`main`）

## 1. 上游来源

| 项 | 值 | 核对方式（2026-09-26） |
|---|---|---|
| 上游仓库 | <https://github.com/vector-wlc/AsmVsZombies> | 非 fork |
| 默认分支 | `master` | GitHub API |
| 固定提交 | `c42676c269b5b482a1eb9203a5b979e9d8a2a5c7`，`feat: release 2.9.2`，2026-06-26 | 提交存在于上游；上游 `master` 顶端即该提交 |
| 许可 | GPL-3.0 | 上游 [LICENSE](https://github.com/vector-wlc/AsmVsZombies/blob/c42676c269b5b482a1eb9203a5b979e9d8a2a5c7/LICENSE) |
| 本仓默认分支 | `master` = 上游 `master` | compare API：`identical`，ahead 0 / behind 0 |

组织内的 fork 链：`pvz-agent-lab/avz`（GitHub 元数据：`fork=true`，`parent=guajun/AsmVsZombies`，`source=vector-wlc/AsmVsZombies`）。

| 仓库 | 分支 | SHA | 状态 |
|---|---|---|---|
| `vector-wlc/AsmVsZombies` | `master` | `c42676c` | 上游，release 2.9.2 |
| `guajun/AsmVsZombies` | `lvz/l1-determinism-primitives` | `e266e18` | 个人 fork 上的原始开发分支 |
| `pvz-agent-lab/avz` | `master` | `c42676c` | 与上游相同；本治理文档 PR 合并后仅多 docs |
| `pvz-agent-lab/avz` | `lvz/l1-determinism-primitives` | `e266e18` | fork 已建；未合并、未采用、无 PR |

`e266e18` 的两个提交（基于 `c42676c`）：`37e1eea` `feat(rng): 增加游戏随机数状态与 App 时钟的读写原语`、`e266e18` `docs(fork): LVZ 差异表与离线自检`。分支没有触碰 `runtime/avz_overlay.cmake` 的任一守卫文件（见 §4）。

## 2. 旧仓实际采用的 pristine 来源

旧仓（主仓）没有采用 `lvz/l1` 分支，采用的就是上游 pristine 源码：

- 子模块路径 `avz/framework`，URL `https://github.com/vector-wlc/AsmVsZombies.git`，pin `c42676c269b5b482a1eb9203a5b979e9d8a2a5c7`（`git ls-tree 389803f avz` / `git submodule status`）。
- [`dependencies.lock.json`](https://github.com/guajun/llm-vs-zombies/blob/389803f10bef8d1b2abdd7b1cee0b9328ac26332/dependencies.lock.json) 记录：
  - `avz_repository`: `https://github.com/vector-wlc/AsmVsZombies`
  - `avz_commit`: `c42676c269b5b482a1eb9203a5b979e9d8a2a5c7`
  - `avz_runtime_release`: `2.9.2_2026_06_26`
- 构建期补丁在 [`runtime/avz_overlay.cmake`](https://github.com/guajun/llm-vs-zombies/blob/389803f10bef8d1b2abdd7b1cee0b9328ac26332/runtime/avz_overlay.cmake)：读入 pristine 源文件、归一化换行、比对 SHA-256、做字符串替换，把生成文件写进构建目录；磁盘上的子模块保持原样。该文件在审查基线 `c4d7e73` 与当前 `main`（`389803f`）之间逐字节相同（blob `a2b8ec4ce0e89bd10509109d48e0c976e40bf376`）。

### 守卫哈希（LF 归一化后的 SHA-256）

| 上游文件 | 哈希 | 守卫位置 |
|---|---|---|
| `src/avz_script.cpp` | `9457a13e056aa785d8efb5de8616a19c04107b75d60c9607eea9b778d8134d7b` | overlay L19–L24 |
| `src/avz_hook.cpp` | `2376e147e671b6ef8dbbae33811b654f92e29c7f18c4d9989d394cd4c5537134` | 同上 |
| `src/avz_card.cpp` | `d377619432d1c1a5bd060d851a3be4f46bc0768094bb2a401fbfcc67a84ff1d0` | 同上 |
| `src/avz_coroutine.cpp` | `8840adfd08b113828b4485a6e89120d96c77e1cb96ac68127860198b4b025cfb` | 同上 |
| `src/avz_smart.cpp` | `d863a802196acc25e026c8bbcf2aa78c97d1d0686fb1ee03a977a233c72bc0fe` | overlay L85–L87 |
| `src/avz_cob_manager.cpp` | `f985ac3366d815a3cdd9268a09e8a6d7134f396c62ab68cd130f440ee8185a21` | overlay L109–L111 |

任何一项不符，CMake 直接 `FATAL_ERROR`，构建失败。哈希守卫是「固定来源 + 永久可复查」的机器化部分；补丁锚点与替换语义登记在 [patch-registry.md](patch-registry.md)。

## 3. 许可与 attribution

- 上游与本 org 仓均为 GPL-3.0；本 org 仓 `master` 当前就是上游 `master`，`README.md` 与 `LICENSE` 是上游原文。
- fork 分支 `e266e18` 的 6 个差异文件是 `inc/avz_rng.h`、`src/avz_rng.cpp`、`inc/avz_pvz_struct.h`、`inc/libavz.h`、`tests/avz_fork_selfcheck.cpp`、`LVZ-FORK-NOTES.md`；没有删除或修改上游 `LICENSE`、上游致谢与作者署名。
- 旧仓 overlay 是构建期生成文件，子模块磁盘内容不变，不产生新的许可或署名改动；GPL-3.0 继承在旧仓迁移质量门槛中已声明。
- fork 提交作者为 `gua_jun <whgls323232@gmail.com>`；上游作者与致谢列表保持原样。
- 本治理 PR 只新增 `docs/lvz/` 并在上游 `README.md` 顶部加一条指向它们的说明；不改 `LICENSE`，不改 game/资源，不提交游戏二进制或凭据。
- 上游 `.gitignore` 忽略 `/docs/`；为保持上游 `.gitignore` 原文，`docs/lvz/` 以 `git add -f` 强制跟踪。新增本目录文件需要 `git add -f`；是否改成 `.gitignore` 例外留给 avz#1 决定（§8 第 8 步）。

## 4. 当前采用状态（明确）

| 事项 | 状态 |
|---|---|
| 旧仓 AvZ 源码 | **pristine 子模块 @ `c42676c`**（未采用 fork 分支） |
| 旧仓构建 | **pristine + 构建期 overlay**（11 处补丁点，见 registry）；默认构建与托管构建共用 script/hook/card/coroutine/smart overlay，cob overlay 仅在托管构建替换 |
| fork 分支消费方 | **无**；旧仓 `determinism/` 仍用自己的 `MtState`、`0x75a910`、`_getptd` 逻辑，尚未改用 `A*` 原语 |
| 上游提交 | **无 PR、无 draft**；`pvz-agent-lab/avz` 与 `guajun/AsmVsZombies` 无任何 PR，上游无来自本组织的 PR/issue（上游自身有一个无关的 open PR，2026-09-26） |
| 实验身份引用 | 全部使用固定 SHA / 锁文件 / DLL 哈希；无浮动分支作为实验身份 |

换句话说：fork 已建（F1–F5），主仓采用（O1–O11）是两件不同的事，二者没有发生合并或替换。

## 5. 升级流程（提案；本轮未执行）

1. 候选提交必须是上游 `master` 上已存在的固定 SHA；禁止以分支名/`HEAD` 作为实验或构建身份。
2. 对照 overlay：逐条复核补丁锚点与替换语义；用新 pristine 文件重算 6 项守卫哈希。脚本 `FATAL_ERROR` 只保证"没悄悄变"，语义复核仍必须人工完成。
3. 运行旧仓离线测试（不运行游戏）：`avz_hosted_script`、`avz_hosted_fire` + `avz_hosted_fire_default`、`hosted_crash_probe`、runtime/determinism 相关测试。
4. L1 原语变更另做等价校验：对照 `determinism/evidence.json` 的签名/布局证据与 fork 自检（`tests/avz_fork_selfcheck.cpp`）；任一未复跑项必须写"未验证"，不得外推为通过。
5. 需要真机门槛的变更（帧推进、加载、RNG、窗口/卡槽路径）必须附真机验收报告后才宣告等价；离线通过、真机通过、未验证三者分开写。
6. 通过后一次提交更新：子模块 pin、`dependencies.lock.json`（`avz_commit` / `avz_runtime_release`）、overlay 守卫常量、本目录文档与 [patch-registry.md](patch-registry.md)，并在旧仓迁移 issue 附绑定提交的报告。

## 6. 回退

- 回退 = 恢复上一版 `avz_commit` 与 lock 字段、恢复对应 overlay 守卫哈希与锚点，禁止关闭或绕过守卫。
- 回退必须与升级一样留下报告：触发了什么、回退到哪个 SHA、哪些离线/真机证据基于哪一版。
- fork 分支上的补丁（F1/F2）不随 pin 回退自动消失；消费方没有采用它们，因此 pin 回退不影响旧仓。

## 7. 同步约定（fork）

- `lvz/*` 分支以 `master` 为基线 rebase，保持"新增文件 + 少量小改"；不把实验策略、证据格式或运行时代码带进 fork。
- 每项 fork 差异必须同时登记在 `LVZ-FORK-NOTES.md` 与 [patch-registry.md](patch-registry.md)，注明"是否已提上游"；未知就写未知。
- 上游接受某项补丁后，fork 删除对应差异并把 registry 状态改为"上游已合并"；不保留双份实现。
- 上游不接受但环境仍需要的通用原语，保留在 fork 并在 registry 说明理由；环境专用逻辑一律留在 env，不因"改了上游文件"混入 fork。

## 8. 未来步骤（当前状态与责任）

| # | 步骤 | 状态 |
|---|---|---|
| 1 | 本治理 PR 合并（docs only，Refs avz#1、llm-vs-zombies#101） | 本次提交 |
| 2 | 明确组织内默认维护分支与 `lvz/l1` 的后续角色（继续 fork 分支 / rebase 进 org master） | 未开始（avz#1） |
| 3 | 在独立环境/CI 复跑 `tests/avz_fork_selfcheck.cpp`，把带镜像的 57 项结果与负控附到 avz#1（本轮只有分支自带记录） | 未开始 |
| 4 | env 接入：bump 子模块 URL/pin 到 fork 分支，`determinism/` 改用 `A*` 原语，做离线等价校验并记录（`pvz-env`） | 未开始 |
| 5 | 上游候选整理：F1/F2 在完整字段清单确定后评估一次性提交；O9（协程双重恢复修复）单独评估是否属上游 bugfix | 未开始 |
| 6 | 更新 `LVZ-FORK-NOTES.md` 中关于 overlay "只守护四个文件 / 4 处 string(REPLACE)" 的过时描述（该文写于 coroutine/cob overlay 之前） | 未开始 |
| 7 | 把 `determinism/` 的 L1 地址知识逐步收敛到 fork，消除重复（见 registry §E） | 未开始 |
| 8 | 决定 `docs/` 跟踪政策：维持上游 `.gitignore` + `git add -f`（现状），或加 `/docs/*` + `!/docs/lvz/` 例外 | 未开始 |

以上均不阻塞本次文档 PR，也不代表任何代码迁移已经发生。
