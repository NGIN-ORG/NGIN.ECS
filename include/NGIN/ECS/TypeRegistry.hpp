#pragma once

#include <NGIN/Primitives.hpp>
#include <NGIN/Hashing/FNV.hpp>
#include <NGIN/Meta/TypeName.hpp>
#include <type_traits>

namespace NGIN::ECS
{
    using TypeId = NGIN::UInt64;

    template<typename T>
    [[nodiscard]] inline TypeId GetTypeId() noexcept
    {
        // Compute once per T using runtime-stable qualifiedName.
        static const TypeId kId = [] {
            const auto sv = NGIN::Meta::TypeName<T>::qualifiedName;
            return NGIN::Hashing::FNV1a64(sv.data(), sv.size());
        }();
        return kId;
    }

    struct ComponentInfo
    {
        TypeId      id {0};
        NGIN::UIntSize Size {0};
        NGIN::UIntSize Align {1};
        bool        IsPOD {false};
        bool        IsEmpty {false}; // zero-sized tags
    };

    template<typename T>
    [[nodiscard]] inline ComponentInfo DescribeComponent() noexcept
    {
        ComponentInfo info {};
        info.id      = GetTypeId<T>();
        info.Size    = sizeof(T);
        info.Align   = alignof(T);
        info.IsPOD   = std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>;
        info.IsEmpty = std::is_empty_v<T>;
        return info;
    }
}// namespace NGIN::ECS
