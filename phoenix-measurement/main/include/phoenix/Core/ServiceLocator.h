// ==========================================================================
// FILE: include/phoenix/Core/ServiceLocator.h
// Lightweight service registry — type-erased, fixed slots, no heap alloc
// ==========================================================================
#pragma once

#include "phoenix/Core/Result.h"
#include <cstdint>
#include <cstring>

namespace phoenix {

// Maximum number of services in the system
static constexpr size_t MAX_SERVICES = 16;

class ServiceLocator {
public:
    static ServiceLocator& getInstance() {
        static ServiceLocator instance;
        return instance;
    }

    // Register a service pointer with a string key
    // The caller owns the lifetime — typically static or heap-allocated once
    template <typename T>
    Result<void> registerService(const char* name, T* service) {
        if (count_ >= MAX_SERVICES) {
            return Err(ErrorCategory::MEMORY_ERROR, "ServiceLocator full");
        }
        if (!name || !service) {
            return Err(ErrorCategory::INVALID_PARAMETER, "Null name or service");
        }

        // Check for duplicate
        for (size_t i = 0; i < count_; ++i) {
            if (strcmp(entries_[i].name, name) == 0) {
                // Replace existing
                entries_[i].ptr = static_cast<void*>(service);
                return Ok();
            }
        }

        auto& e = entries_[count_++];
        strncpy(e.name, name, sizeof(e.name) - 1);
        e.name[sizeof(e.name) - 1] = '\0';
        e.ptr = static_cast<void*>(service);
        return Ok();
    }

    // Retrieve a service by name
    template <typename T>
    Result<T*> getService(const char* name) const {
        for (size_t i = 0; i < count_; ++i) {
            if (strcmp(entries_[i].name, name) == 0) {
                return Ok(static_cast<T*>(entries_[i].ptr));
            }
        }
        return Err<T*>(ErrorCategory::NOT_FOUND, "Service not found");
    }

    [[nodiscard]] bool hasService(const char* name) const {
        for (size_t i = 0; i < count_; ++i) {
            if (strcmp(entries_[i].name, name) == 0) return true;
        }
        return false;
    }

    [[nodiscard]] size_t count() const { return count_; }

    void reset() { count_ = 0; }

private:
    ServiceLocator() = default;

    struct Entry {
        char  name[32] = {};
        void* ptr      = nullptr;
    };

    Entry  entries_[MAX_SERVICES] = {};
    size_t count_ = 0;
};

} // namespace phoenix
