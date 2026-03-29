#pragma once

#include <string_view>

#include "timenav/claim.hpp"
#include "timenav/ids.hpp"
#include "timenav/route.hpp"
#include "timenav/workspace_index.hpp"
#include "timenav/zone_policy.hpp"

namespace timenav {

    inline constexpr std::string_view version() noexcept { return "0.0.1"; }

} // namespace timenav

#ifdef SHORT_NAMESPACE
namespace tn = timenav;
#endif
