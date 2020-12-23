// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/telemetry.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/frontend/emu_window.h"
#include "core/settings.h"
#include "core/telemetry_session.h"
#include "video_core/gpu.h"
#include "video_core/renderer_metal/mtl_rasterizer.h"

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

namespace Metal {

RasterizerMetal::RasterizerMetal(Core::Frontend::EmuWindow& emu_window_, Tegra::GPU& gpu_,
                                 Tegra::MemoryManager& gpu_memory_, Core::Memory::Memory& cpu_memory_)
    : RasterizerAccelerated(cpu_memory_), gpu(gpu_), gpu_memory(gpu_memory_),
      maxwell3d(gpu.Maxwell3D()), kepler_compute(gpu.KeplerCompute()) {
}

RasterizerMetal::~RasterizerMetal() {
}

void RasterizerMetal::Draw(bool is_indexed, bool is_instanced) {
    UNIMPLEMENTED();
}

void RasterizerMetal::Clear() {
    UNIMPLEMENTED();
}

void RasterizerMetal::DispatchCompute(GPUVAddr code_addr) {
    UNIMPLEMENTED();
}

void RasterizerMetal::ResetCounter(VideoCore::QueryType type) {
    UNIMPLEMENTED();
}

void RasterizerMetal::Query(GPUVAddr gpu_addr, VideoCore::QueryType type, std::optional<u64> timestamp) {
    UNIMPLEMENTED();
}

void RasterizerMetal::FlushAll() {
    UNIMPLEMENTED();
}

void RasterizerMetal::FlushRegion(VAddr addr, u64 size) {
    UNIMPLEMENTED();
}

bool RasterizerMetal::MustFlushRegion(VAddr addr, u64 size) {
    UNIMPLEMENTED();
    return false;
}

void RasterizerMetal::InvalidateRegion(VAddr addr, u64 size) {
    UNIMPLEMENTED();
}

void RasterizerMetal::OnCPUWrite(VAddr addr, u64 size) {
    UNIMPLEMENTED();
}

void RasterizerMetal::SyncGuestHost() {
    UNIMPLEMENTED();
}

void RasterizerMetal::SignalSemaphore(GPUVAddr addr, u32 value) {
    UNIMPLEMENTED();
}

void RasterizerMetal::SignalSyncPoint(u32 value) {
    UNIMPLEMENTED();
}

void RasterizerMetal::ReleaseFences() {
    UNIMPLEMENTED();
}

void RasterizerMetal::FlushAndInvalidateRegion(VAddr addr, u64 size) {
    UNIMPLEMENTED();
}

void RasterizerMetal::WaitForIdle() {
    UNIMPLEMENTED();
}

void RasterizerMetal::FlushCommands() {
    UNIMPLEMENTED();
}

void RasterizerMetal::TickFrame() {
    UNIMPLEMENTED();
}

bool RasterizerMetal::AccelerateSurfaceCopy(const Tegra::Engines::Fermi2D::Regs::Surface& src,
                                            const Tegra::Engines::Fermi2D::Regs::Surface& dst,
                                            const Tegra::Engines::Fermi2D::Config& copy_config) {
    UNIMPLEMENTED();
    return true;
}

bool RasterizerMetal::AccelerateDisplay(const Tegra::FramebufferConfig& config, VAddr framebuffer_addr,
                                        u32 pixel_stride) {
    UNIMPLEMENTED();
    return true;
}

} // namespace Metal
