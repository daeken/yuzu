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

#include <Hypervisor/Hypervisor.h>

namespace Core {

#define HV_GUARD(func) do { if((func) != HV_SUCCESS) { LOG_ERROR(Core_ARM, "Hypervisor call failed: " #func); UNIMPLEMENTED(); } } while(0)

ARM_Hypervisor::ARM_Hypervisor(System& system, CPUInterrupts& interrupt_handlers, bool uses_wall_clock, std::size_t core_index)
    : ARM_Interface{system, interrupt_handlers, uses_wall_clock}
{
    static bool initializedVm = false;

    if(!initializedVm) {
        initializedVm = true;
        HV_GUARD(hv_vm_create(nullptr));
    }
}

ARM_Hypervisor::~ARM_Hypervisor() {
}

void ARM_Hypervisor::Initialize() {
    LOG_ERROR(Core_ARM, "Initializing VCPU");
    HV_GUARD(hv_vcpu_create(&vcpu, &exit_info, nullptr));
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
    if (index == 31)
        return GetHvSysReg(HV_SYS_REG_SP_EL0);
    return GetHvReg((hv_reg_t) (HV_REG_X0 + index));
}

void ARM_Hypervisor::SetReg(int index, u64 value) {
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
    LOG_CRITICAL(Core_ARM, "HYPERVISOR IS GOOOOO {:x}", GetPC());
    UNIMPLEMENTED();
}

void ARM_Hypervisor::Step() {
}

void ARM_Hypervisor::ExceptionalExit() {
}

void ARM_Hypervisor::ClearInstructionCache() {
}

void ARM_Hypervisor::InvalidateCacheRange(VAddr addr, std::size_t size) {
}

void ARM_Hypervisor::SaveContext(ThreadContext64& ctx) {
    for(auto i = 0; i < 31; ++i)
        ctx.cpu_registers[i] = GetHvReg((hv_reg_t) (HV_REG_X0 + i));
    ctx.sp = GetHvSysReg(HV_SYS_REG_SP_EL0);
    ctx.pc = GetHvReg(HV_REG_PC);
    ctx.pstate = (u32) GetHvReg(HV_REG_CPSR);
    for(auto i = 0; i < 32; ++i)
        HV_GUARD(hv_vcpu_get_simd_fp_reg(vcpu, (hv_simd_fp_reg_t) (HV_SIMD_FP_REG_Q0 + i), (hv_simd_fp_uchar16_t*) &ctx.vector_registers[i]));
    ctx.fpcr = (u32) GetHvReg(HV_REG_FPCR);
    ctx.fpsr = (u32) GetHvReg(HV_REG_FPSR);
    ctx.tpidr = GetHvSysReg(HV_SYS_REG_TPIDR_EL0);
}

void ARM_Hypervisor::LoadContext(const ThreadContext64& ctx) {
    for(auto i = 0; i < 31; ++i)
        SetHvReg((hv_reg_t) (HV_REG_X0 + i), ctx.cpu_registers[i]);
    SetHvSysReg(HV_SYS_REG_SP_EL0, ctx.sp);
    SetHvReg(HV_REG_PC, ctx.pc);
    SetHvReg(HV_REG_CPSR, ctx.pstate);
    for(auto i = 0; i < 32; ++i)
        HV_GUARD(hv_vcpu_set_simd_fp_reg(vcpu, (hv_simd_fp_reg_t) (HV_SIMD_FP_REG_Q0 + i), *(hv_simd_fp_uchar16_t*) &ctx.vector_registers[i]));
    SetHvReg(HV_REG_FPCR, ctx.fpcr);
    SetHvReg(HV_REG_FPSR, ctx.fpsr);
    SetHvSysReg(HV_SYS_REG_TPIDR_EL0, ctx.tpidr);
}

}
