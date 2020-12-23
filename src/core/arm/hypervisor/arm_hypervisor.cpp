// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "common/assert.h"
#include "common/microprofile.h"
#include "core/arm/hypervisor/arm_hypervisor.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/scheduler.h"
#include "core/hle/kernel/svc.h"
#include "core/memory.h"
#include "core/hle/kernel/memory/page_linked_list.h"
#include "core/device_memory.h"

#include <Hypervisor/Hypervisor.h>

namespace Core {

#define HV_GUARD(func) do { if((func) != HV_SUCCESS) { LOG_ERROR(Core_ARM, "Hypervisor call failed: " #func); UNIMPLEMENTED(); } } while(0)

static ARM_Hypervisor_MemoryObserver* instance; // TODO: Access via PageTable or something


ARM_Hypervisor_MemoryObserver::ARM_Hypervisor_MemoryObserver(Core::System& system) : MemoryObserver(system), physTop(0x10000) {
    instance = this;

    HV_GUARD(hv_vm_create(nullptr));

    HV_GUARD(hv_vm_map(system.DeviceMemory().GetPointer(DramMemoryMap::Base), DramMemoryMap::Base, DramMemoryMap::Size, HV_MEMORY_EXEC | HV_MEMORY_WRITE | HV_MEMORY_READ));

    auto vbaTable = AllocateTable();
    auto vbaPtr = (u32*) vbaTable->table;
    for(auto i = 0; i < 16; ++i)
        for(auto j = 0; j < 32; ++j)
            vbaPtr[i * 32 + j] = 0xd4200000 | (i << 5); // BRK 0...15

    pageTableBase = AllocateTable();

    vbaBase = 0x1000; // TODO: We should be reserving this so the system doesn't think it's open!
    auto entry = GetEntryFor(vbaBase);
    // Set VBA to EL1 read+execute
    *entry = vbaTable->physAddr | 0b11 | (1 << 10) | (0b11 << 8) | (1ULL << 54);
}

ARM_Hypervisor_MemoryObserver::~ARM_Hypervisor_MemoryObserver() {
    HV_GUARD(hv_vm_destroy());

    for (auto ptr : allocatedPhysPages)
        free(ptr);
}

u64 ARM_Hypervisor_MemoryObserver::FindOpenPage() {
    if (openPhysPages.empty()) {
        auto addr = physTop;
        physTop += 0x4000;
        return addr;
    } else {
        auto addr = openPhysPages.front();
        openPhysPages.pop();
        return addr;
    }
}

TablePointer* ARM_Hypervisor_MemoryObserver::AllocateTable() {
    if (freeTables.empty()) {
        auto page = aligned_alloc(0x4000, 0x4000);
        auto physAddr = FindOpenPage();
        HV_GUARD(hv_vm_map(page, physAddr, 0x4000, HV_MEMORY_EXEC | HV_MEMORY_WRITE | HV_MEMORY_READ));
        allocatedPhysPages.push_back(page);
        for (auto i = 0; i < 4; ++i) {
            TablePointer* tp = new TablePointer;
            tp->table = (u64*) (((u8*) page) + 0x1000 * i);
            tp->physAddr = physAddr + 0x1000 * i;
            allocatedPhysPages.push_back((void*) tp); // TODO: Unhack this
            freeTables.push(tp);
        }
    }
    auto table = freeTables.front();
    freeTables.pop();
    memset(table->table, 0, 0x1000);
    return table;
}

u64* ARM_Hypervisor_MemoryObserver::GetEntryFor(VAddr addr) {
    auto l1Index = (addr >> 30) & 0x1FF;
    auto l2Index = (addr >> 21) & 0x1FF;
    auto l3Index = (addr >> 12) & 0x1FF;

    if (pageTableBase->table[l1Index] == 0) {
        auto table = pageTableBase->subtables[l1Index] = AllocateTable();
        pageTableBase->table[l1Index] = table->physAddr | 0b11;
    }
    auto nt = pageTableBase->subtables[l1Index];

    if (nt->table[l2Index] == 0) {
        auto table = nt->subtables[l2Index] = AllocateTable();
        nt->table[l2Index] = table->physAddr | 0b11;
    }
    nt = nt->subtables[l2Index];

    return &nt->table[l3Index];
}

void ARM_Hypervisor_MemoryObserver::Allocated(const PageLinkedList& page_list, VAddr addr, MemoryPermission perm) {
    u64 flags = 0b11 | (1 << 10) | (0b11 << 8) | (1ULL << 53);
    if ((perm & MemoryPermission::Execute) != MemoryPermission::Execute)
        flags |= 1ULL << 54;
    if ((perm & MemoryPermission::Write) == MemoryPermission::Write)
        flags |= 0b01 << 6;
    else if ((perm & MemoryPermission::Read) == MemoryPermission::Read)
        flags |= 0b11 << 6;

    for (const auto& it : page_list.Nodes()) {
        auto paddr = it.GetAddress();
        LOG_ERROR(Core_ARM, "Attempting to map guest paddr 0x{:x} to vaddr 0x{:x} -- perms 0x{:x} !!!", paddr, addr, perm);
        for (std::size_t i = 0; i < it.GetNumPages(); ++i, addr += 0x1000, paddr += 0x1000) {
            *GetEntryFor(addr) = paddr | flags;
        }
    }
}

void ARM_Hypervisor_MemoryObserver::Mapped(u64 paddr, VAddr addr, size_t num_pages, MemoryPermission perm) {
    u64 flags = 0b11 | (1 << 10) | (0b11 << 8) | (1ULL << 53);
    if ((perm & MemoryPermission::Execute) != MemoryPermission::Execute)
        flags |= 1ULL << 54;
    if ((perm & MemoryPermission::Write) == MemoryPermission::Write)
        flags |= 0b01 << 6;
    else if ((perm & MemoryPermission::Read) == MemoryPermission::Read)
        flags |= 0b11 << 6;

    LOG_ERROR(Core_ARM, "Attempting to map guest paddr 0x{:x} to vaddr 0x{:x} -- perms 0x{:x} !!!", paddr, addr, perm);
    while (num_pages-- != 0) {
        *GetEntryFor(addr) = paddr | flags;
        addr += 0x1000;
        paddr += 0x1000;
    }
}

void ARM_Hypervisor_MemoryObserver::PermissionsChanged(VAddr addr, std::size_t num_pages, MemoryPermission perm) {
    u64 flags = 0b11 | (1 << 10) | (0b11 << 8) | (1ULL << 53);
    if ((perm & MemoryPermission::Execute) != MemoryPermission::Execute)
        flags |= 1ULL << 54;
    if ((perm & MemoryPermission::Write) == MemoryPermission::Write)
        flags |= 0b01 << 6;
    else if ((perm & MemoryPermission::Read) == MemoryPermission::Read)
        flags |= 0b11 << 6;

    auto mask = (1ULL << 54) | (0b11 << 6);
    LOG_ERROR(Core_ARM, "Attempting to modify perms: vaddr 0x{:x} -- perms 0x{:x} !!!", addr, perm);
    // TODO: We shouldn't pull an entry for every single one, since they're contiguous
    for (std::size_t i = 0; i < num_pages; ++i, addr += 0x1000) {
        auto entry = GetEntryFor(addr);
        *entry = (*entry & ~mask) | flags;
    }
}

void ARM_Hypervisor_MemoryObserver::Freed(const PageLinkedList& page_list, VAddr addr) {
}


ARM_Hypervisor::ARM_Hypervisor(System& system, CPUInterrupts& interrupt_handlers, bool uses_wall_clock, std::size_t core_index)
    : ARM_Interface{system, interrupt_handlers, uses_wall_clock} { }

ARM_Hypervisor::~ARM_Hypervisor() {
}

void ARM_Hypervisor::Initialize() {
    LOG_ERROR(Core_ARM, "Initializing VCPU");
    HV_GUARD(hv_vcpu_create(&vcpu, &exit_info, nullptr));

    HV_GUARD(hv_vcpu_set_trap_debug_exceptions(vcpu, true));
    HV_GUARD(hv_vcpu_set_trap_debug_reg_accesses(vcpu, true));

    u64 tcr = 0;
    tcr |= 0b001ULL << 32;    // 36-bit IPA space
    tcr |= 0b10ULL << 30;     // Granule size for TTBR1_EL1: 4k
    tcr |= 0b11ULL << 28;     // Inner sharable
    tcr |= 0b01ULL << 26;     // Cachable
    tcr |= 0b01ULL << 24;     // Cachable
    tcr |= 0b011001ULL << 16; // Memory region 2^(64-24)
    tcr |= 0b00ULL << 14;     // Granule size for TTBR0_EL1: 4k
    tcr |= 0b11ULL << 12;     // Inner sharable
    tcr |= 0b01ULL << 10;     // Cachable
    tcr |= 0b01ULL << 8;      // Cachable
    tcr |= 0b011001ULL;       // Memory region 2^(64-24)
    SetHvSysReg(HV_SYS_REG_TCR_EL1, tcr);

    SetHvSysReg(HV_SYS_REG_MAIR_EL1, 0xFF);
    SetHvSysReg(HV_SYS_REG_TTBR0_EL1, instance->pageTableBase->physAddr);
    SetHvSysReg(HV_SYS_REG_TTBR1_EL1, instance->pageTableBase->physAddr);
    SetHvSysReg(HV_SYS_REG_SCTLR_EL1, 1 | (1 << 26));

    SetHvSysReg(HV_SYS_REG_CPACR_EL1, 3 << 20);

    SetHvSysReg(HV_SYS_REG_VBAR_EL1, instance->vbaBase);
}

u64 ARM_Hypervisor::GetHvReg(hv_reg_t reg) const {
    u64 value;
    HV_GUARD(hv_vcpu_get_reg(vcpu, reg, &value));
    return value;
}

void ARM_Hypervisor::SetHvReg(hv_reg_t reg, u64 value) {
    HV_GUARD(hv_vcpu_set_reg(vcpu, reg, value));
}

u64 ARM_Hypervisor::GetHvSysReg(hv_sys_reg_t reg) const {
    u64 value;
    HV_GUARD(hv_vcpu_get_sys_reg(vcpu, reg, &value));
    return value;
}

void ARM_Hypervisor::SetHvSysReg(hv_sys_reg_t reg, u64 value) {
    HV_GUARD(hv_vcpu_set_sys_reg(vcpu, reg, value));
}

void ARM_Hypervisor::SetPC(u64 pc) {
    SetHvReg(HV_REG_PC, pc);
}

u64 ARM_Hypervisor::GetPC() const {
    return GetHvReg(HV_REG_PC);
}

u64 ARM_Hypervisor::GetReg(int index) const {
    u64 value;
    if (index == 31)
        value = GetHvSysReg(HV_SYS_REG_SP_EL0);
    else
        value = GetHvReg((hv_reg_t) (HV_REG_X0 + index));
    //LOG_CRITICAL(Core_ARM, "Getting reg {} -- 0x{:X}", index, value);
    return value;
}

void ARM_Hypervisor::SetReg(int index, u64 value) {
    //LOG_CRITICAL(Core_ARM, "Setting reg {} -- 0x{:X}", index, value);
    if (index == 31)
        SetHvSysReg(HV_SYS_REG_SP_EL0, value);
    else
        SetHvReg((hv_reg_t) (HV_REG_X0 + index), value);
}

u128 ARM_Hypervisor::GetVectorReg(int index) const {
    u128 value;
    HV_GUARD(hv_vcpu_get_simd_fp_reg(vcpu, (hv_simd_fp_reg_t) (HV_SIMD_FP_REG_Q0 + index), (hv_simd_fp_uchar16_t*) &value));
    return value;
}

void ARM_Hypervisor::SetVectorReg(int index, u128 value) {
    HV_GUARD(hv_vcpu_set_simd_fp_reg(vcpu, (hv_simd_fp_reg_t) (HV_SIMD_FP_REG_Q0 + index), *(hv_simd_fp_uchar16_t*) &value));
}

u32 ARM_Hypervisor::GetPSTATE() const {
    return (u32) GetHvReg(HV_REG_CPSR);
}

void ARM_Hypervisor::SetPSTATE(u32 pstate) {
    SetHvReg(HV_REG_CPSR, pstate);
}

VAddr ARM_Hypervisor::GetTlsAddress() const {
    return GetHvSysReg(HV_SYS_REG_TPIDRRO_EL0);
}

void ARM_Hypervisor::SetTlsAddress(VAddr address) {
    SetHvSysReg(HV_SYS_REG_TPIDRRO_EL0, address);
}

u64 ARM_Hypervisor::GetTPIDR_EL0() const {
    return GetHvSysReg(HV_SYS_REG_TPIDR_EL0);
}

void ARM_Hypervisor::SetTPIDR_EL0(u64 value) {
    SetHvSysReg(HV_SYS_REG_TPIDR_EL0, value);
}

void ARM_Hypervisor::ChangeProcessorID(std::size_t new_core_id) {
}

void ARM_Hypervisor::PrepareReschedule() {
}

void ARM_Hypervisor::ClearExclusiveState() {
}

void ARM_Hypervisor::Run() {
    while (true) {
        /*LOG_CRITICAL(Core_ARM, "Core {} thread {} -- RUNNING FROM 0x{:x}",
            system.CurrentCoreIndex(),
            system.CurrentScheduler().GetCurrentThread()->GetThreadID(),
            GetPC());*/
        HV_GUARD(hv_vcpu_run(vcpu));

        switch (exit_info->reason) {
        case HV_EXIT_REASON_CANCELED:
            //LOG_CRITICAL(Core_ARM, "Exit requested -- PC 0x{:x}", GetPC());
            return;
        case HV_EXIT_REASON_EXCEPTION: {
            auto elr = GetHvSysReg(HV_SYS_REG_ELR_EL1);
            /*LOG_CRITICAL(Core_ARM, "Core {} thread {} -- EXITED FROM 0x{:x}",
                system.CurrentCoreIndex(),
                system.CurrentScheduler().GetCurrentThread()->GetThreadID(),
                elr);*/
            //LOG_CRITICAL(Core_ARM, "LR 0x{:x}", GetHvReg(HV_REG_LR));

            auto esr = GetHvSysReg(HV_SYS_REG_ESR_EL1);
            auto ec = esr >> 26;
            switch (ec) {
            case 0b011000: { // MSR/MRS trap
                auto masked = esr & ~(0b11111 << 5);
                auto rt = (int) ((esr >> 5) & 0b11111);
                switch (masked) {
                case 0x6232f801: // Read CNTPCT_EL0
                    SetReg(rt, system.CoreTiming().GetClockTicks());
                    break;
                case 0x6232c001: // Read CTR_EL0
                    SetReg(rt, 0x8444c004);
                    break;
                default:
                    LOG_CRITICAL(Core_ARM, "Unsupported MSR/MRS. Masked ESR 0x{:x}", masked);
                    LOG_CRITICAL(Core_ARM, "Instruction: 0x{:x}", system.Memory().Read32(elr));
                    UNIMPLEMENTED();
                }
                SetHvReg(HV_REG_CPSR, GetHvSysReg(HV_SYS_REG_SPSR_EL1));
                SetPC(elr + 4);
                break;
            }
            case 0b100000: // Insn abort
                LOG_CRITICAL(Core_ARM, "Instruction abort");
                UNIMPLEMENTED();
            case 0b100100: { // Data abort
                auto far = GetHvSysReg(HV_SYS_REG_FAR_EL1);
                LOG_CRITICAL(Core_ARM, "Data abort accessing 0x{:x}", far);
                UNIMPLEMENTED();
            }
            case 0b010101: // SVC
                /*LOG_CRITICAL(Core_ARM, "Core {} thread {} -- Got SVC 0x{:x}",
                    system.CurrentCoreIndex(),
                    system.CurrentScheduler().GetCurrentThread()->GetThreadID(),
                    esr & 0xFFFF);*/
                SetHvReg(HV_REG_CPSR, GetHvSysReg(HV_SYS_REG_SPSR_EL1));
                SetPC(elr);
                Kernel::Svc::Call(system, esr & 0xFFFF);
                break;
            default:
                LOG_CRITICAL(Core_ARM, "Unhandled exception code: 0x{:x}", ec);
                UNIMPLEMENTED();
            }
            break;
        }
        default:
            LOG_CRITICAL(Core_ARM, "Unknown exit reason: 0x{:x}", exit_info->reason);
            UNIMPLEMENTED();
        }
    }
}

void ARM_Hypervisor::Step() {
}

void ARM_Hypervisor::ExceptionalExit() {
    //HV_GUARD(hv_vcpus_exit(&vcpu, 1));
}

void ARM_Hypervisor::ClearInstructionCache() {
}

void ARM_Hypervisor::InvalidateCacheRange(VAddr addr, std::size_t size) {
}

void ARM_Hypervisor::SaveContext(ThreadContext64& ctx) {
    //LOG_CRITICAL(Core_ARM, "Saving context");
    for (auto i = 0; i < 31; ++i)
        ctx.cpu_registers[i] = GetHvReg((hv_reg_t) (HV_REG_X0 + i));
    ctx.sp = GetHvSysReg(HV_SYS_REG_SP_EL0);
    ctx.pc = GetHvReg(HV_REG_PC);
    ctx.pstate = (u32) GetHvReg(HV_REG_CPSR);
    for (auto i = 0; i < 32; ++i)
        HV_GUARD(hv_vcpu_get_simd_fp_reg(vcpu, (hv_simd_fp_reg_t) (HV_SIMD_FP_REG_Q0 + i), (hv_simd_fp_uchar16_t*) &ctx.vector_registers[i]));
    ctx.fpcr = (u32) GetHvReg(HV_REG_FPCR);
    ctx.fpsr = (u32) GetHvReg(HV_REG_FPSR);
    ctx.tpidr = GetHvSysReg(HV_SYS_REG_TPIDR_EL0);
}

void ARM_Hypervisor::LoadContext(const ThreadContext64& ctx) {
    //LOG_CRITICAL(Core_ARM, "Loading context");
    for(auto i = 0; i < 31; ++i)
        SetHvReg((hv_reg_t) (HV_REG_X0 + i), ctx.cpu_registers[i]);
    SetHvSysReg(HV_SYS_REG_SP_EL0, ctx.sp);
    SetHvReg(HV_REG_PC, ctx.pc);
    SetHvReg(HV_REG_CPSR, ctx.pstate);
    for (auto i = 0; i < 32; ++i)
        HV_GUARD(hv_vcpu_set_simd_fp_reg(vcpu, (hv_simd_fp_reg_t) (HV_SIMD_FP_REG_Q0 + i), *(hv_simd_fp_uchar16_t*) &ctx.vector_registers[i]));
    SetHvReg(HV_REG_FPCR, ctx.fpcr);
    SetHvReg(HV_REG_FPSR, ctx.fpsr);
    SetHvSysReg(HV_SYS_REG_TPIDR_EL0, ctx.tpidr);
}

}
