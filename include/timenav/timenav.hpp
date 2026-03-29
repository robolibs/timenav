#pragma once

#include <string_view>

#include "timenav/ids.hpp"

namespace timenav {

    inline constexpr std::string_view version() noexcept { return "0.0.1"; }

} // namespace timenav

#ifdef SHORT_NAMESPACE
namespace tn = timenav;
#endif
