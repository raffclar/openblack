/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <optional>
#include <span>
#include <string>

#include "InspectorJson.h"
#include "InspectorProtocol.h"

/// The query language's shaping of results, the same for every query: filters, a search around a point (nearest
/// first), counts, summaries, pages, field selection and a cap on the size. Providers hand over their items and the
/// shaping keeps the answer small.
namespace openblack::inspector
{

/// Whether an item passes a filter
[[nodiscard]] bool Matches(const Json& item, const Filter& filter);

/// A point read from JSON: [x, y, z], [x, z] on the ground, or {"x", "y", "z"}; none for anything else
[[nodiscard]] std::optional<std::array<double, 3>> ReadPoint(const Json& value, bool* planar = nullptr);

/// How far an item's "position" is from the search's point, none if it has no position
[[nodiscard]] std::optional<double> Distance(const Json& item, const Near& near);

/// Only the given dotted paths of a value, nested as they were; the whole value when no paths are given
[[nodiscard]] Json SelectFields(const Json& value, std::span<const std::string> fields);

/// A list of items shaped by the options: filtered, searched around a point nearest first with each item's
/// "distance" added, then counted, summarised or paged, its fields selected and its size capped
[[nodiscard]] Json ShapeList(Json items, const QueryOptions& options);

/// An object result shaped by the options: its fields selected and its size capped
[[nodiscard]] Json ShapeObject(const Json& value, const QueryOptions& options);

} // namespace openblack::inspector
