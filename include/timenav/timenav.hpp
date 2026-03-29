#pragma once

#include <string_view>

#include "timenav/claim.hpp"
#include "timenav/claim_manager.hpp"
#include "timenav/coordinator.hpp"
#include "timenav/ids.hpp"
#include "timenav/robot_state.hpp"
#include "timenav/route.hpp"
#include "timenav/vda/adapter.hpp"
#include "timenav/vda/connection.hpp"
#include "timenav/vda/factsheet.hpp"
#include "timenav/vda/instant_actions.hpp"
#include "timenav/vda/order.hpp"
#include "timenav/vda/responses.hpp"
#include "timenav/vda/state.hpp"
#include "timenav/workspace_index.hpp"
#include "timenav/zone_policy.hpp"

namespace timenav {

    inline constexpr std::string_view version() noexcept { return "0.0.1"; }

} // namespace timenav

#ifdef SHORT_NAMESPACE
namespace tn = timenav;
#endif
