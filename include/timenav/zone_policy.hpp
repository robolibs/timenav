#pragma once

#include <algorithm>
#include <cctype>
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

    enum class TrafficIssueSeverity { Warning, Error };

    struct TrafficParseIssue {
        TrafficIssueSeverity severity = TrafficIssueSeverity::Error;
        dp::String key;
        dp::String message;
    };

    struct ZonePolicy {
        ZonePolicyKind kind = ZonePolicyKind::Informational;
        dp::u64 capacity = 1;
        bool capacity_is_explicit = false;
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
        dp::Optional<bool> blocked;
        dp::Optional<dp::f64> priority;
        dp::Optional<dp::u64> capacity;
        bool capacity_is_explicit = false;
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

        inline std::string trim_copy(const std::string &value) {
            const auto first =
                std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch); });
            const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
                                  return std::isspace(ch);
                              }).base();
            return first >= last ? std::string{} : std::string(first, last);
        }

        inline std::string lower_copy(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return value;
        }

        inline dp::Optional<bool> parse_bool_relaxed(const std::string &value) {
            const auto normalized = lower_copy(trim_copy(value));
            if (normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "on") {
                return true;
            }
            if (normalized == "false" || normalized == "0" || normalized == "no" || normalized == "off") {
                return false;
            }
            return dp::nullopt;
        }

        inline dp::Optional<dp::u64> parse_u64_relaxed(const std::string &value) {
            try {
                const auto parsed = std::stoull(trim_copy(value));
                return static_cast<dp::u64>(parsed);
            } catch (...) {
                return dp::nullopt;
            }
        }

        inline dp::Optional<dp::f64> parse_f64_relaxed(const std::string &value) {
            try {
                return static_cast<dp::f64>(std::stod(trim_copy(value)));
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

        inline bool is_known_zone_policy_kind(const std::string &value) {
            return value == "informational" || value == "exclusive" || value == "shared" || value == "corridor" ||
                   value == "restricted" || value == "slow" || value == "replanning" || value == "no_stop";
        }

        inline bool is_known_zone_traffic_key(const std::string &key) {
            return key == "traffic.policy" || key == "traffic.mode" || key == "traffic.capacity" ||
                   key == "traffic.max_occupancy" || key == "traffic.priority" || key == "traffic.claim_required" ||
                   key == "traffic.entry_rule" || key == "traffic.exit_rule" || key == "traffic.speed_limit" ||
                   key == "traffic.waiting_allowed" || key == "traffic.stop_allowed" || key == "traffic.no_stop" ||
                   key == "traffic.replan_trigger" || key == "traffic.blocked" || key == "traffic.robot_class" ||
                   key == "traffic.schedule_window" || key == "traffic.access_group" ||
                   key == "traffic.blocks_entry_without_grant" || key == "traffic.blocks_traversal_without_grant";
        }

        inline bool is_known_edge_traffic_key(const std::string &key) {
            return key == "traffic.speed_limit" || key == "traffic.lane_type" || key == "traffic.lane_kind" ||
                   key == "traffic.reversible" || key == "traffic.passing_allowed" || key == "traffic.blocked" ||
                   key == "traffic.priority" || key == "traffic.capacity" || key == "traffic.max_occupancy" ||
                   key == "traffic.clearance_width" || key == "traffic.clearance_height" ||
                   key == "traffic.surface_type" || key == "traffic.robot_class" || key == "traffic.allowed_payload" ||
                   key == "traffic.cost_bias" || key == "traffic.no_stop" || key == "traffic.preferred_direction" ||
                   key == "traffic.direction";
        }

    } // namespace detail

    inline dp::Result<bool> parse_traffic_bool(std::string_view value) {
        const auto parsed = detail::parse_bool_relaxed(std::string(value));
        if (!parsed.has_value()) {
            return dp::Result<bool>::err(
                dp::Error::parse_error(dp::String::format("invalid boolean traffic value: %s", value.data())));
        }
        return dp::Result<bool>::ok(*parsed);
    }

    inline dp::Result<dp::u64> parse_traffic_u64(std::string_view value) {
        const auto parsed = detail::parse_u64_relaxed(std::string(value));
        if (!parsed.has_value()) {
            return dp::Result<dp::u64>::err(
                dp::Error::parse_error(dp::String::format("invalid unsigned traffic value: %s", value.data())));
        }
        return dp::Result<dp::u64>::ok(*parsed);
    }

    inline dp::Result<dp::f64> parse_traffic_f64(std::string_view value) {
        const auto parsed = detail::parse_f64_relaxed(std::string(value));
        if (!parsed.has_value()) {
            return dp::Result<dp::f64>::err(
                dp::Error::parse_error(dp::String::format("invalid numeric traffic value: %s", value.data())));
        }
        return dp::Result<dp::f64>::ok(*parsed);
    }

    inline dp::Result<dp::String> parse_traffic_string(std::string_view value) {
        const auto normalized = detail::trim_copy(std::string(value));
        if (normalized.empty()) {
            return dp::Result<dp::String>::err(dp::Error::parse_error("traffic string value must not be empty"));
        }
        return dp::Result<dp::String>::ok(dp::String{normalized});
    }

    /**
     * Parse supported `zone.properties` traffic keys into a typed policy model.
     *
     * Supported zone keys:
     * - `traffic.policy`
     * - `traffic.capacity`
     * - `traffic.priority`
     * - `traffic.claim_required`
     * - `traffic.entry_rule`
     * - `traffic.exit_rule`
     * - `traffic.speed_limit`
     * - `traffic.waiting_allowed`
     * - `traffic.stop_allowed`
     * - `traffic.replan_trigger`
     * - `traffic.blocked`
     * - `traffic.robot_class`
     * - `traffic.schedule_window`
     * - `traffic.access_group`
     *
     * Example:
     * `{"traffic.policy":"exclusive","traffic.capacity":"2","traffic.claim_required":"true"}`
     */
    inline ZonePolicy parse_zone_policy(const std::unordered_map<std::string, std::string> &properties) {
        ZonePolicy policy{};

        for (const auto &[key, value] : properties) {
            policy.properties[dp::String{key}] = dp::String{value};

            if (key == "traffic.policy" || key == "traffic.mode") {
                policy.kind = detail::parse_zone_policy_kind(value);
            } else if (key == "traffic.capacity" || key == "traffic.max_occupancy") {
                const auto parsed = parse_traffic_u64(value);
                if (parsed.is_ok() && parsed.value() >= 1) {
                    policy.capacity = parsed.value();
                    policy.capacity_is_explicit = true;
                    if (parsed.value() > 1 && policy.kind == ZonePolicyKind::Informational) {
                        policy.kind = ZonePolicyKind::CapacityLimited;
                    }
                }
            } else if (key == "traffic.claim_required") {
                const auto parsed = parse_traffic_bool(value);
                if (parsed.is_ok()) {
                    policy.requires_claim = parsed.value();
                    policy.blocks_entry_without_grant = parsed.value();
                }
            } else if (key == "traffic.blocked") {
                const auto parsed = parse_traffic_bool(value);
                if (parsed.is_ok()) {
                    policy.blocked = parsed.value();
                    if (parsed.value()) {
                        policy.kind = ZonePolicyKind::Restricted;
                        policy.blocks_entry_without_grant = true;
                        policy.blocks_traversal_without_grant = true;
                    }
                }
            } else if (key == "traffic.blocks_entry_without_grant") {
                const auto parsed = parse_traffic_bool(value);
                if (parsed.is_ok()) {
                    policy.blocks_entry_without_grant = parsed.value();
                }
            } else if (key == "traffic.blocks_traversal_without_grant") {
                const auto parsed = parse_traffic_bool(value);
                if (parsed.is_ok()) {
                    policy.blocks_traversal_without_grant = parsed.value();
                }
            } else if (key == "traffic.priority") {
                const auto parsed = parse_traffic_f64(value);
                if (parsed.is_ok()) {
                    policy.priority = parsed.value();
                }
            } else if (key == "traffic.speed_limit") {
                const auto parsed = parse_traffic_f64(value);
                if (parsed.is_ok()) {
                    policy.speed_limit = parsed.value();
                }
            } else if (key == "traffic.waiting_allowed") {
                const auto parsed = parse_traffic_bool(value);
                if (parsed.is_ok()) {
                    policy.waiting_allowed = parsed.value();
                }
            } else if (key == "traffic.stop_allowed") {
                const auto parsed = parse_traffic_bool(value);
                if (parsed.is_ok()) {
                    policy.stop_allowed = parsed.value();
                }
            } else if (key == "traffic.no_stop") {
                const auto parsed = parse_traffic_bool(value);
                if (parsed.is_ok()) {
                    policy.stop_allowed = !parsed.value();
                    if (parsed.value()) {
                        policy.kind = ZonePolicyKind::NoStop;
                    }
                }
            } else if (key == "traffic.replan_trigger") {
                const auto parsed = parse_traffic_bool(value);
                if (parsed.is_ok()) {
                    policy.replan_trigger = parsed.value();
                }
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

        if (policy.blocked.value_or(false)) {
            policy.kind = ZonePolicyKind::Restricted;
            policy.blocks_entry_without_grant = true;
            policy.blocks_traversal_without_grant = true;
        } else if (policy.replan_trigger.value_or(false)) {
            policy.kind = ZonePolicyKind::Replanning;
        } else if (policy.stop_allowed.has_value() && !policy.stop_allowed.value()) {
            policy.kind = ZonePolicyKind::NoStop;
        }

        return policy;
    }

    /**
     * Parse supported `edge.properties` traffic keys into typed traversal semantics.
     *
     * Supported edge keys:
     * - `traffic.speed_limit`
     * - `traffic.lane_type`
     * - `traffic.reversible`
     * - `traffic.passing_allowed`
     * - `traffic.blocked`
     * - `traffic.priority`
     * - `traffic.capacity`
     * - `traffic.clearance_width`
     * - `traffic.clearance_height`
     * - `traffic.surface_type`
     * - `traffic.robot_class`
     * - `traffic.allowed_payload`
     * - `traffic.cost_bias`
     * - `traffic.no_stop`
     * - `traffic.preferred_direction`
     *
     * The `directed` argument is structural and therefore wins over any property hint.
     */
    inline EdgeTrafficSemantics
    parse_edge_traffic_semantics(const std::unordered_map<std::string, std::string> &properties,
                                 bool directed = false) {
        EdgeTrafficSemantics semantics{};
        semantics.directed = directed;

        for (const auto &[key, value] : properties) {
            semantics.properties[dp::String{key}] = dp::String{value};

            if (key == "traffic.speed_limit") {
                const auto parsed = parse_traffic_f64(value);
                if (parsed.is_ok()) {
                    semantics.speed_limit = parsed.value();
                }
            } else if (key == "traffic.lane_type" || key == "traffic.lane_kind") {
                semantics.lane_type = dp::String{value};
            } else if (key == "traffic.reversible") {
                const auto parsed = parse_traffic_bool(value);
                if (parsed.is_ok()) {
                    semantics.reversible = parsed.value();
                }
            } else if (key == "traffic.passing_allowed") {
                const auto parsed = parse_traffic_bool(value);
                if (parsed.is_ok()) {
                    semantics.passing_allowed = parsed.value();
                }
            } else if (key == "traffic.blocked") {
                const auto parsed = parse_traffic_bool(value);
                if (parsed.is_ok()) {
                    semantics.blocked = parsed.value();
                }
            } else if (key == "traffic.priority") {
                const auto parsed = parse_traffic_f64(value);
                if (parsed.is_ok()) {
                    semantics.priority = parsed.value();
                }
            } else if (key == "traffic.capacity" || key == "traffic.max_occupancy") {
                const auto parsed = parse_traffic_u64(value);
                if (parsed.is_ok()) {
                    semantics.capacity = parsed.value();
                    semantics.capacity_is_explicit = true;
                }
            } else if (key == "traffic.clearance_width") {
                const auto parsed = parse_traffic_f64(value);
                if (parsed.is_ok()) {
                    semantics.clearance_width = parsed.value();
                }
            } else if (key == "traffic.clearance_height") {
                const auto parsed = parse_traffic_f64(value);
                if (parsed.is_ok()) {
                    semantics.clearance_height = parsed.value();
                }
            } else if (key == "traffic.surface_type") {
                semantics.surface_type = dp::String{value};
            } else if (key == "traffic.robot_class") {
                semantics.robot_class = dp::String{value};
            } else if (key == "traffic.allowed_payload") {
                semantics.allowed_payload = dp::String{value};
            } else if (key == "traffic.cost_bias") {
                const auto parsed = parse_traffic_f64(value);
                if (parsed.is_ok()) {
                    semantics.cost_bias = parsed.value();
                }
            } else if (key == "traffic.no_stop") {
                const auto parsed = parse_traffic_bool(value);
                if (parsed.is_ok()) {
                    semantics.no_stop = parsed.value();
                }
            } else if (key == "traffic.preferred_direction" || key == "traffic.direction") {
                semantics.preferred_direction = dp::String{value};
            }
        }

        return semantics;
    }

    inline dp::Vector<TrafficParseIssue>
    validate_zone_traffic_properties(const std::unordered_map<std::string, std::string> &properties) {
        dp::Vector<TrafficParseIssue> issues;

        for (const auto &[key, value] : properties) {
            if (key.rfind("traffic.", 0) == 0 && !detail::is_known_zone_traffic_key(key)) {
                issues.push_back(TrafficParseIssue{TrafficIssueSeverity::Warning, dp::String{key},
                                                   dp::String{"unknown zone traffic key"}});
            } else if (key == "traffic.policy" || key == "traffic.mode") {
                if (!detail::is_known_zone_policy_kind(value)) {
                    issues.push_back(TrafficParseIssue{TrafficIssueSeverity::Warning, dp::String{key},
                                                       dp::String{"unknown zone policy keyword"}});
                }
            } else if (key == "traffic.capacity" || key == "traffic.max_occupancy") {
                const auto parsed = parse_traffic_u64(value);
                if (parsed.is_err() || parsed.value_or(0) < 1) {
                    issues.push_back(TrafficParseIssue{TrafficIssueSeverity::Error, dp::String{key},
                                                       dp::String{"traffic.capacity must be an integer >= 1"}});
                }
            } else if (key == "traffic.priority") {
                if (parse_traffic_f64(value).is_err()) {
                    issues.push_back(TrafficParseIssue{TrafficIssueSeverity::Error, dp::String{key},
                                                       dp::String{"traffic.priority must be numeric"}});
                }
            } else if (key == "traffic.speed_limit") {
                const auto parsed = parse_traffic_f64(value);
                if (parsed.is_err() || parsed.value_or(0.0) <= 0.0) {
                    issues.push_back(TrafficParseIssue{TrafficIssueSeverity::Error, dp::String{key},
                                                       dp::String{"traffic.speed_limit must be positive"}});
                }
            } else if (key == "traffic.claim_required" || key == "traffic.waiting_allowed" ||
                       key == "traffic.stop_allowed" || key == "traffic.blocked" || key == "traffic.replan_trigger" ||
                       key == "traffic.no_stop" || key == "traffic.blocks_entry_without_grant" ||
                       key == "traffic.blocks_traversal_without_grant") {
                if (parse_traffic_bool(value).is_err()) {
                    issues.push_back(TrafficParseIssue{TrafficIssueSeverity::Error, dp::String{key},
                                                       dp::String{"traffic boolean key must parse as true/false"}});
                }
            } else if (key == "traffic.entry_rule" || key == "traffic.exit_rule" || key == "traffic.robot_class" ||
                       key == "traffic.schedule_window" || key == "traffic.access_group") {
                if (parse_traffic_string(value).is_err()) {
                    issues.push_back(TrafficParseIssue{TrafficIssueSeverity::Error, dp::String{key},
                                                       dp::String{"traffic string key must not be empty"}});
                }
            }
        }

        return issues;
    }

    inline dp::Vector<TrafficParseIssue>
    validate_edge_traffic_properties(const std::unordered_map<std::string, std::string> &properties) {
        dp::Vector<TrafficParseIssue> issues;

        for (const auto &[key, value] : properties) {
            if (key.rfind("traffic.", 0) == 0 && !detail::is_known_edge_traffic_key(key)) {
                issues.push_back(TrafficParseIssue{TrafficIssueSeverity::Warning, dp::String{key},
                                                   dp::String{"unknown edge traffic key"}});
            } else if (key == "traffic.capacity" || key == "traffic.max_occupancy") {
                const auto parsed = parse_traffic_u64(value);
                if (parsed.is_err() || parsed.value_or(0) < 1) {
                    issues.push_back(TrafficParseIssue{TrafficIssueSeverity::Error, dp::String{key},
                                                       dp::String{"traffic.capacity must be an integer >= 1"}});
                }
            } else if (key == "traffic.speed_limit" || key == "traffic.priority" || key == "traffic.clearance_width" ||
                       key == "traffic.clearance_height" || key == "traffic.cost_bias") {
                const auto parsed = parse_traffic_f64(value);
                if (parsed.is_err()) {
                    issues.push_back(TrafficParseIssue{TrafficIssueSeverity::Error, dp::String{key},
                                                       dp::String{"traffic numeric key must parse as number"}});
                } else if ((key == "traffic.speed_limit" || key == "traffic.clearance_width" ||
                            key == "traffic.clearance_height") &&
                           parsed.value() <= 0.0) {
                    issues.push_back(TrafficParseIssue{TrafficIssueSeverity::Error, dp::String{key},
                                                       dp::String{"traffic positive numeric key must be > 0"}});
                }
            } else if (key == "traffic.reversible" || key == "traffic.passing_allowed" || key == "traffic.no_stop" ||
                       key == "traffic.blocked") {
                if (parse_traffic_bool(value).is_err()) {
                    issues.push_back(TrafficParseIssue{TrafficIssueSeverity::Error, dp::String{key},
                                                       dp::String{"traffic boolean key must parse as true/false"}});
                }
            } else if (key == "traffic.lane_type" || key == "traffic.lane_kind" || key == "traffic.surface_type" ||
                       key == "traffic.robot_class" || key == "traffic.allowed_payload" ||
                       key == "traffic.preferred_direction" || key == "traffic.direction") {
                if (parse_traffic_string(value).is_err()) {
                    issues.push_back(TrafficParseIssue{TrafficIssueSeverity::Error, dp::String{key},
                                                       dp::String{"traffic string key must not be empty"}});
                }
            }
        }

        return issues;
    }

    /**
     * Merge recursive zone policies deterministically.
     *
     * Rules:
     * - `Restricted` dominates everything.
     * - `ExclusiveAccess` dominates `SharedAccess`.
     * - smaller capacity wins when capacity is explicitly constrained.
     * - child optionals override inherited values when present.
     */
    inline ZonePolicy merge_zone_policy(const ZonePolicy &parent, const ZonePolicy &child) {
        ZonePolicy merged = parent;

        auto merged_kind = parent.kind;
        if (parent.kind == ZonePolicyKind::Restricted || child.kind == ZonePolicyKind::Restricted) {
            merged_kind = ZonePolicyKind::Restricted;
        } else if (parent.kind == ZonePolicyKind::ExclusiveAccess || child.kind == ZonePolicyKind::ExclusiveAccess) {
            merged_kind = ZonePolicyKind::ExclusiveAccess;
        } else if (child.kind != ZonePolicyKind::Informational) {
            merged_kind = child.kind;
        }

        merged.kind = merged_kind;
        if (parent.capacity_is_explicit && child.capacity_is_explicit) {
            merged.capacity = std::min(parent.capacity, child.capacity);
            merged.capacity_is_explicit = true;
        } else if (child.capacity_is_explicit) {
            merged.capacity = child.capacity;
            merged.capacity_is_explicit = true;
        } else if (parent.capacity_is_explicit) {
            merged.capacity = parent.capacity;
            merged.capacity_is_explicit = true;
        } else {
            merged.capacity = child.capacity;
            merged.capacity_is_explicit = false;
        }
        merged.requires_claim = parent.requires_claim || child.requires_claim;
        merged.blocks_traversal_without_grant =
            parent.blocks_traversal_without_grant || child.blocks_traversal_without_grant;
        merged.blocks_entry_without_grant = parent.blocks_entry_without_grant || child.blocks_entry_without_grant;

        if (child.priority.has_value()) {
            merged.priority = child.priority;
        }
        if (child.speed_limit.has_value()) {
            merged.speed_limit = child.speed_limit;
        }
        if (child.waiting_allowed.has_value()) {
            merged.waiting_allowed = child.waiting_allowed;
        }
        if (child.stop_allowed.has_value()) {
            merged.stop_allowed = child.stop_allowed;
        }
        if (child.blocked.has_value()) {
            merged.blocked = child.blocked;
        }
        if (child.replan_trigger.has_value()) {
            merged.replan_trigger = child.replan_trigger;
        }
        if (child.entry_rule.has_value()) {
            merged.entry_rule = child.entry_rule;
        }
        if (child.exit_rule.has_value()) {
            merged.exit_rule = child.exit_rule;
        }
        if (child.robot_class.has_value()) {
            merged.robot_class = child.robot_class;
        }
        if (child.schedule_window.has_value()) {
            merged.schedule_window = child.schedule_window;
        }
        if (child.access_group.has_value()) {
            merged.access_group = child.access_group;
        }

        for (const auto &[key, value] : child.properties) {
            merged.properties[key] = value;
        }

        if (merged.kind == ZonePolicyKind::Restricted) {
            merged.blocks_entry_without_grant = true;
            merged.blocks_traversal_without_grant = true;
        }

        return merged;
    }

    /**
     * Combine structural edge facts, parsed edge properties, and containing-zone restrictions.
     *
     * Example:
     * `derive_effective_edge_semantics(edge_props, true, zone_policies)`
     */
    inline EdgeTrafficSemantics
    derive_effective_edge_semantics(const std::unordered_map<std::string, std::string> &properties, bool directed,
                                    const dp::Vector<ZonePolicy> &zone_policies = {}) {
        auto semantics = parse_edge_traffic_semantics(properties, directed);
        semantics.directed = directed;

        for (const auto &zone_policy : zone_policies) {
            if (zone_policy.speed_limit.has_value()) {
                if (!semantics.speed_limit.has_value()) {
                    semantics.speed_limit = zone_policy.speed_limit;
                } else {
                    semantics.speed_limit = std::min(semantics.speed_limit.value(), zone_policy.speed_limit.value());
                }
            }

            if (zone_policy.capacity_is_explicit || zone_policy.kind == ZonePolicyKind::CapacityLimited) {
                if (!semantics.capacity.has_value()) {
                    semantics.capacity = zone_policy.capacity;
                    semantics.capacity_is_explicit = true;
                } else {
                    semantics.capacity = std::min(semantics.capacity.value(), zone_policy.capacity);
                    semantics.capacity_is_explicit = true;
                }
            }

            if (!semantics.robot_class.has_value() && zone_policy.robot_class.has_value()) {
                semantics.robot_class = zone_policy.robot_class;
            }

            if (!semantics.priority.has_value() && zone_policy.priority.has_value()) {
                semantics.priority = zone_policy.priority;
            }

            if (zone_policy.kind == ZonePolicyKind::NoStop || zone_policy.blocked.value_or(false) ||
                zone_policy.kind == ZonePolicyKind::Restricted) {
                semantics.no_stop = true;
            }

            if (zone_policy.blocked.value_or(false) || zone_policy.kind == ZonePolicyKind::Restricted) {
                semantics.blocked = true;
            }

            if (zone_policy.kind == ZonePolicyKind::Slowdown) {
                semantics.cost_bias = semantics.cost_bias.value_or(0.0) + 1.0;
            }
        }

        return semantics;
    }

} // namespace timenav
