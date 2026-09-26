# ADR-0001: L1 归属边界——通用原语归薄 fork，环境政策归 env

- 状态：Accepted（2026-09-26）
- 范围：`pvz-agent-lab/*` 各仓与旧仓 `guajun/llm-vs-zombies` 的迁移、新功能归属
- 背景来源：[guajun/llm-vs-zombies#72](https://github.com/guajun/llm-vs-zombies/issues/72)（原归属原则）、[#101](https://github.com/guajun/llm-vs-zombies/issues/101)（P2 整改项）、[pvz-agent-lab/avz#1](https://github.com/pvz-agent-lab/avz/issues/1)
- 现状盘点：[patch-registry.md](patch-registry.md)；pin 与流程：[upstream-and-pins.md](upstream-and-pins.md)

## 背景

#72 已确立的方向是"怎么读写这个游戏的某个量"与"这个量在实验里意味着什么"分开。当时的具体表述接近"所有游戏地址原语一律进 AvZ（薄 fork）"。实践后出现两个问题：

1. L1 仍被切成两半：结构布局在 AvZ，RNG/时钟地址在旧仓 `determinism/`；fork 分支（`lvz/l1-determinism-primitives`）已建但无人采用。
2. 另一些"碰了 AvZ 源文件"的能力其实不是通用原语，而是环境专属政策：日志路由、IPC/帧预算门、镜像身份判等、初始化选卡、审计流格式、窗口/无头政策。把这些推给上游既没有通用性，也会让 fork 变厚。

本 ADR 明确新的归属判据，**作为今后新增与迁移的决策依据，不追溯改写历史**：已有补丁的既有归属与提交信息保持原样，只在 [patch-registry.md](patch-registry.md) 中登记"现状归属"与"按 ADR 的目标归属"。

## 决策

### 1. 薄 fork `pvz-agent-lab/avz` 承接

- **通用访问原语**：读写固定版本游戏状态的最小 API（如 F1 的 MT/CRT 状态读写、F2 的 App 计数读写）。
- **版本适配**：per-version 地址、结构偏移、布局自检与其证据（常量 + 自检函数）。
- **上游可接受的生命周期钩子**：语义不依赖任何实验概念、换一个使用者也成立的钩子（例如 AvZ 自身协程/等待语义的修复）。

判据 A：这个能力离开本实验、换一个消费者仍然成立吗？成立 → fork。

### 2. 环境仓 `pvz-env`（当前在 `llm-vs-zombies`）承接

- 环境专用原生适配与插桩（为特定实验采集证据的探针、审计记录）。
- 初始化目标与归一化声明（目标值、顺序、fail-closed 语义）。
- 证据判等、回执、镜像身份/签名政策。
- IPC、帧预算/调度、请求 journal、动作协议。
- 窗口/无头/焦点政策。
- 审计/轨迹格式与跨语言读取规则。

判据 B：它是否依赖实验概念（`Started`、revision、journal、回执、声明目标、证据比对）？依赖 → env。

### 3. AvZ 自身语义缺陷

判据 C：它是否是 AvZ 自身语义缺陷的修复？是 → 优先"先提上游"；在上游合并前可以薄 fork 补丁或 overlay 落地，并登记为候选上游项。登记时必须区分"已在 fork/overlay 落地"与"已提上游"，后者当前为 0。

### 4. 冲突时的裁决规则

- 一个能力现在看似 env 专用，但出现第二个消费者或脱离实验仍然成立时，按"上游优先 → 薄 fork"迁移，不做双份实现。
- fork 不吸收实验政策；env 不再新增不与实验绑定的 per-version 地址表；确需新增时在 registry 登记并说明为何暂不能进 fork。

## 对现状的效力

- **O1–O8、O10、O11 保持 env**：它们依赖 lvz runtime / 审计 / 初始化政策，现有归属就是正确归属。
- **O9（协程双重恢复修复）**：按判据 C 是候选上游 bugfix；当前 overlay 落地不变，是否提交上游单独评估。
- **F1–F5 保持 fork 已建状态**：没有被采用，也没有被合并；env 侧 `determinism/` 的重复实现（registry §D）按迁移步骤逐步收敛，期间不得删除守卫。
- **不追改历史**：#72 原文、fork 提交信息、旧仓文档中按当时口径写的归属表述都保留；本 ADR 只约束今后与迁移动作。

## 后果

- 正面：fork 保持"少量、可审、可上游"；env 的实验政策变化不需要上游节奏；两边的差异有单一登记处（registry）。
- 代价：迁移期存在重复实现（env 地址表 + fork 原语），必须显式登记并在等价校验通过后收敛；不允许多头状态表。
- 验证门槛：任何"归 fork"的迁移都必须先做离线等价（对照 `determinism/evidence.json` 签名/布局与 fork 自检），涉及帧/RNG/加载路径的还需要真机验收；未运行不得写等价。

## 被否方案

- **所有游戏地址原语一律进 fork**：#72 的宽读法。会让 fork 携带实验政策，变厚且不可上游。
- **全部留在 env**：同一版本的地址知识会在多个 env 消费者中复制，RNG/时钟的重复问题扩大。
- **一次性把 overlay 补丁全搬进 fork**：没有等价验证的历史代码搬动，风险大、review 成本高，也不符合"逐项替换、保留等价验收"的要求。
