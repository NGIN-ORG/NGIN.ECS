#pragma once

#include <string_view>
#include <NGIN/ECS/Export.hpp>

namespace NGIN::ECS {

// Minimal skeleton API for ECS lib.
[[nodiscard]] NGIN_ECS_API int ParseInt(std::string_view text);

// For quick sanity checks / examples.
[[nodiscard]] NGIN_ECS_API constexpr std::string_view LibraryName() noexcept { return "NGIN.ECS"; }

} // namespace NGIN::ECS

