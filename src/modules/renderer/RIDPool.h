#pragma once

#include "RID.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

template<typename T>
class RIDPool {
public:
    template<typename... Args>
    RID create(Args&&... args) {
        uint32_t index;

        if (!freeIndices_.empty()) {
            index = freeIndices_.back();
            freeIndices_.pop_back();
        } else {
            if (slots_.size() >= RID::INVALID_INDEX) {
                return {};
            }

            index = static_cast<uint32_t>(slots_.size());
            slots_.emplace_back();
        }

        Slot& slot = slots_[index];

        slot.object = std::make_unique<T>(
            std::forward<Args>(args)...
        );

        ++count_;

        return RID{index, slot.generation};
    }

    T* get(RID rid) {
        if (!isValid(rid)) {
            return nullptr;
        }

        return slots_[rid.index].object.get();
    }

    const T* get(RID rid) const {
        if (!isValid(rid)) {
            return nullptr;
        }

        return slots_[rid.index].object.get();
    }

    bool destroy(RID rid) {
        if (!isValid(rid)) {
            return false;
        }

        Slot& slot = slots_[rid.index];

        slot.object.reset();

        ++slot.generation;

        if (slot.generation == 0) {
            ++slot.generation;
        }

        freeIndices_.push_back(rid.index);

        --count_;

        return true;
    }

    bool isValid(RID rid) const {
        if (!rid.isValid() || rid.index >= slots_.size()) {
            return false;
        }

        const Slot& slot = slots_[rid.index];

        return slot.object != nullptr &&
               slot.generation == rid.generation;
    }

    size_t size() const {
        return count_;
    }

    void clear() {
        slots_.clear();
        freeIndices_.clear();
        count_ = 0;
    }

    template<typename F>
    void forEach(F&& callback) {
        for (uint32_t i = 0; i < slots_.size(); ++i) {
            Slot& slot = slots_[i];

            if (!slot.object) {
                continue;
            }

            callback(
                RID{i, slot.generation},
                *slot.object
            );
        }
    }

private:
    struct Slot {
        std::unique_ptr<T> object;
        uint32_t generation = 1;
    };

    std::vector<Slot> slots_;
    std::vector<uint32_t> freeIndices_;

    size_t count_ = 0;
};
