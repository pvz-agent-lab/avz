#ifndef __AVZ_RNG_H__
#define __AVZ_RNG_H__

#include "avz_pvz_struct.h"
#include "avz_types.h"
#include <array>
#include <cstddef>
#include <cstdint>

// 游戏随机数状态
//
// 原版引擎里参与玩法的随机数有两处，并且都不会随存档往返：
// 1. Sexy 框架的全局 MT19937 实例（Sexy::MTRand 的全局对象），状态是 624 个 32 位字加一个游标，
//    抽波、僵尸速度、音效变体、粒子等共用这一条流；
// 2. 引擎静态链接的 CRT 的 rand / srand，状态在当前线程的 CRT 数据块中，
//    粒子的抖动种子由它产生。
// 因此想让某个时刻的状态可复现（存档重入、跨进程对齐、录制与回放），
// 就必须能直接读写这两处状态。
//
// 下列地址与布局只对 1.0.0.1051 英文版成立，可以用 AIsRngLayoutValid() 自检。

// 全局 MT19937 实例
struct AMTRand {
    // 全局实例所在地址
    static constexpr uintptr_t INSTANCE = 0x75a910;
    // 游标相对实例的偏移
    static constexpr uintptr_t CURSOR_OFFSET = 0x9c0;
    // 状态字数
    static constexpr std::size_t WORD_COUNT = 624;
    // 种子为 0 时构造函数使用的默认种子
    static constexpr uint32_t DEFAULT_SEED = 0x1105;

    // 合法游标范围为 [0, 624]，取值 624 表示下一次取数前先做一次 twist
    uint32_t words[WORD_COUNT] {};
    uint32_t cursor {};

    bool operator==(const AMTRand&) const = default;
};

static_assert(sizeof(AMTRand) == AMTRand::CURSOR_OFFSET + sizeof(uint32_t));
static_assert(offsetof(AMTRand, cursor) == AMTRand::CURSOR_OFFSET);

// MT19937 相关代码在这份镜像里的位置
struct AMTRandImage {
    // MTRand::MTRand(uint32_t seed)，调用约定为 eax = this，ecx = seed
    static constexpr uintptr_t SEED_FUNCTION = 0x5a98d0;
    // 引擎内部真正被调用的跳板，内容为 jmp SEED_FUNCTION
    static constexpr uintptr_t SEED_THUNK = 0x5a9890;
    // 全局实例的静态初始化，以 DEFAULT_SEED 构造 INSTANCE
    static constexpr uintptr_t GLOBAL_INITIALIZER = 0x5a98a0;
    // 使用全局实例的随机数包装函数，内容为 mov edx, INSTANCE
    static constexpr uintptr_t GLOBAL_WRAPPER = 0x5a9930;
    // 引擎里全部 4 个“构造一个 MT19937 实例”的调用点
    static constexpr std::array<uintptr_t, 4> SEED_CALL_SITES {0x40a7ea, 0x4256b7, 0x426a42, 0x484173};
};

// 引擎静态链接的 CRT 的 rand / srand 状态
struct AEngineCrtRand {
    // _getptd()，返回当前线程的 CRT 数据块
    static constexpr uintptr_t GETPTD = 0x628a3d;
    // rand()
    static constexpr uintptr_t RAND = 0x61e087;
    // srand()
    static constexpr uintptr_t SRAND = 0x61e07a;
    // 状态在 _getptd() 返回对象中的偏移
    static constexpr uintptr_t STATE_OFFSET = 0x14;
};

// 读取 MT19937 状态，instance 默认为全局实例
__ANodiscard AMTRand& AGetMTRand(uintptr_t instance = AMTRand::INSTANCE) noexcept;

// 覆写 MT19937 状态，instance 默认为全局实例
void AWriteMTRand(const AMTRand& state, uintptr_t instance = AMTRand::INSTANCE) noexcept;

// 由 _getptd() 返回的对象得到 rand / srand 状态
// 注意：CRT 状态是每线程的，此处只应传入当前线程（游戏线程）的 _getptd() 返回值
__ANodiscard uint32_t& AEngineCrtRandState(void* ptd) noexcept;

// 当前线程的 rand / srand 状态
__ANodiscard uint32_t& AGetEngineCrtRand() noexcept;

// 覆写当前线程的 rand / srand 状态，返回实际读回值
__ANodiscard uint32_t AWriteEngineCrtRand(uint32_t value) noexcept;

// 解析 call rel32 指令的目标地址
// site 为指令所在地址，code 指向该指令的首字节，非 E8 指令时返回 0
__ANodiscard uintptr_t ARel32CallTarget(uintptr_t site, const uint8_t* code) noexcept;

// 自检：本文件中的常量与当前进程里已加载的镜像是否一致
// 只在原版引擎已加载时为真，游戏未加载或镜像不符时返回 false
__ANodiscard bool AIsRngLayoutValid() noexcept;

#endif
