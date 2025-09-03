#include <NGIN/ECS/ECS.hpp>

#include <charconv>
#include <stdexcept>

namespace NGIN::ECS {

int ParseInt(std::string_view text) {
    int value = 0;
    const auto* first = text.data();
    const auto* last  = first + text.size();
    auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || ptr != last) {
        throw std::invalid_argument("NGIN.ECS::ParseInt: invalid integer");
    }
    return value;
}

} // namespace NGIN::ECS

