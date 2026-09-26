# 补丁差异表：`lvz/l1` 分支与旧仓 overlay

本表逐项登记两处现有差异，并把仍留在 env 的 L1 知识单独列出。归属判据见 [ADR-0001](adr-0001-l1-ownership.md)（Proposed，合并后生效）；pin、守卫哈希与升级流程见 [upstream-and-pins.md](upstream-and-pins.md)；链接可复查不等于语义验证，边界见该文 §9。

固定基线（完整 SHA 永久链接）：

- 上游：`vector-wlc/AsmVsZombies@c42676c269b5b482a1eb9203a5b979e9d8a2a5c7`
- 薄 fork 分支：`pvz-agent-lab/avz@e266e18aa447b2732113ff994aa81f32b70d2214`（`lvz/l1-determinism-primitives`，2 提交，基线 `c42676c`）
- 旧仓（主仓）：`guajun/llm-vs-zombies@389803f10bef8d1b2abdd7b1cee0b9328ac26332`（`main`）；overlay 与审查基线 `c4d7e73` 逐字节相同（blob `a2b8ec4ce0e89bd10509109d48e0c976e40bf376`）

图例：

- 归属：`fork` = 薄 fork（pvz-agent-lab/avz）；`env` = 环境仓（现位于 llm-vs-zombies，迁移后归 pvz-env）；`待评估` = 候选上游/待定。
- 现状分类：`fork 已建`（分支存在，未合并/未采用）、`主仓采用`（构建期 overlay 已在旧仓生效）、`上游 PR`（本表相关补丁当前为 0 项）。
- 验证：`离线`、`真机`、`缺`（尚无验证）、`未验证`；分支自带结果会明确标注"未在本治理任务复跑"。

## 0. 概览

| ID | 内容 | 真实位置 | 归属 | 现状 | 上游提交 |
|---|---|---|---|---|---|
| F1 | 全局 MT19937 / 引擎 CRT rand 状态读写原语、地址常量、布局自检 | `inc/avz_rng.h`、`src/avz_rng.cpp`（fork 新增） | fork | fork 已建 | 否 |
| F2 | `AppUpdateCount()`、`WriteMjClock()`、`WriteAppUpdateCount()`（返回读回值） | `inc/avz_pvz_struct.h` L172–L188（fork 修改） | fork | fork 已建 | 否 |
| F3 | 总头文件包含 `avz_rng.h` | `inc/libavz.h` L20 | fork | fork 已建 | 否 |
| F4 | 离线自检（常量、布局、往返、可选镜像字节核对） | `tests/avz_fork_selfcheck.cpp`（fork 新增） | fork | fork 已建 | 不计划 |
| F5 | fork 差异表与说明 | `LVZ-FORK-NOTES.md`（fork 新增） | fork | fork 已建 | 不计划 |
| O1 | AvZ logger 替换为 runtime 诊断 logger | overlay 对 `avz_script.cpp` L8 的替换 | env | 主仓采用 | 否 |
| O2 | `catch std exception` 弹窗改为记日志 | overlay 对 `avz_script.cpp` L182 的替换 | env | 主仓采用 | 否 |
| O3 | `unknown exception` 弹窗改为记日志 | overlay 对 `avz_script.cpp` L185 的替换 | env | 主仓采用 | 否 |
| O4 | 常驻 runtime 下不走 AvZ 递归生命周期循环 | overlay 对 `avz_script.cpp` L45–L47 的替换 | env | 主仓采用 | 否 |
| O5 | 帧门：`BeforeFrame` / `AfterAvzRunTotal` | overlay 对 `avz_script.cpp` L193–L194 的替换 | env | 主仓采用 | 否 |
| O6 | 由 controller 授予并计量一次引擎更新 | overlay 对 `avz_script.cpp` L198–L199 的替换 | env | 主仓采用 | 否 |
| O7 | 装 hook 前校验目标镜像身份 | overlay 对 `avz_hook.cpp` L26 的插入 | env | 主仓采用 | 否 |
| O8 | runtime 常驻时跳过 AvZ 自动选卡/等待战斗 | overlay 对 `avz_card.cpp` L222 的替换 | env | 主仓采用 | 否 |
| O9 | 修复 `__AWait::await_suspend()` 双重恢复（#88） | overlay 对 `avz_coroutine.cpp` L50–L51 的替换 | 待评估 | 主仓采用 | 否（上游候选） |
| O10 | 环境辅助收集写入审计流 | overlay 对 `avz_smart.cpp` L62 的插入 | env | 主仓采用 | 否 |
| O11 | 托管炮击写入审计流（#84 open item 2） | overlay 对 `avz_cob_manager.cpp` L48 的插入（仅托管构建） | env | 主仓采用 | 否 |

全局上游状态：所有条目的"上游提交"均为否——`LVZ-FORK-NOTES.md`（`e266e18`）记录未向上游开 PR；2026-09-26 检索 `vector-wlc/AsmVsZombies` 公开 PR/issue 列表，未发现来自本组织的条目。该检索只覆盖该仓库公开列表，不外推其他渠道；本仓存在治理文档 PR #2 不影响该结论，也不存在任何"上游已接受/已合并"的结论。

---

## A. 薄 fork 分支差异（`lvz/l1-determinism-primitives`）

分支 tip `e266e18`；相对上游 `c42676c` 共 6 个文件、+646/−1（`git diff --stat`）。

### F1 游戏 RNG 状态读写原语

- 位置：[`inc/avz_rng.h`](https://github.com/pvz-agent-lab/avz/blob/e266e18aa447b2732113ff994aa81f32b70d2214/inc/avz_rng.h)（新，93 行）、[`src/avz_rng.cpp`](https://github.com/pvz-agent-lab/avz/blob/e266e18aa447b2732113ff994aa81f32b70d2214/src/avz_rng.cpp)（新，104 行）。
- 内容：全局 MT19937 实例 `0x75a910` 的 624 个状态字 + 游标 `+0x9c0` 读写（`AGetMTRand` / `AWriteMTRand`，`AMTRand::INSTANCE/CURSOR_OFFSET/WORD_COUNT`）；引擎静态 CRT `rand`/`srand` 的每线程状态（`AEngineCrtRand::GETPTD = 0x628a3d`，`STATE_OFFSET = 0x14`，`AGetEngineCrtRand` / `AWriteEngineCrtRand`）；MT 构造函数/跳板/全局初始化器/4 个构造点常量（`0x5a98d0`、`0x5a9890`、`0x5a98a0`、`0x5a9930`、`0x40a7ea/0x4256b7/0x426a42/0x484173`）；`ARel32CallTarget()` 与 `AIsRngLayoutValid()` 布局自检。
- 理由：引擎随机流不随存档往返；"怎么读写这个游戏的某个量"属通用访问原语（#72 原则、ADR-0001）。上游 `c42676c` 源码树（`.h`/`.cpp`）检索 `MTRand` / `0x75a910` / `_getptd` 无命中（本治理任务复核）。
- 归属：fork（通用访问原语 + 版本适配常量）。
- 现状：fork 已建；未合并进 org `master`，已审查消费方未采用（范围见 [upstream-and-pins.md](upstream-and-pins.md) §4）。
- 已有验证（历史记录）：`LVZ-FORK-NOTES.md`（`e266e18`）记录用 llvm-mingw i686 构建 26 个 TU、链接 `libavz.a` 无告警；`tests/avz_fork_selfcheck.cpp` 对目标镜像报 57 checks / 0 failures（含 26 项镜像字节核对），不带镜像 31 项；负控：换包装器镜像 24 failures、两处位移写错能报出对应 FAIL。**本治理任务未重跑**（不是"历史未做"）。
- 缺失验证：该分支的真机运行记录（自述未做）；旧仓 `determinism/` 与 fork 原语之间没有交叉等价测试；上述离线结果尚无 CI 复跑。
- 上游提交：否（`LVZ-FORK-NOTES.md` 记载按 #72 安排等"完整字段清单"后一次性提；未创建 PR 或 draft）。

### F2 App 时钟/更新计数写原语

- 位置：[`inc/avz_pvz_struct.h`](https://github.com/pvz-agent-lab/avz/blob/e266e18aa447b2732113ff994aa81f32b70d2214/inc/avz_pvz_struct.h#L172-L188)（修改）。
- 内容：新增 `AppUpdateCount()`（`LawnApp + 0x484`，候选反编译 `mUpdateCount`）、`WriteMjClock(int)` / `WriteAppUpdateCount(int)`：写入后返回实际读回值，调用方不能假设写入生效。
- 理由：`MjClock()` 已存在但 App 更新计数缺失；两个计数可能在建局前被对齐，读写原语属通用访问原语。反汇编依据：写点 `0x4526e6`（mj clock）、`0x44ec2a`（构造函数置零）、读点 `0x54ec60`。
- 归属：fork（通用访问原语）。
- 现状：fork 已建；已审查消费方未采用。旧仓 B(0) 归一化仍用 env 自己的地址与写入路径（[`docs/b0-normalization-native.md`](https://github.com/guajun/llm-vs-zombies/blob/389803f10bef8d1b2abdd7b1cee0b9328ac26332/docs/b0-normalization-native.md)：字段 `/sound_effects/app_update_count`、`/app/mj_clock`，目标/回执/fail-closed 属 env）。
- 已有验证（历史记录）：同 F1 的离线自检覆盖布局/常量/镜像字节；本治理任务未重跑。
- 缺失验证：真机写入读回；与旧仓 B(0) 路径的等价对比。
- 上游提交：否。

### F3 总头文件包含

- 位置：[`inc/libavz.h`](https://github.com/pvz-agent-lab/avz/blob/e266e18aa447b2732113ff994aa81f32b70d2214/inc/libavz.h#L20)（+1 行）。
- 理由：让 `avz_rng.h` 随总头文件可用。
- 归属：fork。现状：fork 已建；已审查消费方未采用。
- 已有验证：无独立验证（单行 include，随 F1 的构建/自检一起编译）。缺失：无。
- 上游提交：否。

### F4 fork 离线自检

- 位置：[`tests/avz_fork_selfcheck.cpp`](https://github.com/pvz-agent-lab/avz/blob/e266e18aa447b2732113ff994aa81f32b70d2214/tests/avz_fork_selfcheck.cpp)（新，308 行）。
- 理由：这些地址只对 1.0.0.1051 英文版成立，常量必须有出处、可复核；分支说明明确该文件属 fork 验证材料、不打算混进上游补丁。
- 已有验证（历史记录）：见 F1（`e266e18` 分支记录）。
- 缺失验证：本治理任务未复跑（非历史未做）；无 CI 接线。
- 上游提交：不计划（维护材料）。

### F5 fork 说明文档

- 位置：[`LVZ-FORK-NOTES.md`](https://github.com/pvz-agent-lab/avz/blob/e266e18aa447b2732113ff994aa81f32b70d2214/LVZ-FORK-NOTES.md)（新，116 行）。
- 内容：基线、归属原则、F1–F4 差异表、地址证据、验证方式、未做事项。
- 已知过时点（不影响分支内容）：文中“overlay 哈希守卫只看四个文件 / 4 处 `string(REPLACE)`”写于 2026-09-23/24 分支提交时点；此后旧仓在 #88（`avz_coroutine.cpp`）与 #84（`avz_cob_manager.cpp`）各加了 overlay，当前守卫是 6 个文件。后续同步时更新（见 [upstream-and-pins.md](upstream-and-pins.md) §8）。
- 归属：fork（文档）。现状：fork 已建；已审查消费方未采用。
- 验证：无（说明性文档）；缺：与当前 overlay 状态的同步更新。
- 上游提交：不计划（治理文档）。

---

## B. 旧仓构建期 overlay 补丁（`runtime/avz_overlay.cmake`）

子模块保持 pristine @ `c42676c`；overlay 在构建时读源、LF 归一化、比对守卫哈希、做替换，生成 `avz_*_overlay.cpp`。除 O11 外，替换对默认构建与托管构建都生效。

### O1 AvZ logger 替换

- 真实位置：上游 [`src/avz_script.cpp#L8`](https://github.com/vector-wlc/AsmVsZombies/blob/c42676c269b5b482a1eb9203a5b979e9d8a2a5c7/src/avz_script.cpp#L8)（`static ALogger<AMsgBox> logger;`）；overlay [L25](https://github.com/guajun/llm-vs-zombies/blob/389803f10bef8d1b2abdd7b1cee0b9328ac26332/runtime/avz_overlay.cmake#L25) → `lvz::runtime::DiagnosticLogger`（旧仓 `runtime/diagnostics.hpp`：输出 DebugView + 可选流）。
- 理由：AvZ 默认 logger 走消息框；常驻 runtime 需要非阻塞诊断输出。
- 归属：env（运行时/日志政策依赖 lvz runtime 类型）。
- 已有验证：离线 `avz_hosted_script` 链接同一 overlay（PR #85）；真机托管运行使用该 logger（`docs/avz-script-hosting.md` §5–§7，PR #85/#86/#90）。缺：对 DiagnosticLogger 路由本身的独立断言（离线假图有日志格式限制）。本治理任务未重跑。
- 上游提交：否（env 专用）。

### O2 / O3 异常路径改记日志

- 真实位置：上游 [`avz_script.cpp#L182`](https://github.com/vector-wlc/AsmVsZombies/blob/c42676c269b5b482a1eb9203a5b979e9d8a2a5c7/src/avz_script.cpp#L182)、[#L185](https://github.com/vector-wlc/AsmVsZombies/blob/c42676c269b5b482a1eb9203a5b979e9d8a2a5c7/src/avz_script.cpp#L185)；overlay L26–L29 把 `AMsgBox::Show` 换成 `aLogger->Error`。
- 理由：崩溃/异常时不再弹框阻塞，统一进诊断流；与 O1 同一政策。
- 归属：env。
- 已有验证：编译内建（默认与托管构建）。缺：直接命中 catch 分支的离线测试；托管运行只在真发生异常时才走该路径。
- 上游提交：否。

### O4 常驻 runtime 生命周期

- 真实位置：上游 [`avz_script.cpp#L45-L47`](https://github.com/vector-wlc/AsmVsZombies/blob/c42676c269b5b482a1eb9203a5b979e9d8a2a5c7/src/avz_script.cpp#L45-L47)；overlay L30–L32 在 `RunTotal()` 前插入 `if (lvz::runtime::Started()) { RunScript(); return; }`。
- 理由：常驻 runtime 拥有生命周期，不能进入 AvZ 递归 loop。
- 归属：env（依赖 `lvz::runtime::Started()`，属 runtime 政策而非通用钩子）。
- 已有验证：离线 `avz_hosted_script`（closed gate 不推进脚本/时钟；granted frame 恰好一次引擎调用；PR #85）；真机 `hosted-c1` 崩/`hosted-c5` 源跑+冷重放记录见 `docs/avz-script-hosting.md` §7.6 与 PR #90（`bad0ce2b`）。本治理任务未重跑。
- 缺失验证：无独立针对"递归被跳过"的计数器断言（由行为测试间接覆盖）。
- 上游提交：否。

### O5 帧门（ScriptHook）

- 真实位置：上游 [`avz_script.cpp#L193-L194`](https://github.com/vector-wlc/AsmVsZombies/blob/c42676c269b5b482a1eb9203a5b979e9d8a2a5c7/src/avz_script.cpp#L193-L194)；overlay L33–L35 插入 `BeforeFrame()` 与 `AfterAvzRunTotal()`。
- 理由：IPC drain、线程检查、ShouldStep 与帧预算由 runtime 裁决。
- 归属：env（帧调度政策）。
- 已有验证/缺失/上游：同 O4（含 PR #85 离线与 PR #90 §7.6 真机记录）。

### O6 受控引擎更新

- 真实位置：上游 [`avz_script.cpp#L198-L199`](https://github.com/vector-wlc/AsmVsZombies/blob/c42676c269b5b482a1eb9203a5b979e9d8a2a5c7/src/avz_script.cpp#L198-L199)；overlay L36–L51 插入 `RunOneEngineFrame()` 并在 `Started()` 时提前返回，跳过 AvZ 自己的 tick 循环。
- 理由：controller 负责所有帧预算与引擎调用边界（`AAsm::GameTotalLoop` 包装为受控调用）。
- 归属：env。
- 已有验证：离线 `avz_hosted_script`（每 granted frame 恰好一次引擎调用；暂停段 0 次；PR #85）；真机托管运行记录同 O4/PR #90。本治理任务未重跑。
- 缺失验证：真机上"帧预算与 AvZ 循环计数"的逐帧对账（由验收运行覆盖，未在本次复核）。
- 上游提交：否。

### O7 目标镜像身份校验

- 真实位置：上游 [`avz_hook.cpp#L26`](https://github.com/vector-wlc/AsmVsZombies/blob/c42676c269b5b482a1eb9203a5b979e9d8a2a5c7/src/avz_hook.cpp#L26)（`DllMain` 的 `DLL_PROCESS_ATTACH`）；overlay L52–L55 插入 `if (!lvz::determinism::ValidateTargetImage()) return FALSE;`。
- 理由：确定性适配器只接受固定 1.0.0.1051 镜像签名；装 hook 前拒绝未知镜像。实现见旧仓 [`determinism/audit.cpp`](https://github.com/guajun/llm-vs-zombies/blob/389803f10bef8d1b2abdd7b1cee0b9328ac26332/determinism/audit.cpp#L413) 与 `docs/determinism.md`。
- 归属：env（证据判等/镜像身份政策）。
- 已有验证：`ValidateTargetImage()` 的签名/布局检查有实现与文档；离线 hosted 测试把它桩为 `true`（测试假图）。缺：直接验证"错误镜像 → DllMain 返回 FALSE"的负例测试（本治理任务在旧仓测试目录未找到，不排除其他环境有）。
- 上游提交：否。

### O8 选卡/等待战斗跳过

- 真实位置：上游 [`avz_card.cpp#L222`](https://github.com/vector-wlc/AsmVsZombies/blob/c42676c269b5b482a1eb9203a5b979e9d8a2a5c7/src/avz_card.cpp#L222)；overlay L56 改为 `if (!lvz::runtime::Started()) AWaitForFight(selectInterval == 0);`。
- 理由：runtime 的 `initialize` 自己选卡并拒绝不匹配的列表；AvZ 自动流程在常驻模式下会与 runtime 竞争同一组卡槽。
- 归属：env（初始化目标/卡槽政策）。
- 已有验证（历史记录）：`docs/avz-script-hosting.md` §3（runtime 选卡决策与 `verify_scenario()`）；真机已由 #99 运行覆盖：PR #107（`346e495a`）的 `issue99-jd12r` 四轨迹在 `docs/issue99-铲子同根分叉.md` §1/§8 确认 `Scene()==2`、十卡槽、12 门炮，运行 manifest 绑定 `recorder_sha256`；正式 `fc2` 四轨迹（树 `ce770e531a225234b53ac3b810bc3209e4417b257410605f105bc38b0bc90220`、seal `work/issue99-fc2-seal.json`）由旧仓 #101 整改补记（PR #119，OPEN）记录同一结论；#111 阶段 D（PR #117，`11917a78`）四条 hosted-v2 冷启动（同一 hosted DLL `a6e7dcf06060a4d3c3a87545b5e3044f2a47786b09cc2a293a42c9deeb9fb8ad`）从 epoch3/tick0/rev5 跑到 tick1201/rev0 并到达 wave2（`docs/issue111-阶段D-离线复验与验收报告.md`）。hosting 文档的 open item 4 措辞早于这些运行；本治理任务未重跑真机，也不改写旧文档。
- 缺失验证：本治理任务未重跑（历史已有上述真机证据，不是"未做"）；托管选卡与 runtime `initialize` 的逐字段等价不在本任务范围。
- 上游提交：否。

### O9 协程等待双重恢复修复（#88）

- 真实位置：上游 [`avz_coroutine.cpp#L50-L51`](https://github.com/vector-wlc/AsmVsZombies/blob/c42676c269b5b482a1eb9203a5b979e9d8a2a5c7/src/avz_coroutine.cpp#L50-L51)；overlay L70–L79：先判"是否已到"，已到只运行一次 `func()`，否则才走 `AConnect` 失败路径。
- 理由：`__AOpQueueManager::Push()` 对已到期操作已经运行过一次并返回失败，原代码再运行一次 → 同一 `co_await` 恢复两次 → 残留操作在协程帧销毁后 `resume()`（访问违例）。这是 AvZ 自身语义缺陷，与 lvz runtime 无关，因此是候选上游 bugfix；当前因"子模块 pristine"以 overlay 落地。
- 归属：待评估（倾向于"先提上游"；在合并前属薄 fork 补丁或 overlay 均可，本次不移动）。
- 已有验证（历史记录）：离线 `tests/hosted_crash_probe_tests.cpp`（真触发访问违例的子进程验证 VEH 落盘与退出码）+ `test_avz_hosted_script.py` 接线；真机：修前 `hosted-c1` tick 3251 崩、`resumes 0→2→3`，修后 `hosted-c5` 源跑与冷重放均在 3151/3201/3251 各恢复一次并正常完成（`docs/avz-script-hosting.md` §7.6；根因修复 PR #90 `bad0ce2b`）。本治理任务未重跑。
- 缺失验证：上游是否接受该修复（未联系上游）；其他等待组合（`ONLY_FIGHT` tick runner 等）的回归未单列。
- 上游提交：否（未创建；建议作为独立 bugfix 评估）。

### O10 环境辅助收集审计

- 真实位置：上游 [`avz_smart.cpp#L62`](https://github.com/vector-wlc/AsmVsZombies/blob/c42676c269b5b482a1eb9203a5b979e9d8a2a5c7/src/avz_smart.cpp#L62)；overlay L88–L95 在 `ALeftClick(x, y)` 前插入 `lvz::runtime::RecordEnvironmentCollect(...)`（旧仓 `runtime/runtime.cpp` L518 写 `environment_collect` 审计事件）。
- 理由：环境辅助（自动收集）不经过请求 journal，需要在事件流里留痕。
- 归属：env（审计格式/证据政策）。
- 已有验证：编译内建；`RecordEnvironmentCollect` 有实现。缺：本治理任务未找到针对 collect 记录的独立断言测试，也未单独核对真机 collect 事件。
- 上游提交：否。

### O11 托管炮击审计（仅托管构建）

- 真实位置：上游 [`avz_cob_manager.cpp#L48`](https://github.com/vector-wlc/AsmVsZombies/blob/c42676c269b5b482a1eb9203a5b979e9d8a2a5c7/src/avz_cob_manager.cpp#L48)；overlay L101–L138 在 `AAsm::Fire(x, y, cobIdx);` 后插入 `RecordHostedFire(...)`，包在 `#ifdef LVZ_AVZ_HOSTED_FIRE_AUDIT` 内；默认构建不替换该文件。
- 理由：托管脚本的炮击是直接引擎调用，不进请求 journal；只补审计、不改语义（引擎调用次数/参数/顺序逐字节保持）。
- 归属：env（审计流格式与绑定规则）。
- 已有验证（历史记录）：离线 `avz_hosted_fire`（overlay + 真记录路径，与默认构建的引擎调用轨迹逐字节相同）、`avz_hosted_fire_default`（0 条记录）、`test_hosted_fire_audit.py`（声明/绑定/计数/digest/顺序负例与跨语言 digest 对账）；代码 [PR #89](https://github.com/guajun/llm-vs-zombies/pull/89)（`9b00d703`）、digest 对账修复 [PR #92](https://github.com/guajun/llm-vs-zombies/pull/92)（`40893d76`）。真机：2026-09-24 `jd12-smoke-01-s42-c0` 20 条炮击记录确认落盘并暴露 id ≥ 2^31 的 digest 混合缺陷（修复前证据被读取端有意拒绝）；修复后的真机端到端由 #99 运行提供——PR #107（`346e495a`）的 `issue99-jd12r` 四条轨迹每条 4 条 `hosted_fire`（tick 567、1168 各两发）、同分支原跑=复跑（`docs/issue99-铲子同根分叉.md` §6/§8）；旧仓 #101 整改矩阵（[PR #119](https://github.com/guajun/llm-vs-zombies/pull/119)，OPEN）R14/R15 复核了 `hosted_fire_count=4`/条与复跑，并记录正式 `fc2` 树 `ce770e531a225234b53ac3b810bc3209e4417b257410605f105bc38b0bc90220`（`experiments/trees/issue99-fc2-shovel-fork`、seal `work/issue99-fc2-seal.json`，只读校验见 trajectory-core [PR #2](https://github.com/pvz-agent-lab/trajectory-core/pull/2) `afb770ee`）。真机结论以绑定构建哈希的运行 manifest 为准。
- 缺失验证：本治理任务未重跑真机/离线（历史证据如上，不是"未做"）；托管炮击仍不是一等 `fire` 动作（无 request journal/不可重放），属协议扩展议题。
- 上游提交：否（env 专用）。

---

## C. 交叉核对

- **fork 与 overlay 无重叠**：fork 只改 `inc/avz_rng.h`、`src/avz_rng.cpp`、`inc/avz_pvz_struct.h`、`inc/libavz.h`（+ fork 自带 tests/docs）；overlay 守卫 6 个 `.cpp`（script/hook/card/coroutine/smart/cob_manager）。因此若未来把子模块切到 `e266e18`，现有 6 项守卫不会因 fork 改动而失败；`src/avz_rng.cpp` 会被旧仓 `file(GLOB avz/framework/src/*.cpp)`（CMakeLists L8）自动纳入。本治理任务**没有执行**任何 pin 切换。
- **fork 说明的过时描述**：见 F5；记录在案，后续同步更新。
- **无浮动分支**：旧仓以 `dependencies.lock.json` + 子模块 SHA 固定身份；fork 分支仅作为待评估代码，未进入任何运行路径。
- **无上游结论**：相关补丁（F1–F5、O1–O11）均未创建上游 PR——`LVZ-FORK-NOTES.md`（`e266e18`）明确"本轮没有向上游开 PR"；2026-09-26 检索 `vector-wlc/AsmVsZombies` 公开 PR/issue 未发现来自本组织的条目（检索范围仅该仓库公开列表）。凡"上游会不会接受/何时接受"均为未知，本表不写结论。

## D. 仍在 env 的 L1 知识（fork 尚未接管；迁移对象）

这些是 ADR-0001 下"通用访问原语应归 fork"的现有重复实现，本次**不迁移**，仅登记：

| 知识 | 位置（旧仓 @ `389803f`） | 说明 |
|---|---|---|
| 全局 MT 地址 | `determinism/audit.cpp` L37 `kGlobalMt = 0x75a910` | 与 F1 `AMTRand::INSTANCE` 重复 |
| 引擎 CRT 状态 | `determinism/audit.cpp` L197 `CrtStateAddress()`：`0x628a3d` + `0x14` | 与 F1 `AEngineCrtRand` 重复 |
| MT 状态结构 | `determinism/model.hpp` L15 `MtState`（624 words + cursor，精确大小 2500） | 与 F1 `AMTRand` 重复（序列化形状属 env） |
| MT 读写使用点 | `determinism/audit.cpp` `CaptureRng` L604 / `RestoreRng` L613 / `SeedRng` L635；`recording/native_capture.cpp` L18–L19 `GlobalMt` / `MtBytes`；`determinism/foley_trace.cpp` L216 cursor 地址 | 迁移时替换为 `A*` 原语 |
| MJ/App 时钟地址 | `determinism/mj_clock_anchor.cpp`、`determinism/app_update_anchor.cpp`、runtime B(0) | 与 F2 `AppUpdateCount`/`Write*` 重复；目标/回执/判等仍归 env |
| 镜像签名 | `determinism/audit.cpp` L413 `ValidateTargetImage()` + `TargetSignatures()` 字节表 | 通用版本自检可考虑 fork（F1 `AIsRngLayoutValid` 已有一份）；本治理任务未合并 |
| 实验插桩 | `determinism/lifecycle_probes.cpp` 等 #110 之后的逐版本探针 | 属证据用插桩，按 ADR 留在 env；若出现可复用原语再评估 |

迁移状态：未开始；等价校验方式见 [upstream-and-pins.md](upstream-and-pins.md) §5–§6。禁止在未经等价验证的情况下删除 env 侧守卫或地址表。

## E. 证据索引

- fork 分支：`37e1eea`、`e266e18`；`LVZ-FORK-NOTES.md`（地址证据、验证、未做事项）。
- 旧仓 overlay：[`runtime/avz_overlay.cmake`](https://github.com/guajun/llm-vs-zombies/blob/389803f10bef8d1b2abdd7b1cee0b9328ac26332/runtime/avz_overlay.cmake)（守卫与 11 处替换）。
- 旧仓文档：[`docs/avz-script-hosting.md`](https://github.com/guajun/llm-vs-zombies/blob/389803f10bef8d1b2abdd7b1cee0b9328ac26332/docs/avz-script-hosting.md)（托管、#88、炮击审计与各离线测试）、[`docs/determinism.md`](https://github.com/guajun/llm-vs-zombies/blob/389803f10bef8d1b2abdd7b1cee0b9328ac26332/docs/determinism.md)（RNG/时钟捕获恢复）、[`docs/b0-normalization-native.md`](https://github.com/guajun/llm-vs-zombies/blob/389803f10bef8d1b2abdd7b1cee0b9328ac26332/docs/b0-normalization-native.md)（目标/回执/fail-closed）。
- 测试：`tests/avz_hosted_script_tests.cpp`、`tests/avz_hosted_fire_tests.cpp`、`tests/hosted_crash_probe_tests.cpp`、`tests/test_hosted_fire_audit.py`、`tests/test_avz_hosted_script.py`（均在旧仓 @ `389803f`）。
- 后续真机/封存证据：[#107 四轨迹文档](https://github.com/guajun/llm-vs-zombies/blob/389803f10bef8d1b2abdd7b1cee0b9328ac26332/docs/issue99-%E9%93%B2%E5%AD%90%E5%90%8C%E6%A0%B9%E5%88%86%E5%8F%89.md)、[#111 阶段 D 报告](https://github.com/guajun/llm-vs-zombies/blob/389803f10bef8d1b2abdd7b1cee0b9328ac26332/docs/issue111-%E9%98%B6%E6%AE%B5D-%E7%A6%BB%E7%BA%BF%E5%A4%8D%E9%AA%8C%E4%B8%8E%E9%AA%8C%E6%94%B6%E6%8A%A5%E5%91%8A.md)、#101 整改矩阵（[PR #119](https://github.com/guajun/llm-vs-zombies/pull/119)，OPEN）、trajectory-core [PR #2](https://github.com/pvz-agent-lab/trajectory-core/pull/2)（`afb770ee`）。
- 上游锚点：见 O1–O11 的永久链接。
