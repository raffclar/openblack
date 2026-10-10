/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "InspectorJson.h"

/// The inspector's wire protocol: one JSON request per line in, one JSON response per line out.
///
/// A request names a query, gives it parameters, and shapes what comes back:
///
///     {"id": 7, "query": "objects.find", "params": {"component": "Tree"},
///      "near": [100, 0, 200], "radius": 30, "fields": ["id", "label", "distance"],
///      "where": [{"field": "label", "op": "contains", "value": "Oak"}],
///      "limit": 20, "cursor": 0, "count": false, "summary": "distance", "max_bytes": 16384}
///
/// Only "query" is required. The answer echoes the id:
///
///     {"id": 7, "ok": true, "result": {...}}      or      {"id": 7, "ok": false, "error": "..."}
///
/// A list's result is {"items": [...], "total": n, "next_cursor": n or null, "truncated": bool}; in count mode it is
/// {"count": n}, and with a summary {"field": ..., "count": n, "min": ..., "max": ..., "mean": ...}.
namespace openblack::inspector
{

/// Lists give this many items unless asked for more, and never more than the most
inline constexpr size_t k_DefaultLimit = 20;
inline constexpr size_t k_MostItems = 500;
/// A result is cut to this many bytes of JSON unless asked otherwise, and never more than the most
inline constexpr size_t k_DefaultMaxBytes = 16 * 1024;
inline constexpr size_t k_MostBytes = 1024 * 1024;

/// How a filter compares a field with its value
enum class FilterOp : uint8_t
{
	Equal,
	NotEqual,
	Less,
	LessEqual,
	Greater,
	GreaterEqual,
	/// A string holding the value as a part, or an array holding it as an element
	Contains,
	/// The field is there at all; the value is unused
	Exists,
};

struct Filter
{
	std::string field;
	FilterOp op {FilterOp::Equal};
	Json value;
};

/// Where to search around: a point in the world, and how far from it. Two coordinates are x and z on the ground, and
/// the distance is measured across the ground; three are x, y and z, and the distance is measured in space.
struct Near
{
	std::array<double, 3> point {};
	bool planar {false};
	double radius {0.0};
};

/// How a result is shaped, the same for every query
struct QueryOptions
{
	/// Only these dotted paths of each item (or of an object result); everything when empty
	std::vector<std::string> fields;
	std::vector<Filter> where;
	std::optional<Near> near;
	size_t limit {k_DefaultLimit};
	/// The place in the list to start from, as the last page's next_cursor gave it
	size_t cursor {0};
	/// Only how many items there are
	bool countOnly {false};
	/// The count, least, most and mean of this numeric field, instead of the items
	std::optional<std::string> summary;
	size_t maxBytes {k_DefaultMaxBytes};
};

struct Request
{
	/// Echoed in the response so that a client can match them up; null when the request gave none
	Json id;
	std::string query;
	Json params = Json::object();
	QueryOptions options;
};

/// A request read from a line of text, or why it couldn't be read
[[nodiscard]] std::variant<Request, std::string> DecodeRequest(std::string_view line);

/// A request written as a line of text (without the line's end), as a client sends it
[[nodiscard]] std::string EncodeRequest(const Request& request);

/// The lines of the two answers (without the line's end)
[[nodiscard]] std::string EncodeResult(const Json& id, const Json& result);
[[nodiscard]] std::string EncodeError(const Json& id, std::string_view error);
/// The same, with more members beside id, ok and result or error: the game that answered, whether it is loading
[[nodiscard]] std::string EncodeResult(const Json& id, const Json& result, const Json& extra);
[[nodiscard]] std::string EncodeError(const Json& id, std::string_view error, const Json& extra);

[[nodiscard]] std::optional<FilterOp> ParseFilterOp(std::string_view op);
[[nodiscard]] std::string_view Name(FilterOp op);

} // namespace openblack::inspector
