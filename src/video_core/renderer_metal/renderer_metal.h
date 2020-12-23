// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "video_core/renderer_base.h"

namespace Core {
class TelemetrySession;
}

namespace Core::Memory {
class Memory;
}

namespace Tegra {
class GPU;
}

namespace Metal {

class RendererMetal final : public VideoCore::RendererBase {
public:
    explicit RendererMetal(Core::TelemetrySession& telemtry_session,
                            Core::Frontend::EmuWindow& emu_window,
                            Core::Memory::Memory& cpu_memory_, Tegra::GPU& gpu_,
                            std::unique_ptr<Core::Frontend::GraphicsContext> context_);
    ~RendererMetal() override;

    bool Init() override;
    void ShutDown() override;
    void SwapBuffers(const Tegra::FramebufferConfig* framebuffer) override;

    static std::vector<std::string> EnumerateDevices();

private:
    Core::TelemetrySession& telemetry_session;
    Core::Memory::Memory& cpu_memory;
    Tegra::GPU& gpu;
};

} // namespace Metal
