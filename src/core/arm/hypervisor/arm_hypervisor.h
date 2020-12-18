// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/arm/arm_interface.h"

#include <Hypervisor/Hypervisor.h>

namespace Core {

class System;

class ARM_Hypervisor final : public ARM_Interface {
public:
    explicit ARM_Hypervisor(System& system, CPUInterrupts& interrupt_handlers, bool uses_wall_clock, std::size_t core_index);
    ~ARM_Hypervisor() override;

    void Initialize() override;

    void SetPC(u64 pc) override;
    u64 GetPC() const override;
    u64 GetReg(int index) const override;
    void SetReg(int index, u64 value) override;
    u128 GetVectorReg(int index) const override;
    void SetVectorReg(int index, u128 value) override;
    u32 GetPSTATE() const override;
    void SetPSTATE(u32 pstate) override;
    VAddr GetTlsAddress() const override;
    void SetTlsAddress(VAddr address) override;
    u64 GetTPIDR_EL0() const override;
    void SetTPIDR_EL0(u64 value) override;
    void ChangeProcessorID(std::size_t new_core_id) override;
    void PrepareReschedule() override;
    void ClearExclusiveState() override;
    void ExecuteInstructions(std::size_t num_instructions);
    void Run() override;
    void Step() override;
    void ExceptionalExit() override;
    void ClearInstructionCache() override;
    void InvalidateCacheRange(VAddr addr, std::size_t size) override;
    void PageTableChanged(Common::PageTable&, std::size_t) override {}

    void SaveContext(ThreadContext32& ctx) override {}
    void SaveContext(ThreadContext64& ctx) override;
    void LoadContext(const ThreadContext32& ctx) override {}
    void LoadContext(const ThreadContext64& ctx) override;

private:
    hv_vcpu_t vcpu;
    hv_vcpu_exit_t* exit_info;

    u64 GetHvReg(hv_reg_t reg) const;
    void SetHvReg(hv_reg_t reg, u64 value);
    u64 GetHvSysReg(hv_sys_reg_t reg) const;
    void SetHvSysReg(hv_sys_reg_t reg, u64 value);
};

} // namespace Core
