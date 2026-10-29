#pragma once

#include "RenderPass.h"


#include <concepts>
#include <cstddef>

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace render_pipeline_detail {

template<typename Left, typename Right>
struct Concat;

template<typename... Left, typename... Right>
struct Concat<TypeList<Left...>, TypeList<Right...>> {
    using Type = TypeList<Left..., Right...>;
};

template<typename Needed, typename Available>
struct ContainsAll;

template<typename... Needed, typename... Available>
struct ContainsAll<TypeList<Needed...>, TypeList<Available...>> {
    template<typename T>
    static constexpr bool contains = (std::is_same_v<T, Available> || ...);
    static constexpr bool value = (contains<Needed> && ...);
};

template<typename Left, typename Right>
struct Intersects;

template<typename... Left, typename... Right>
struct Intersects<TypeList<Left...>, TypeList<Right...>> {
    static constexpr bool value =
        (ContainsAll<TypeList<Left>, TypeList<Right...>>::value || ...);
};

template<typename... Passes>
struct RequiredResults;

template<>
struct RequiredResults<> {
    using Type = TypeList<>;
};

template<typename First, typename... Rest>
struct RequiredResults<First, Rest...> {
    using Type = typename Concat<
        typename First::Requires,
        typename RequiredResults<Rest...>::Type>::Type;
};

struct Entry {
    std::unique_ptr<RenderPass> pass;
    bool enabled = true;
    bool required = false;
};

template<typename Available = TypeList<>, typename... Passes>
class Registration {
public:
    template<typename Pass, typename... Args>
    auto addPass(Args&&... args) && {
        static_assert(std::derived_from<Pass, RenderPass>);
        static_assert((!std::is_same_v<Pass, Passes> && ...),
                      "A pass can only be registered once");
        static_assert(ContainsAll<typename Pass::Requires, Available>::value,
                      "Required result must be produced by an earlier enabled pass");
        static_assert(requires { typename Pass::Optional; },
                      "Pass must declare optional dependencies");
        entries_.push_back({std::make_unique<Pass>(std::forward<Args>(args)...)});
        using Results = typename Concat<Available, typename Pass::Produces>::Type;
        return Registration<Results, Passes..., Pass>(std::move(entries_));
    }

    std::vector<Entry> finish() && {
        std::size_t index = 0;
        using Required = typename RequiredResults<Passes...>::Type;
        ((entries_[index++].required =
              Intersects<typename Passes::Produces, Required>::value), ...);
        return std::move(entries_);
    }

    Registration() requires (
        std::is_same_v<Available, TypeList<>> && sizeof...(Passes) == 0
    ) = default;

private:

    explicit Registration(std::vector<Entry> entries) : entries_(std::move(entries)) {}

    template<typename, typename...>
    friend class Registration;

    std::vector<Entry> entries_;
};

}

class RenderPipeline {
public:
    RenderPipeline();
    void execute(RenderContext& render, FrameContext& frame);

    template<typename Pass>
    Pass* findPass() {
        for (auto& entry : passes_) {
            if (auto* pass = dynamic_cast<Pass*>(entry.pass.get())) {
                return pass;
            }
        }
        return nullptr;
    }

    template<typename Pass>
    bool setEnabled(bool enabled) {
        for (auto& entry : passes_) {
            if (dynamic_cast<Pass*>(entry.pass.get())) {
                if (!enabled && entry.required) {
                    return false;
                }
                entry.enabled = enabled;
                return true;
            }
        }
        return false;
    }

private:
    std::vector<render_pipeline_detail::Entry> passes_;
};
