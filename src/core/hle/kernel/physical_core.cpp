// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/spin_lock.h"
#include "core/arm/cpu_interrupt_handler.h"
#ifdef ARCHITECTURE_x86_64
#include "core/arm/dynarmic/arm_dynarmic_32.h"
#include "core/arm/dynarmic/arm_dynarmic_64.h"
#endif
#ifdef ARCHITECTURE_ARM64
#include "core/arm/hypervisor/arm_hypervisor.h"
#endif
#include "core/core.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/physical_core.h"
#include "core/hle/kernel/scheduler.h"

namespace Kernel {

PhysicalCore::PhysicalCore(std::size_t core_index, Core::System& system,
                           Kernel::Scheduler& scheduler, Core::CPUInterrupts& interrupts)
    : core_index{core_index}, system{system}, scheduler{scheduler},
      interrupts{interrupts}, guard{std::make_unique<Common::SpinLock>()} {}

PhysicalCore::~PhysicalCore() = default;

void PhysicalCore::Initialize([[maybe_unused]] bool is_64_bit) {
    LOG_ERROR(Kernel, "Initializing core {}", core_index);
    auto& kernel = system.Kernel();
#ifdef ARCHITECTURE_x86_64
    if (is_64_bit) {
        arm_interface = std::make_unique<Core::ARM_Dynarmic_64>(
            system, interrupts, kernel.IsMulticore(), kernel.GetExclusiveMonitor(), core_index);
    } else {
        arm_interface = std::make_unique<Core::ARM_Dynarmic_32>(
            system, interrupts, kernel.IsMulticore(), kernel.GetExclusiveMonitor(), core_index);
    }
#elif ARCHITECTURE_ARM64
    if (is_64_bit) {
        arm_interface = std::make_unique<Core::ARM_Hypervisor>(
            system, interrupts, kernel.IsMulticore(), core_index);
    } else {
        LOG_ERROR(Kernel, "32-bit ARM guest not supported on ARM64 host");
    }
#else
#error Platform not supported yet.
#endif
}

void PhysicalCore::Run() {
    arm_interface->Run();
}

void PhysicalCore::Idle() {
    interrupts[core_index].AwaitInterrupt();
}

void PhysicalCore::Shutdown() {
    scheduler.Shutdown();
}

bool PhysicalCore::IsInterrupted() const {
    return interrupts[core_index].IsInterrupted();
}

void PhysicalCore::Interrupt() {
    guard->lock();
    interrupts[core_index].SetInterrupt(true);
    guard->unlock();
}

void PhysicalCore::ClearInterrupt() {
    guard->lock();
    interrupts[core_index].SetInterrupt(false);
    guard->unlock();
}

} // namespace Kernel
