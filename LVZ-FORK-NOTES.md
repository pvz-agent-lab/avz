# LVZ fork 说明

本仓库是 [`vector-wlc/AsmVsZombies`](https://github.com/vector-wlc/AsmVsZombies) 的个人 fork，由
[llm-vs-zombies](https://github.com/guajun/llm-vs-zombies) 维护，供该项目在「游戏内存 / RNG / 时钟原语」这一层使用。
上游与本 fork 都是 GPL-3.0，许可无冲突。基线提交：
`c42676c269b5b482a1eb9203a5b979e9d8a2a5c7`（上游 release 2.9.2）。

## 为什么存在这个 fork

llm-vs-zombies 的 L1 层（游戏内存与版本适配）原本被切成两半：结构布局在 AvZ 的 `avz_pvz_struct.h`，
而 RNG 与时钟的地址知识只存在于该项目的 `determinism/`。归属原则（llm-vs-zombies issue #72 已定）：

| 判据 | 归属 |
|---|---|
| 「怎么读写这个游戏的某个量」（per-version 地址、结构偏移、读写原语） | AvZ（先提上游，落地在薄 fork） |
| 「这个量在实验里意味着什么」（声明目标、回执、判等、作用域） | llm-vs-zombies（不进 fork） |

方案是 **upstream-first + 薄 fork**：fork 只保留少量、明确、将来可以提上游的补丁，
并用下面的差异表维护「为什么 / 是否已提 PR / 链接」。
**本轮没有向上游开 PR**（draft 也没有）：按 issue #72 的安排，等配方完整性检查产出完整字段清单后再一次性提交。

## 差异表

相对基线 `c42676c2` 的全部差异：

| 编号 | 文件 | 能力 | 上游为什么需要它 | 已提上游 PR | 链接 |
|---|---|---|---|---|---|
| F1 | `inc/avz_rng.h`（新）、`src/avz_rng.cpp`（新） | 游戏全局 MT19937 实例（`0x75a910`）的 624 个状态字 + 游标（`+0x9c0`）读写原语；引擎静态 CRT `rand` / `srand` 的每线程状态（`_getptd()` → `+0x14`）读写；MT 构造函数、全局初始化器、4 个 MT 构造点的地址常量；`AIsRngLayoutValid()` 布局自检与 `ARel32CallTarget()` | 引擎的随机数流不随存档往返，脚本想让「某个时刻」可复现（存档重入、录制回放、跨进程对齐）就必须能直接读写它。AvZ 现有 API 完全没有游戏侧 RNG：在上游源码里检索 `MTRand` / `0x75A910` / `_getptd` 均无命中（本轮实测） | 否 | issue [#72](https://github.com/guajun/llm-vs-zombies/issues/72) |
| F2 | `inc/avz_pvz_struct.h`（改） | `APvzBase::AppUpdateCount()`（`LawnApp + 0x484`，反编译 `LawnApp::mUpdateCount`）；`APvzBase::WriteMjClock()` / `WriteAppUpdateCount()`：写入后返回**实际读回值** | `MjClock()` 早已存在，但 App 更新计数在框架里缺失（它决定原版演示时基与更新间隔）。两个计数都可能被脚本在战斗开始前对齐，因此需要文档化的写入原语，并且必须报告真实读回值：调用者不能假设写入生效 | 否 | 同上 |
| F3 | `inc/libavz.h`（改） | 把 `avz_rng.h` 加入总头文件（1 行） | —— | 否 | 同上 |
| F4 | `tests/avz_fork_selfcheck.cpp`（新） | 离线自检：常量与结构布局、合成缓冲区上的读写往返、以及（可选）对原版镜像逐条核对字节 | 这些地址只对 1.0.0.1051 成立，常量必须有出处、可复核。若上游接受 F1 / F2，这份自检可以作为附带证据；本轮先留在 fork | 否 | 同上 |

F4 与本文件属于本 fork 的验证与记录材料，不打算混进上游补丁；
`F1` + `F2` + `F3` 是准备提上游的那一部分。

## 地址证据

目标镜像：`PlantsVsZombies.exe`（PvZ 1.0.0.1051 英文版），
SHA-256 `f9669af338964787a3785a7895791297d599295b8bb669b0db49443f736a1322`，
加载基址 `0x400000`，文件 3,007,800 字节。下表地址均为该镜像的绝对地址（VA）。

| 量 | 常量 | 反汇编证据 |
|---|---|---|
| 全局 MT19937 实例 | `0x75a910` | `0x5a98a6 mov eax, 0x75a910`（全局实例的静态初始化）、`0x5a9931 mov edx, 0x75a910`（使用全局实例的包装函数） |
| 游标偏移 | `+0x9c0` | `0x5a98db mov dword ptr [eax + 0x9c0], 1`（构造时将游标置 1）；`0x5a9913 cmp dword ptr [eax + 0x9c0], 0x270`（取数时游标 ≥ 624 先 twist） |
| MT 构造函数 | `0x5a98d0` | `test ecx, ecx` / `jne` / `mov ecx, 0x1105`（种子为 0 时的默认种子）/ `mov dword ptr [eax], ecx` / `mov dword ptr [eax + 0x9c0], 1` / 逐字 `imul …, 0x6c078965` |
| 4 个 MT 构造点 | `0x40a7ea`、`0x4256b7`、`0x426a42`、`0x484173` | 每处都是 `call rel32` → `0x5a9890`；后者是 `jmp 0x5a98d0` 的跳板。扫描整个 `.text`：指向该跳板的调用点**恰好**这 4 个（另有 `0x5a98a0` 的全局初始化器直接调用 `0x5a98d0`） |
| `_getptd()` | `0x628a3d` | `56 e8 83 ff ff ff …` |
| `srand` / `rand` | `0x61e07a` / `0x61e087` | `srand`：`call _getptd` → `mov dword ptr [eax + 0x14], ecx`；`rand`：`call _getptd` → `imul ecx, ecx, 0x343fd` / `add ecx, 0x269ec3` → 取第 16~30 位 |
| MJ 时钟（`mAppCounter`，AvZ `MjClock()`） | `0x838` | 写入点 `0x4526e6 add dword ptr [edi + 0x838], ebp`；构造函数置零 `0x44ec2a mov dword ptr [esi + 0x838], ebx` |
| App 更新计数（`mUpdateCount`） | `0x484` | 读取点 `0x54ec60 mov eax, dword ptr [edi + 0x484]`（随后 `sub eax, dword ptr [edi + 0x578]` 构造更新间隔） |

llm-vs-zombies 侧的原始证据（与本表一致）：`determinism/evidence.json` 的 `rng_layout` 与
`mt_seed_instance_calls`、`docs/determinism.md`、`docs/mj-clock-anchor-native.md`、
`docs/app-update-anchor-native.md`、`docs/机制读取点表.md`（L59 / L61 / L64 / L74）。

本轮新增的细节：证据里的 4 个「MT 构造点」是**调用点**地址，它们的直接目标是跳板 `0x5a9890`
（`jmp 0x5a98d0`），不是 `0x5a98d0` 本体；`0x5a98d0` 自带默认种子 `0x1105`，
全局实例的静态初始化（`0x5a98a0`）就用这个种子构造 `0x75a910`。

## 验证

全部使用 llm-vs-zombies 固定的 llvm-mingw i686 工具链；**不需要运行游戏**。

1. **上游构建**（与 `tools/build-avz.ps1` 相同的配置方式）：

   ```powershell
   cmake -S . -B build/cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_NAME=Windows `
     -DCMAKE_C_COMPILER=<toolchain>/i686-w64-mingw32-clang.exe `
     -DCMAKE_CXX_COMPILER=<toolchain>/i686-w64-mingw32-clang++.exe
   cmake --build build/cmake --target avz --parallel 4
   ```

   结果：26 个 TU 全部编译，链接出 `bin/libavz.a`，`-Wall` 下无告警。

2. **离线自检**：

   ```powershell
   i686-w64-mingw32-clang++.exe -m32 -std=c++20 -fexperimental-library -Wall -Wextra -I inc `
     tests/avz_fork_selfcheck.cpp src/avz_rng.cpp -static -o build/lvz/avz_fork_selfcheck.exe
   build/lvz/avz_fork_selfcheck.exe <PlantsVsZombies.exe>
   ```

   结果：`57 checks, 0 failures`（不带镜像路径时只跑常量 / 布局 / 往返，31 项）。
   其中 26 项直接核对镜像字节：构造函数里的默认种子与游标偏移、跳板、4 个构造点、全局初始化器、
   `srand` / `rand` 的 `_getptd()` 调用与 `+0x14` 位移、以及两个 App 计数的偏移。

   负控（证明检查不是空转）：

   - 把镜像换成包装器 `game/original/PlantsVsZombies.exe` → `24 failures`；
   - 开发过程中两个位移曾写错 2 个字节，自检立刻报出对应的两条 `FAIL`。

3. **与 llm-vs-zombies 的接入面**：该项目的 `runtime/avz_overlay.cmake` 用哈希守卫只看
   `avz_script.cpp` / `avz_hook.cpp` / `avz_card.cpp` / `avz_smart.cpp` 四个文件（LF 归一化后的
   SHA-256）。本补丁不碰这四个文件，本轮实测四个哈希与守寻常量逐一相同，4 处 `string(REPLACE)`
   的锚点也未被改动；新增的 `src/avz_rng.cpp` 会被该项目 `file(GLOB avz/framework/src/*.cpp)`
   自动纳入。子模块 pin 不变。

## 未做 / 待定

- **未开 PR**（含 draft）：等配方完整性检查给出完整字段清单后一次性提上游。
- **未在真实游戏里运行**：`AIsRngLayoutValid()` 与这些读写原语只做了离线字节级验证
  （本轮的机器上游戏进程被别的任务占用）。真机验证仍待补。
- **有意不放进 fork**：MT 的 twist / temper / 播种算法（不重实现游戏算法，只在 F2 的写入原语之外
  提供状态读写）、B(0) 归一化的声明形状、回执与判等、分支作用域、请求去重日志、IPC 协议、
  审计流格式——这些属于 llm-vs-zombies。
- **llm-vs-zombies 侧的切换未做**：`determinism/` 目前仍用自己的 `MtState` / `_getptd` 逻辑，
  要改用本原语需要先 bump 子模块 pin，本轮不动 pin、也不改该项目的任何文件。
- **同步方式**：以 `master` 为基线 rebase，保持补丁为「新增文件 + `avz_pvz_struct.h` 的小改」。
  上游一旦接受 F1 / F2，fork 直接删掉对应差异。

## 相关链接

- llm-vs-zombies issue [#72](https://github.com/guajun/llm-vs-zombies/issues/72)（管线分层与层间接口，L1↔L2）
- 上游仓库：<https://github.com/vector-wlc/AsmVsZombies>
- 本分支：`lvz/l1-determinism-primitives`
