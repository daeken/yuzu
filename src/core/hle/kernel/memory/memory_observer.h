// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <mutex>

#include "common/common_types.h"
#include "core/core.h"
#include "core/hle/kernel/memory/memory_block.h"

namespace Kernel::Memory {

class PageLinkedList;

class MemoryObserver : NonCopyable {
public:
    MemoryObserver(Core::System& system) : system(system) { }
    virtual ~MemoryObserver() { }
    virtual void Allocated(const PageLinkedList& page_linked_list, VAddr addr, MemoryPermission perm) = 0;
    virtual void Mapped(u64 paddr, VAddr addr, size_t num_pages, MemoryPermission perm) = 0;
    virtual void PermissionsChanged(VAddr addr, std::size_t num_pages, MemoryPermission perm) = 0;
    virtual void Freed(const PageLinkedList& page_linked_list, VAddr addr) = 0;

private:
    [[maybe_unused]] Core::System& system;
};

} // namespace Kernel::Memory
