// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cinttypes>
#include <memory>
#include "core/arm/hypervisor/hypervisor_exclusive_monitor.h"
#include "core/memory.h"
#include "common/assert.h"

namespace Core {

HypervisorExclusiveMonitor::HypervisorExclusiveMonitor(Memory::Memory& memory, std::size_t core_count)
    : memory{memory} {}

HypervisorExclusiveMonitor::~HypervisorExclusiveMonitor() = default;

u8 HypervisorExclusiveMonitor::ExclusiveRead8(std::size_t core_index, VAddr addr) {
    auto ptr = (volatile u8*) memory.GetPointer(addr);
    return __builtin_arm_ldrex(ptr);
}

u16 HypervisorExclusiveMonitor::ExclusiveRead16(std::size_t core_index, VAddr addr) {
    auto ptr = (volatile u16*) memory.GetPointer(addr);
    return __builtin_arm_ldrex(ptr);
}

u32 HypervisorExclusiveMonitor::ExclusiveRead32(std::size_t core_index, VAddr addr) {
    auto ptr = (volatile u32*) memory.GetPointer(addr);
    return __builtin_arm_ldrex(ptr);
}

u64 HypervisorExclusiveMonitor::ExclusiveRead64(std::size_t core_index, VAddr addr) {
    auto ptr = (volatile u64*) memory.GetPointer(addr);
    return __builtin_arm_ldrex(ptr);
}

u128 HypervisorExclusiveMonitor::ExclusiveRead128(std::size_t core_index, VAddr addr) {
    UNIMPLEMENTED();
}

void HypervisorExclusiveMonitor::ClearExclusive() {
    __builtin_arm_clrex();
}

bool HypervisorExclusiveMonitor::ExclusiveWrite8(std::size_t core_index, VAddr vaddr, u8 value) {
    auto ptr = (volatile u8*) memory.GetPointer(vaddr);
    return __builtin_arm_strex(value, ptr);
}

bool HypervisorExclusiveMonitor::ExclusiveWrite16(std::size_t core_index, VAddr vaddr, u16 value) {
    auto ptr = (volatile u16*) memory.GetPointer(vaddr);
    return __builtin_arm_strex(value, ptr);
}

bool HypervisorExclusiveMonitor::ExclusiveWrite32(std::size_t core_index, VAddr vaddr, u32 value) {
    auto ptr = (volatile u32*) memory.GetPointer(vaddr);
    return __builtin_arm_strex(value, ptr);
}

bool HypervisorExclusiveMonitor::ExclusiveWrite64(std::size_t core_index, VAddr vaddr, u64 value) {
    auto ptr = (volatile u64*) memory.GetPointer(vaddr);
    return __builtin_arm_strex(value, ptr);
}

bool HypervisorExclusiveMonitor::ExclusiveWrite128(std::size_t core_index, VAddr vaddr, u128 value) {
    UNIMPLEMENTED();
}

} // namespace Core
