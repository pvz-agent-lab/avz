// 本模块只依赖自己的头文件：它不引用 AvZ 的其它状态，
// 因此可以脱离框架单独编译，供 tests/avz_fork_selfcheck.cpp 离线自检。
#include "avz_rng.h"

#include <cstring>

AMTRand& AGetMTRand(uintptr_t instance) noexcept {
    return AMRef<AMTRand>(instance);
}

void AWriteMTRand(const AMTRand& state, uintptr_t instance) noexcept {
    AGetMTRand(instance) = state;
}

uint32_t& AEngineCrtRandState(void* ptd) noexcept {
    return *reinterpret_cast<uint32_t*>(uintptr_t(ptd) + AEngineCrtRand::STATE_OFFSET);
}

uint32_t& AGetEngineCrtRand() noexcept {
    using GetPtd = void* (*)();
    return AEngineCrtRandState(reinterpret_cast<GetPtd>(AEngineCrtRand::GETPTD)());
}

uint32_t AWriteEngineCrtRand(uint32_t value) noexcept {
    volatile uint32_t& state = AGetEngineCrtRand();
    state = value;
    return state;
}

uintptr_t ARel32CallTarget(uintptr_t site, const uint8_t* code) noexcept {
    if (code[0] != 0xe8)
        return 0;
    int32_t offset = 0;
    std::memcpy(&offset, code + 1, sizeof(offset));
    return site + 5 + uintptr_t(intptr_t(offset));
}

// 自检只做读取，游戏未加载或地址不可读时直接判定失败，不做任何修正
namespace {
bool AIsReadable(uintptr_t address, std::size_t size) {
    MEMORY_BASIC_INFORMATION info {};
    if (!VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)))
        return false;
    if (info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
        return false;
    return uintptr_t(info.BaseAddress) + info.RegionSize >= address + size;
}

template <std::size_t Size>
bool AMatch(uintptr_t address, const uint8_t (&bytes)[Size]) {
    if (!AIsReadable(address, Size))
        return false;
    return std::memcmp(reinterpret_cast<const void*>(address), bytes, Size) == 0;
}

bool ACallTargetIs(uintptr_t site, uintptr_t target) {
    if (!AIsReadable(site, 5))
        return false;
    const auto code = reinterpret_cast<const uint8_t*>(site);
    return code[0] == 0xe8 && ARel32CallTarget(site, code) == target;
}

bool AJumpTargetIs(uintptr_t site, uintptr_t target) {
    if (!AIsReadable(site, 5))
        return false;
    const auto code = reinterpret_cast<const uint8_t*>(site);
    if (code[0] != 0xe9)
        return false;
    int32_t offset = 0;
    std::memcpy(&offset, code + 1, sizeof(offset));
    return site + 5 + uintptr_t(intptr_t(offset)) == target;
}
}

bool AIsRngLayoutValid() noexcept {
    // 构造函数开头：种子为 0 时换成默认种子，写入首字，游标置 1，随后逐字初始化
    static constexpr uint8_t seed[] = {0x85, 0xc9, 0x75, 0x05, 0xb9, 0x05, 0x11, 0x00, 0x00, 0x89, 0x08,
        0xc7, 0x80, 0xc0, 0x09, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
    // 全局实例的静态初始化：mov ecx, DEFAULT_SEED; mov eax, INSTANCE; call SEED_FUNCTION
    static constexpr uint8_t globalInit[] = {0xb9, 0x05, 0x11, 0x00, 0x00, 0xb8, 0x10, 0xa9, 0x75, 0x00};
    // 全局实例的包装函数：mov edx, INSTANCE; jmp ...
    static constexpr uint8_t globalWrapper[] = {0xba, 0x10, 0xa9, 0x75, 0x00};
    // srand：call _getptd; mov [eax + STATE_OFFSET], [esp + 4]
    static constexpr uint8_t srand[] = {0xe8, 0xbe, 0xa9, 0x00, 0x00, 0x8b, 0x4c, 0x24, 0x04, 0x89, 0x48, 0x14, 0xc3};
    // rand：call _getptd; 读取 [eax + STATE_OFFSET] 并做一次 15 位线性同余
    static constexpr uint8_t rand[] = {0xe8, 0xb1, 0xa9, 0x00, 0x00, 0x8b, 0x48, 0x14, 0x69, 0xc9, 0xfd, 0x43, 0x03, 0x00};
    if (!AMatch(AMTRandImage::SEED_FUNCTION, seed))
        return false;
    if (!AMatch(AMTRandImage::GLOBAL_INITIALIZER, globalInit))
        return false;
    if (!AMatch(AMTRandImage::GLOBAL_WRAPPER, globalWrapper))
        return false;
    if (!AJumpTargetIs(AMTRandImage::SEED_THUNK, AMTRandImage::SEED_FUNCTION))
        return false;
    if (!ACallTargetIs(AMTRandImage::GLOBAL_INITIALIZER + sizeof(globalInit), AMTRandImage::SEED_FUNCTION))
        return false;
    for (auto site : AMTRandImage::SEED_CALL_SITES)
        if (!ACallTargetIs(site, AMTRandImage::SEED_THUNK))
            return false;
    if (!AMatch(AEngineCrtRand::SRAND, srand) || !AMatch(AEngineCrtRand::RAND, rand))
        return false;
    return ACallTargetIs(AEngineCrtRand::SRAND, AEngineCrtRand::GETPTD)
        && ACallTargetIs(AEngineCrtRand::RAND, AEngineCrtRand::GETPTD);
}
