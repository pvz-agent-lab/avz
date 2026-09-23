// 离线自检：不会启动游戏，也不会读写游戏进程。
//
// 1. 常量与结构布局（与原始二进制证据一致）；
// 2. 原语在合成缓冲区上的读写往返，以及相邻字节不被触碰；
// 3. 可选：给定原版镜像路径时，逐条核对常量与镜像里的真实字节，
//    例如四个 MT 构造点确实是 call SEED_THUNK，srand 确实读写 _getptd() + 0x14。
//
// 用法：avz_fork_selfcheck.exe [PlantsVsZombies.exe]
// 不给镜像路径时只跑前两组检查，退出码 0 表示全部通过。

#include "avz_rng.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
int checks = 0;
int failures = 0;

void Check(bool condition, const std::string& what) {
    ++checks;
    if (condition)
        return;
    ++failures;
    std::printf("FAIL %s\n", what.c_str());
}

void CheckEqual(unsigned long long actual, unsigned long long expected, const std::string& what) {
    ++checks;
    if (actual == expected)
        return;
    ++failures;
    std::printf("FAIL %s (actual %llu, expected %llu)\n", what.c_str(), actual, expected);
}

// 合成缓冲区上的读写往返，不接触游戏进程
void CheckPrimitives() {
    struct Guarded {
        uint32_t before = 0xa5a5a5a5;
        AMTRand mt;
        uint32_t after = 0x5a5a5a5a;
    } guarded;
    const auto instance = reinterpret_cast<uintptr_t>(&guarded.mt);
    AMTRand state;
    for (std::size_t i = 0; i < AMTRand::WORD_COUNT; ++i)
        state.words[i] = uint32_t(i * 2654435761u + 0x9e3779b9u);
    state.cursor = 137;
    AWriteMTRand(state, instance);
    Check(std::memcmp(&guarded.mt, &state, sizeof(state)) == 0, "MT write must land as raw little endian words");
    Check(AGetMTRand(instance) == state, "MT read back must equal the written state");
    CheckEqual(guarded.before, 0xa5a5a5a5, "MT write must not touch the preceding word");
    CheckEqual(guarded.after, 0x5a5a5a5a, "MT write must not touch the following word");
    // 游标 624 表示下一次取数前先 twist，本原语按原值往返，不做范围修正
    state.cursor = 624;
    AWriteMTRand(state, instance);
    CheckEqual(AGetMTRand(instance).cursor, 624, "cursor 624 must round trip unchanged");

    alignas(4) uint8_t block[0x40] {};
    const auto ptd = reinterpret_cast<void*>(block);
    CheckEqual(reinterpret_cast<uintptr_t>(&AEngineCrtRandState(ptd)) - reinterpret_cast<uintptr_t>(block),
        AEngineCrtRand::STATE_OFFSET, "CRT rand state must live at _getptd() + STATE_OFFSET");
    AEngineCrtRandState(ptd) = 0x12345678;
    Check(std::memcmp(block + AEngineCrtRand::STATE_OFFSET, "\x78\x56\x34\x12", 4) == 0,
        "CRT rand state must be a little endian 32 bit word");
    CheckEqual(block[AEngineCrtRand::STATE_OFFSET - 1], 0, "CRT write must not touch the preceding byte");
    CheckEqual(block[AEngineCrtRand::STATE_OFFSET + 4], 0, "CRT write must not touch the following byte");
}

// App 两个计数的写入原语，同样在合成对象上验证
void CheckAppClockPrimitives() {
    std::vector<uint8_t> memory(0x900, 0);
    auto* app = reinterpret_cast<APvzBase*>(memory.data());
    const auto base = reinterpret_cast<uintptr_t>(memory.data());
    CheckEqual(reinterpret_cast<uintptr_t>(&app->MjClock()) - base, 0x838, "MjClock field offset");
    CheckEqual(reinterpret_cast<uintptr_t>(&app->AppUpdateCount()) - base, 0x484, "AppUpdateCount field offset");
    auto* before = reinterpret_cast<int*>(memory.data() + 0x834);
    auto* after = reinterpret_cast<int*>(memory.data() + 0x83c);
    *before = 0x11111111;
    *after = 0x22222222;
    CheckEqual(unsigned(app->WriteMjClock(1340)), 1340, "WriteMjClock must return the value it read back");
    CheckEqual(unsigned(*reinterpret_cast<int*>(memory.data() + 0x838)), 1340, "WriteMjClock must land on the field");
    CheckEqual(unsigned(*before), 0x11111111u, "WriteMjClock must not touch the preceding word");
    CheckEqual(unsigned(*after), 0x22222222u, "WriteMjClock must not touch the following word");
    CheckEqual(unsigned(app->WriteAppUpdateCount(1500)), 1500,
        "WriteAppUpdateCount must return the value it read back");
    CheckEqual(unsigned(*reinterpret_cast<int*>(memory.data() + 0x484)), 1500,
        "WriteAppUpdateCount must land on the field");
    CheckEqual(unsigned(app->WriteAppUpdateCount(-1)), 0xffffffffu,
        "WriteAppUpdateCount must keep the raw 32 bit value");
}

// 只读地解析 PE，把地址换算成文件偏移
class Image {
public:
    struct Section {
        uint32_t virtualAddress;
        uint32_t virtualSize;
        uint32_t rawOffset;
        uint32_t rawSize;
    };

    bool Load(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return false;
        _bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        if (_bytes.size() < 0x100 || Read<uint16_t>(0) != 0x5a4d)
            return false;
        const auto pe = Read<uint32_t>(0x3c);
        if (pe + 0x100 > _bytes.size() || Read<uint32_t>(pe) != 0x00004550)
            return false;
        if (Read<uint16_t>(pe + 4) != 0x014c)
            return false;
        const auto optionalHeaderSize = Read<uint16_t>(pe + 20);
        const auto sections = Read<uint16_t>(pe + 6);
        _imageBase = Read<uint32_t>(pe + 24 + 28);
        for (uint16_t index = 0; index < sections; ++index) {
            const auto header = pe + 24 + optionalHeaderSize + index * 40;
            if (header + 40 > _bytes.size())
                return false;
            Section section;
            section.virtualSize = Read<uint32_t>(header + 8);
            section.virtualAddress = Read<uint32_t>(header + 12);
            section.rawSize = Read<uint32_t>(header + 16);
            section.rawOffset = Read<uint32_t>(header + 20);
            if (section.rawOffset + section.rawSize > _bytes.size())
                return false;
            _sections.push_back(section);
        }
        return !_sections.empty();
    }

    uint32_t ImageBase() const { return _imageBase; }
    std::size_t Size() const { return _bytes.size(); }

    // 返回地址处的文件字节，地址不落在任何节里时返回 nullptr
    const uint8_t* At(uintptr_t address, std::size_t size) const {
        if (_imageBase == 0 || address < _imageBase)
            return nullptr;
        const auto rva = uint32_t(address - _imageBase);
        for (const auto& section : _sections) {
            const auto span = std::max(section.virtualSize, section.rawSize);
            if (rva < section.virtualAddress || rva + size > section.virtualAddress + span)
                continue;
            const auto offset = section.rawOffset + (rva - section.virtualAddress);
            if (offset + size > section.rawSize)
                return nullptr;
            return _bytes.data() + offset;
        }
        return nullptr;
    }

private:
    template <typename T>
    T Read(std::size_t offset) const {
        T value {};
        std::memcpy(&value, _bytes.data() + offset, sizeof(T));
        return value;
    }

    std::vector<uint8_t> _bytes;
    std::vector<Section> _sections;
    uint32_t _imageBase = 0;
};

template <std::size_t Size>
bool Match(const Image& image, uintptr_t address, const uint8_t (&bytes)[Size]) {
    const auto code = image.At(address, Size);
    return code && std::memcmp(code, bytes, Size) == 0;
}

// 解析 E8 rel32 调用指令，非 E8 或不可读时返回 0
uintptr_t CallTarget(const Image& image, uintptr_t address) {
    const auto code = image.At(address, 5);
    if (!code || code[0] != 0xe8)
        return 0;
    return ARel32CallTarget(address, code);
}

// 解析 E9 rel32 跳转指令，非 E9 或不可读时返回 0
uintptr_t JumpTarget(const Image& image, uintptr_t address) {
    const auto code = image.At(address, 5);
    if (!code || code[0] != 0xe9)
        return 0;
    int32_t offset = 0;
    std::memcpy(&offset, code + 1, sizeof(offset));
    return address + 5 + uintptr_t(intptr_t(offset));
}

uint32_t Immediate(const Image& image, uintptr_t address) {
    const auto code = image.At(address, 4);
    if (!code)
        return 0;
    uint32_t value = 0;
    std::memcpy(&value, code, sizeof(value));
    return value;
}

uint32_t ByteAt(const Image& image, uintptr_t address) {
    const auto code = image.At(address, 1);
    return code ? code[0] : 0x100;
}

void CheckImage(const Image& image) {
    static constexpr uint8_t seed[] = {0x85, 0xc9, 0x75, 0x05, 0xb9, 0x05, 0x11, 0x00, 0x00, 0x89, 0x08,
        0xc7, 0x80, 0xc0, 0x09, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
    static constexpr uint8_t globalInit[] = {0xb9, 0x05, 0x11, 0x00, 0x00, 0xb8, 0x10, 0xa9, 0x75, 0x00};
    static constexpr uint8_t srand[] = {0xe8, 0xbe, 0xa9, 0x00, 0x00, 0x8b, 0x4c, 0x24, 0x04, 0x89, 0x48, 0x14, 0xc3};
    static constexpr uint8_t rand[] = {0xe8, 0xb1, 0xa9, 0x00, 0x00, 0x8b, 0x48, 0x14, 0x69, 0xc9, 0xfd, 0x43, 0x03, 0x00};
    // MT19937 构造函数：种子为 0 时换成默认种子，写入首字，游标置 1
    Check(Match(image, AMTRandImage::SEED_FUNCTION, seed), "SEED_FUNCTION must be MTRand::MTRand(uint32_t)");
    // mov ecx, DEFAULT_SEED 的立即数在构造函数第 6 个字节
    CheckEqual(Immediate(image, AMTRandImage::SEED_FUNCTION + 5), AMTRand::DEFAULT_SEED,
        "SEED_FUNCTION must embed AMTRand::DEFAULT_SEED");
    // mov dword ptr [eax + CURSOR_OFFSET], 1 的位移在构造函数第 14 个字节
    CheckEqual(Immediate(image, AMTRandImage::SEED_FUNCTION + 0xd), AMTRand::CURSOR_OFFSET,
        "SEED_FUNCTION must embed AMTRand::CURSOR_OFFSET");
    // 引擎内部走跳板，四个构造点都调用跳板
    CheckEqual(JumpTarget(image, AMTRandImage::SEED_THUNK), AMTRandImage::SEED_FUNCTION,
        "SEED_THUNK must jump to SEED_FUNCTION");
    CheckEqual(AMTRandImage::SEED_CALL_SITES.size(), 4, "the engine has exactly four MT construction sites");
    for (auto site : AMTRandImage::SEED_CALL_SITES)
        CheckEqual(CallTarget(image, site), AMTRandImage::SEED_THUNK, "MT construction site must call SEED_THUNK");
    // 全局实例的静态初始化与包装函数
    Check(Match(image, AMTRandImage::GLOBAL_INITIALIZER, globalInit),
        "GLOBAL_INITIALIZER must build the global instance with the default seed");
    CheckEqual(Immediate(image, AMTRandImage::GLOBAL_INITIALIZER + 1), AMTRand::DEFAULT_SEED,
        "GLOBAL_INITIALIZER must embed AMTRand::DEFAULT_SEED");
    CheckEqual(Immediate(image, AMTRandImage::GLOBAL_INITIALIZER + 6), AMTRand::INSTANCE,
        "GLOBAL_INITIALIZER must embed AMTRand::INSTANCE");
    CheckEqual(CallTarget(image, AMTRandImage::GLOBAL_INITIALIZER + 10), AMTRandImage::SEED_FUNCTION,
        "GLOBAL_INITIALIZER must call SEED_FUNCTION");
    CheckEqual(Immediate(image, AMTRandImage::GLOBAL_WRAPPER + 1), AMTRand::INSTANCE,
        "GLOBAL_WRAPPER must embed AMTRand::INSTANCE");
    // 引擎 CRT 的 rand / srand，两者都经 _getptd() 读写 +0x14
    Check(Match(image, AEngineCrtRand::SRAND, srand), "srand must store its argument into _getptd() + STATE_OFFSET");
    Check(Match(image, AEngineCrtRand::RAND, rand), "rand must advance the _getptd() + STATE_OFFSET word");
    CheckEqual(CallTarget(image, AEngineCrtRand::SRAND), AEngineCrtRand::GETPTD, "srand must call _getptd");
    CheckEqual(CallTarget(image, AEngineCrtRand::RAND), AEngineCrtRand::GETPTD, "rand must call _getptd");
    CheckEqual(ByteAt(image, AEngineCrtRand::SRAND + 11), AEngineCrtRand::STATE_OFFSET,
        "srand must embed AEngineCrtRand::STATE_OFFSET");
    CheckEqual(ByteAt(image, AEngineCrtRand::RAND + 7), AEngineCrtRand::STATE_OFFSET,
        "rand must embed AEngineCrtRand::STATE_OFFSET");
    // App 两个计数的偏移必须等于 APvzBase 访问器给出的偏移
    std::vector<uint8_t> memory(0x900, 0);
    auto* app = reinterpret_cast<APvzBase*>(memory.data());
    const auto base = reinterpret_cast<uintptr_t>(memory.data());
    const auto mjClock = uint32_t(reinterpret_cast<uintptr_t>(&app->MjClock()) - base);
    const auto updateCount = uint32_t(reinterpret_cast<uintptr_t>(&app->AppUpdateCount()) - base);
    CheckEqual(Immediate(image, 0x4526e6 + 2), mjClock,
        "LawnApp::UpdateFrames must increment the field APvzBase::MjClock reads");
    CheckEqual(Immediate(image, 0x44ec2a + 2), mjClock,
        "the LawnApp constructor must zero the field APvzBase::MjClock reads");
    CheckEqual(Immediate(image, 0x54ec60 + 2), updateCount,
        "the update delta must read the field APvzBase::AppUpdateCount reads");
    Check(std::memcmp(image.At(0x4526e6, 2), "\x01\xaf", 2) == 0 && std::memcmp(image.At(0x44ec2a, 2), "\x89\x9e", 2) == 0,
        "the two App clock instructions must keep their documented opcodes");
    CheckEqual(sizeof(AMTRand), uintptr_t(AMTRand::CURSOR_OFFSET) + 4,
        "one MT19937 object is 624 words plus a cursor");
    // 锁定的镜像文件为 3007800 字节，这里只做“确实是完整镜像”的下界检查
    Check(image.Size() > 0x200000, "the locked engine image must be larger than the minimum extent");
}
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("avz fork selfcheck: constants, layout and offline round trips\n");
    CheckEqual(AMTRand::INSTANCE, 0x75a910, "AMTRand::INSTANCE");
    CheckEqual(AMTRand::CURSOR_OFFSET, 0x9c0, "AMTRand::CURSOR_OFFSET");
    CheckEqual(AMTRand::WORD_COUNT, 624, "AMTRand::WORD_COUNT");
    CheckEqual(AMTRand::DEFAULT_SEED, 0x1105, "AMTRand::DEFAULT_SEED");
    CheckEqual(AMTRandImage::SEED_FUNCTION, 0x5a98d0, "AMTRandImage::SEED_FUNCTION");
    CheckEqual(AMTRandImage::SEED_THUNK, 0x5a9890, "AMTRandImage::SEED_THUNK");
    CheckEqual(AMTRandImage::GLOBAL_INITIALIZER, 0x5a98a0, "AMTRandImage::GLOBAL_INITIALIZER");
    CheckEqual(AMTRandImage::GLOBAL_WRAPPER, 0x5a9930, "AMTRandImage::GLOBAL_WRAPPER");
    CheckEqual(AEngineCrtRand::GETPTD, 0x628a3d, "AEngineCrtRand::GETPTD");
    CheckEqual(AEngineCrtRand::RAND, 0x61e087, "AEngineCrtRand::RAND");
    CheckEqual(AEngineCrtRand::SRAND, 0x61e07a, "AEngineCrtRand::SRAND");
    CheckEqual(AEngineCrtRand::STATE_OFFSET, 0x14, "AEngineCrtRand::STATE_OFFSET");
    CheckEqual(sizeof(AMTRand), 2500, "sizeof(AMTRand)");
    CheckPrimitives();
    CheckAppClockPrimitives();

    if (argc > 1) {
        Image image;
        if (!image.Load(argv[1])) {
            std::printf("FAIL cannot read a 32 bit x86 PE image at %s\n", argv[1]);
            ++failures;
        } else if (image.ImageBase() != 0x400000) {
            std::printf("FAIL unexpected image base 0x%x\n", unsigned(image.ImageBase()));
            ++failures;
        } else {
            std::printf("image: %s (%llu bytes, base 0x%x)\n", argv[1],
                static_cast<unsigned long long>(image.Size()), unsigned(image.ImageBase()));
            CheckImage(image);
        }
    } else {
        std::printf("no image path given: the byte checks against the original engine were skipped\n");
    }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
