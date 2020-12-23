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
#include "video_core/renderer_metal/renderer_metal.h"
#include "video_core/renderer_metal/mtl_rasterizer.h"

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

namespace Metal {

RendererMetal::RendererMetal(Core::TelemetrySession& telemetry_session_,
                               Core::Frontend::EmuWindow& emu_window,
                               Core::Memory::Memory& cpu_memory_, Tegra::GPU& gpu_,
                               std::unique_ptr<Core::Frontend::GraphicsContext> context_)
    : RendererBase{emu_window, std::move(context_)}, telemetry_session{telemetry_session_},
      cpu_memory{cpu_memory_}, gpu{gpu_} {}

RendererMetal::~RendererMetal() {
    ShutDown();
}

void RendererMetal::SwapBuffers(const Tegra::FramebufferConfig* framebuffer) {
    UNIMPLEMENTED();
}

bool RendererMetal::Init() {
    auto wsi = render_window.GetWindowInfo();
    auto view = (NSView*) wsi.render_surface;
    auto layer = [CAMetalLayer layer];
    [view setWantsLayer: YES];
    [view setLayer: layer];
    auto factor = [[NSScreen mainScreen] backingScaleFactor];
    [layer setContentsScale: factor];
    wsi.render_surface = layer;

    rasterizer = std::make_unique<RasterizerMetal>(render_window, gpu, gpu.MemoryManager(),
                                                   cpu_memory);

    return true;
}

void RendererMetal::ShutDown() {
}

std::vector<std::string> RendererMetal::EnumerateDevices() {
    std::vector<std::string> devNames;

    auto devices = MTLCopyAllDevices();
    for (id device : devices)
        devNames.emplace_back([[device name] UTF8String]);

    return devNames;
}

} // namespace Metal
