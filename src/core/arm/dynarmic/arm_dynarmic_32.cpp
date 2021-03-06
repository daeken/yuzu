// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cinttypes>
#include <memory>
#include <dynarmic/A32/a32.h>
#include <dynarmic/A32/config.h>
#include <dynarmic/A32/context.h>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/page_table.h"
#include "core/arm/cpu_interrupt_handler.h"
#include "core/arm/dynarmic/arm_dynarmic_32.h"
#include "core/arm/dynarmic/arm_dynarmic_cp15.h"
#include "core/arm/dynarmic/arm_exclusive_monitor.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/svc.h"
#include "core/memory.h"
#include "core/settings.h"

namespace Core {

class DynarmicCallbacks32 : public Dynarmic::A32::UserCallbacks {
public:
    explicit DynarmicCallbacks32(ARM_Dynarmic_32& parent) : parent(parent) {}

    u8 MemoryRead8(u32 vaddr) override {
        return parent.system.Memory().Read8(vaddr);
    }
    u16 MemoryRead16(u32 vaddr) override {
        return parent.system.Memory().Read16(vaddr);
    }
    u32 MemoryRead32(u32 vaddr) override {
        return parent.system.Memory().Read32(vaddr);
    }
    u64 MemoryRead64(u32 vaddr) override {
        return parent.system.Memory().Read64(vaddr);
    }

    void MemoryWrite8(u32 vaddr, u8 value) override {
        parent.system.Memory().Write8(vaddr, value);
    }
    void MemoryWrite16(u32 vaddr, u16 value) override {
        parent.system.Memory().Write16(vaddr, value);
    }
    void MemoryWrite32(u32 vaddr, u32 value) override {
        parent.system.Memory().Write32(vaddr, value);
    }
    void MemoryWrite64(u32 vaddr, u64 value) override {
        parent.system.Memory().Write64(vaddr, value);
    }

    bool MemoryWriteExclusive8(u32 vaddr, u8 value, u8 expected) override {
        return parent.system.Memory().WriteExclusive8(vaddr, value, expected);
    }
    bool MemoryWriteExclusive16(u32 vaddr, u16 value, u16 expected) override {
        return parent.system.Memory().WriteExclusive16(vaddr, value, expected);
    }
    bool MemoryWriteExclusive32(u32 vaddr, u32 value, u32 expected) override {
        return parent.system.Memory().WriteExclusive32(vaddr, value, expected);
    }
    bool MemoryWriteExclusive64(u32 vaddr, u64 value, u64 expected) override {
        return parent.system.Memory().WriteExclusive64(vaddr, value, expected);
    }

    void InterpreterFallback(u32 pc, std::size_t num_instructions) override {
        UNIMPLEMENTED_MSG("This should never happen, pc = {:08X}, code = {:08X}", pc,
                          MemoryReadCode(pc));
    }

    void ExceptionRaised(u32 pc, Dynarmic::A32::Exception exception) override {
        switch (exception) {
        case Dynarmic::A32::Exception::UndefinedInstruction:
        case Dynarmic::A32::Exception::UnpredictableInstruction:
            break;
        case Dynarmic::A32::Exception::Breakpoint:
            break;
        }
        LOG_CRITICAL(Core_ARM, "ExceptionRaised(exception = {}, pc = {:08X}, code = {:08X})",
                     static_cast<std::size_t>(exception), pc, MemoryReadCode(pc));
        UNIMPLEMENTED();
    }

    void CallSVC(u32 swi) override {
        Kernel::Svc::Call(parent.system, swi);
    }

    void AddTicks(u64 ticks) override {
        if (parent.uses_wall_clock) {
            return;
        }
        // Divide the number of ticks by the amount of CPU cores. TODO(Subv): This yields only a
        // rough approximation of the amount of executed ticks in the system, it may be thrown off
        // if not all cores are doing a similar amount of work. Instead of doing this, we should
        // device a way so that timing is consistent across all cores without increasing the ticks 4
        // times.
        u64 amortized_ticks =
            (ticks - num_interpreted_instructions) / Core::Hardware::NUM_CPU_CORES;
        // Always execute at least one tick.
        amortized_ticks = std::max<u64>(amortized_ticks, 1);

        parent.system.CoreTiming().AddTicks(amortized_ticks);
        num_interpreted_instructions = 0;
    }

    u64 GetTicksRemaining() override {
        if (parent.uses_wall_clock) {
            if (!parent.interrupt_handlers[parent.core_index].IsInterrupted()) {
                return minimum_run_cycles;
            }
            return 0U;
        }
        return std::max<s64>(parent.system.CoreTiming().GetDowncount(), 0);
    }

    ARM_Dynarmic_32& parent;
    std::size_t num_interpreted_instructions{};
    static constexpr u64 minimum_run_cycles = 1000U;
};

std::shared_ptr<Dynarmic::A32::Jit> ARM_Dynarmic_32::MakeJit(Common::PageTable& page_table,
                                                             std::size_t address_space_bits) const {
    Dynarmic::A32::UserConfig config;
    config.callbacks = cb.get();
    // TODO(bunnei): Implement page table for 32-bit
    // config.page_table = &page_table.pointers;
    config.coprocessors[15] = cp15;
    config.define_unpredictable_behaviour = true;
    static constexpr std::size_t PAGE_BITS = 12;
    static constexpr std::size_t NUM_PAGE_TABLE_ENTRIES = 1 << (32 - PAGE_BITS);
    config.page_table = reinterpret_cast<std::array<std::uint8_t*, NUM_PAGE_TABLE_ENTRIES>*>(
        page_table.pointers.data());
    config.absolute_offset_page_table = true;
    config.detect_misaligned_access_via_page_table = 16 | 32 | 64 | 128;
    config.only_detect_misalignment_via_page_table_on_page_boundary = true;

    // Multi-process state
    config.processor_id = core_index;
    config.global_monitor = &exclusive_monitor.monitor;

    // Timing
    config.wall_clock_cntpct = uses_wall_clock;

    // Safe optimizations
    if (Settings::values.cpu_accuracy == Settings::CPUAccuracy::DebugMode) {
        if (!Settings::values.cpuopt_page_tables) {
            config.page_table = nullptr;
        }
        if (!Settings::values.cpuopt_block_linking) {
            config.optimizations &= ~Dynarmic::OptimizationFlag::BlockLinking;
        }
        if (!Settings::values.cpuopt_return_stack_buffer) {
            config.optimizations &= ~Dynarmic::OptimizationFlag::ReturnStackBuffer;
        }
        if (!Settings::values.cpuopt_fast_dispatcher) {
            config.optimizations &= ~Dynarmic::OptimizationFlag::FastDispatch;
        }
        if (!Settings::values.cpuopt_context_elimination) {
            config.optimizations &= ~Dynarmic::OptimizationFlag::GetSetElimination;
        }
        if (!Settings::values.cpuopt_const_prop) {
            config.optimizations &= ~Dynarmic::OptimizationFlag::ConstProp;
        }
        if (!Settings::values.cpuopt_misc_ir) {
            config.optimizations &= ~Dynarmic::OptimizationFlag::MiscIROpt;
        }
        if (!Settings::values.cpuopt_reduce_misalign_checks) {
            config.only_detect_misalignment_via_page_table_on_page_boundary = false;
        }
    }

    // Unsafe optimizations
    if (Settings::values.cpu_accuracy == Settings::CPUAccuracy::Unsafe) {
        config.unsafe_optimizations = true;
        if (Settings::values.cpuopt_unsafe_unfuse_fma) {
            config.optimizations |= Dynarmic::OptimizationFlag::Unsafe_UnfuseFMA;
        }
        if (Settings::values.cpuopt_unsafe_reduce_fp_error) {
            config.optimizations |= Dynarmic::OptimizationFlag::Unsafe_ReducedErrorFP;
        }
    }

    return std::make_unique<Dynarmic::A32::Jit>(config);
}

void ARM_Dynarmic_32::Run() {
    jit->Run();
}

void ARM_Dynarmic_32::ExceptionalExit() {
    jit->ExceptionalExit();
}

void ARM_Dynarmic_32::Step() {
    jit->Step();
}

ARM_Dynarmic_32::ARM_Dynarmic_32(System& system, CPUInterrupts& interrupt_handlers,
                                 bool uses_wall_clock, ExclusiveMonitor& exclusive_monitor,
                                 std::size_t core_index)
    : ARM_Interface{system, interrupt_handlers, uses_wall_clock},
      cb(std::make_unique<DynarmicCallbacks32>(*this)),
      cp15(std::make_shared<DynarmicCP15>(*this)), core_index{core_index},
      exclusive_monitor{dynamic_cast<DynarmicExclusiveMonitor&>(exclusive_monitor)} {}

ARM_Dynarmic_32::~ARM_Dynarmic_32() = default;

void ARM_Dynarmic_32::SetPC(u64 pc) {
    jit->Regs()[15] = static_cast<u32>(pc);
}

u64 ARM_Dynarmic_32::GetPC() const {
    return jit->Regs()[15];
}

u64 ARM_Dynarmic_32::GetReg(int index) const {
    return jit->Regs()[index];
}

void ARM_Dynarmic_32::SetReg(int index, u64 value) {
    jit->Regs()[index] = static_cast<u32>(value);
}

u128 ARM_Dynarmic_32::GetVectorReg(int index) const {
    return {};
}

void ARM_Dynarmic_32::SetVectorReg(int index, u128 value) {}

u32 ARM_Dynarmic_32::GetPSTATE() const {
    return jit->Cpsr();
}

void ARM_Dynarmic_32::SetPSTATE(u32 cpsr) {
    jit->SetCpsr(cpsr);
}

u64 ARM_Dynarmic_32::GetTlsAddress() const {
    return cp15->uro;
}

void ARM_Dynarmic_32::SetTlsAddress(VAddr address) {
    cp15->uro = static_cast<u32>(address);
}

u64 ARM_Dynarmic_32::GetTPIDR_EL0() const {
    return cp15->uprw;
}

void ARM_Dynarmic_32::SetTPIDR_EL0(u64 value) {
    cp15->uprw = static_cast<u32>(value);
}

void ARM_Dynarmic_32::ChangeProcessorID(std::size_t new_core_id) {
    jit->ChangeProcessorID(new_core_id);
}

void ARM_Dynarmic_32::SaveContext(ThreadContext32& ctx) {
    Dynarmic::A32::Context context;
    jit->SaveContext(context);
    ctx.cpu_registers = context.Regs();
    ctx.extension_registers = context.ExtRegs();
    ctx.cpsr = context.Cpsr();
    ctx.fpscr = context.Fpscr();
}

void ARM_Dynarmic_32::LoadContext(const ThreadContext32& ctx) {
    Dynarmic::A32::Context context;
    context.Regs() = ctx.cpu_registers;
    context.ExtRegs() = ctx.extension_registers;
    context.SetCpsr(ctx.cpsr);
    context.SetFpscr(ctx.fpscr);
    jit->LoadContext(context);
}

void ARM_Dynarmic_32::PrepareReschedule() {
    jit->HaltExecution();
}

void ARM_Dynarmic_32::ClearInstructionCache() {
    if (!jit) {
        return;
    }
    jit->ClearCache();
}

void ARM_Dynarmic_32::InvalidateCacheRange(VAddr addr, std::size_t size) {
    if (!jit) {
        return;
    }
    jit->InvalidateCacheRange(static_cast<u32>(addr), size);
}

void ARM_Dynarmic_32::ClearExclusiveState() {
    jit->ClearExclusiveState();
}

void ARM_Dynarmic_32::PageTableChanged(Common::PageTable& page_table,
                                       std::size_t new_address_space_size_in_bits) {
    auto key = std::make_pair(&page_table, new_address_space_size_in_bits);
    auto iter = jit_cache.find(key);
    if (iter != jit_cache.end()) {
        jit = iter->second;
        return;
    }
    jit = MakeJit(page_table, new_address_space_size_in_bits);
    jit_cache.emplace(key, jit);
}

} // namespace Core
