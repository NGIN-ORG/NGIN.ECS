#pragma once

#include <NGIN/Primitives.hpp>
#include <NGIN/Hashing/FNV.hpp>
#include <NGIN/Meta/TypeName.hpp>
#include <NGIN/Meta/TypeTraits.hpp>

#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace NGIN::ECS
{
    using TypeId = NGIN::UInt64;

    using CopyConstructFn     = void (*)(void* destination, const void* source);
    using MoveConstructFn     = void (*)(void* destination, void* source);
    using RelocateConstructFn = void (*)(void* destination, void* source);
    using DestroyFn           = void (*)(void* instance) noexcept;

    template<typename T>
    [[nodiscard]] inline TypeId GetTypeId() noexcept
    {
        static const TypeId kId = [] {
            const auto sv = NGIN::Meta::TypeName<T>::qualifiedName;
            return NGIN::Hashing::FNV1a64(sv.data(), sv.size());
        }();
        return kId;
    }

    struct ComponentInfo
    {
        TypeId              id {0};
        NGIN::UIntSize      Size {0};
        NGIN::UIntSize      Align {1};
        bool                IsPOD {false};
        bool                IsEmpty {false};
        bool                IsBitwiseRelocatable {false};
        CopyConstructFn     CopyConstruct {nullptr};
        MoveConstructFn     MoveConstruct {nullptr};
        RelocateConstructFn RelocateConstruct {nullptr};
        DestroyFn           Destroy {nullptr};
    };

    namespace detail
    {
        template<typename T>
        inline void CopyConstructImpl(void* destination, const void* source)
        {
            ::new (destination) T(*static_cast<const T*>(source));
        }

        template<typename T>
        inline void MoveConstructImpl(void* destination, void* source)
        {
            if constexpr (std::is_move_constructible_v<T>)
            {
                ::new (destination) T(std::move(*static_cast<T*>(source)));
            }
            else
            {
                ::new (destination) T(*static_cast<const T*>(source));
            }
        }

        template<typename T>
        inline void RelocateConstructImpl(void* destination, void* source)
        {
            MoveConstructImpl<T>(destination, source);
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                static_cast<T*>(source)->~T();
            }
        }

        template<typename T>
        inline void BitwiseCopyImpl(void* destination, const void* source)
        {
            std::memcpy(destination, source, sizeof(T));
        }

        template<typename T>
        inline void BitwiseMoveImpl(void* destination, void* source)
        {
            std::memcpy(destination, source, sizeof(T));
        }

        template<typename T>
        inline void BitwiseRelocateImpl(void* destination, void* source)
        {
            std::memcpy(destination, source, sizeof(T));
        }

        template<typename T>
        inline void DestroyImpl(void* instance) noexcept
        {
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                static_cast<T*>(instance)->~T();
            }
        }
    }// namespace detail

    template<typename T>
    [[nodiscard]] inline ComponentInfo DescribeComponent() noexcept
    {
        using Component = std::remove_cvref_t<T>;
        static_assert(std::is_destructible_v<Component>, "ECS components must be destructible.");
        static_assert(std::is_move_constructible_v<Component> || std::is_copy_constructible_v<Component>,
                      "ECS components must be move-constructible or copy-constructible.");

        ComponentInfo info {};
        info.id                   = GetTypeId<Component>();
        info.Size                 = sizeof(Component);
        info.Align                = alignof(Component);
        info.IsPOD                = std::is_trivially_copyable_v<Component> && std::is_trivially_destructible_v<Component>;
        info.IsEmpty              = std::is_empty_v<Component>;
        info.IsBitwiseRelocatable = NGIN::Meta::TypeTraits<Component>::IsBitwiseRelocatable();

        if constexpr (!std::is_empty_v<Component>)
        {
            if constexpr (NGIN::Meta::TypeTraits<Component>::IsBitwiseRelocatable())
            {
                info.CopyConstruct     = &detail::BitwiseCopyImpl<Component>;
                info.MoveConstruct     = &detail::BitwiseMoveImpl<Component>;
                info.RelocateConstruct = &detail::BitwiseRelocateImpl<Component>;
            }
            else
            {
                if constexpr (std::is_copy_constructible_v<Component>)
                {
                    info.CopyConstruct = &detail::CopyConstructImpl<Component>;
                }
                if constexpr (std::is_move_constructible_v<Component> || std::is_copy_constructible_v<Component>)
                {
                    info.MoveConstruct     = &detail::MoveConstructImpl<Component>;
                    info.RelocateConstruct = &detail::RelocateConstructImpl<Component>;
                }
            }
            info.Destroy = &detail::DestroyImpl<Component>;
        }
        return info;
    }
}// namespace NGIN::ECS
