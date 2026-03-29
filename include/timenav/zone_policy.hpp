#pragma once

#include <string>
#include <unordered_map>

#include <datapod/datapod.hpp>

namespace timenav {

    enum class ZonePolicyKind {
        Informational,
        ExclusiveAccess,
        SharedAccess,
        CapacityLimited,
        Corridor,
        Replanning,
        Restricted,
        NoStop,
        Slowdown
    };

    struct ZonePolicy {
        ZonePolicyKind kind = ZonePolicyKind::Informational;
        dp::u64 capacity = 1;
        bool requires_claim = false;
        bool blocks_traversal_without_grant = false;
        bool blocks_entry_without_grant = false;
        dp::Optional<dp::f64> priority;
        dp::Optional<dp::f64> speed_limit;
        dp::Optional<bool> waiting_allowed;
        dp::Optional<bool> stop_allowed;
        dp::Optional<bool> blocked;
        dp::Optional<bool> replan_trigger;
        dp::Optional<dp::String> entry_rule;
        dp::Optional<dp::String> exit_rule;
        dp::Optional<dp::String> robot_class;
        dp::Optional<dp::String> schedule_window;
        dp::Optional<dp::String> access_group;
        dp::Map<dp::String, dp::String> properties;
    };

    struct EdgeTrafficSemantics {
        bool directed = false;
        dp::Optional<dp::f64> speed_limit;
        dp::Optional<dp::String> lane_type;
        dp::Optional<bool> reversible;
        dp::Optional<bool> passing_allowed;
        dp::Optional<dp::f64> priority;
        dp::Optional<dp::u64> capacity;
        dp::Optional<dp::f64> clearance_width;
        dp::Optional<dp::f64> clearance_height;
        dp::Optional<dp::String> surface_type;
        dp::Optional<dp::String> robot_class;
        dp::Optional<dp::String> allowed_payload;
        dp::Optional<dp::f64> cost_bias;
        dp::Optional<bool> no_stop;
        dp::Optional<dp::String> preferred_direction;
        dp::Map<dp::String, dp::String> properties;
    };

    namespace detail {

        inline dp::Optional<bool> parse_bool_relaxed(const std::string &value) {
            if (value == "true" || value == "1" || value == "yes" || value == "on") {
                return true;
            }
            if (value == "false" || value == "0" || value == "no" || value == "off") {
                return false;
            }
            return dp::nullopt;
        }

        inline dp::Optional<dp::u64> parse_u64_relaxed(const std::string &value) {
            try {
                const auto parsed = std::stoull(value);
                return static_cast<dp::u64>(parsed);
            } catch (...) {
                return dp::nullopt;
            }
        }

        inline dp::Optional<dp::f64> parse_f64_relaxed(const std::string &value) {
            try {
                return static_cast<dp::f64>(std::stod(value));
            } catch (...) {
                return dp::nullopt;
            }
        }

        inline ZonePolicyKind parse_zone_policy_kind(const std::string &value) {
            if (value == "exclusive") {
                return ZonePolicyKind::ExclusiveAccess;
            }
            if (value == "shared") {
                return ZonePolicyKind::SharedAccess;
            }
            if (value == "corridor") {
                return ZonePolicyKind::Corridor;
            }
            if (value == "restricted") {
                return ZonePolicyKind::Restricted;
            }
            if (value == "slow") {
                return ZonePolicyKind::Slowdown;
            }
            if (value == "replanning") {
                return ZonePolicyKind::Replanning;
            }
            if (value == "no_stop") {
                return ZonePolicyKind::NoStop;
            }
            return ZonePolicyKind::Informational;
        }

    } // namespace detail

    inline ZonePolicy parse_zone_policy(const std::unordered_map<std::string, std::string> &properties) {
        ZonePolicy policy{};

        for (const auto &[key, value] : properties) {
            policy.properties[dp::String{key}] = dp::String{value};

            if (key == "traffic.policy") {
                policy.kind = detail::parse_zone_policy_kind(value);
            } else if (key == "traffic.capacity") {
                const auto parsed = detail::parse_u64_relaxed(value);
                if (parsed.has_value() && *parsed >= 1) {
                    policy.capacity = *parsed;
                    if (*parsed > 1 && policy.kind == ZonePolicyKind::Informational) {
                        policy.kind = ZonePolicyKind::CapacityLimited;
                    }
                }
            } else if (key == "traffic.claim_required") {
                const auto parsed = detail::parse_bool_relaxed(value);
                if (parsed.has_value()) {
                    policy.requires_claim = *parsed;
                    policy.blocks_entry_without_grant = *parsed;
                }
            } else if (key == "traffic.blocked") {
                const auto parsed = detail::parse_bool_relaxed(value);
                if (parsed.has_value()) {
                    policy.blocked = *parsed;
                    if (*parsed) {
                        policy.kind = ZonePolicyKind::Restricted;
                        policy.blocks_entry_without_grant = true;
                        policy.blocks_traversal_without_grant = true;
                    }
                }
            } else if (key == "traffic.priority") {
                policy.priority = detail::parse_f64_relaxed(value);
            } else if (key == "traffic.speed_limit") {
                policy.speed_limit = detail::parse_f64_relaxed(value);
            } else if (key == "traffic.waiting_allowed") {
                policy.waiting_allowed = detail::parse_bool_relaxed(value);
            } else if (key == "traffic.stop_allowed") {
                policy.stop_allowed = detail::parse_bool_relaxed(value);
            } else if (key == "traffic.replan_trigger") {
                policy.replan_trigger = detail::parse_bool_relaxed(value);
                if (policy.replan_trigger.value_or(false)) {
                    policy.kind = ZonePolicyKind::Replanning;
                }
            } else if (key == "traffic.entry_rule") {
                policy.entry_rule = dp::String{value};
            } else if (key == "traffic.exit_rule") {
                policy.exit_rule = dp::String{value};
            } else if (key == "traffic.robot_class") {
                policy.robot_class = dp::String{value};
            } else if (key == "traffic.schedule_window") {
                policy.schedule_window = dp::String{value};
            } else if (key == "traffic.access_group") {
                policy.access_group = dp::String{value};
            }
        }

        return policy;
    }

    inline EdgeTrafficSemantics
    parse_edge_traffic_semantics(const std::unordered_map<std::string, std::string> &properties,
                                 bool directed = false) {
        EdgeTrafficSemantics semantics{};
        semantics.directed = directed;

        for (const auto &[key, value] : properties) {
            semantics.properties[dp::String{key}] = dp::String{value};

            if (key == "traffic.speed_limit") {
                semantics.speed_limit = detail::parse_f64_relaxed(value);
            } else if (key == "traffic.lane_type") {
                semantics.lane_type = dp::String{value};
            } else if (key == "traffic.reversible") {
                semantics.reversible = detail::parse_bool_relaxed(value);
            } else if (key == "traffic.passing_allowed") {
                semantics.passing_allowed = detail::parse_bool_relaxed(value);
            } else if (key == "traffic.priority") {
                semantics.priority = detail::parse_f64_relaxed(value);
            } else if (key == "traffic.capacity") {
                semantics.capacity = detail::parse_u64_relaxed(value);
            } else if (key == "traffic.clearance_width") {
                semantics.clearance_width = detail::parse_f64_relaxed(value);
            } else if (key == "traffic.clearance_height") {
                semantics.clearance_height = detail::parse_f64_relaxed(value);
            } else if (key == "traffic.surface_type") {
                semantics.surface_type = dp::String{value};
            } else if (key == "traffic.robot_class") {
                semantics.robot_class = dp::String{value};
            } else if (key == "traffic.allowed_payload") {
                semantics.allowed_payload = dp::String{value};
            } else if (key == "traffic.cost_bias") {
                semantics.cost_bias = detail::parse_f64_relaxed(value);
            } else if (key == "traffic.no_stop") {
                semantics.no_stop = detail::parse_bool_relaxed(value);
            } else if (key == "traffic.preferred_direction") {
                semantics.preferred_direction = dp::String{value};
            }
        }

        return semantics;
    }

} // namespace timenav
