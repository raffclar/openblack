/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InspectorQuery.h"

#include <cmath>
#include <cstddef>

#include <algorithm>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

using namespace openblack::inspector;

namespace
{

/// Compares two values that are both numbers or both strings: below, equal or above as -1, 0 or 1; none for values
/// that can't be ordered against each other
std::optional<int> Compare(const Json& left, const Json& right)
{
	if (left.is_string() && right.is_string())
	{
		const auto& a = left.get_ref<const std::string&>();
		const auto& b = right.get_ref<const std::string&>();
		return a < b ? -1 : (b < a ? 1 : 0);
	}
	const auto a = Number(left);
	const auto b = Number(right);
	if (!a.has_value() || !b.has_value())
	{
		return std::nullopt;
	}
	return *a < *b ? -1 : (*b < *a ? 1 : 0);
}

bool Equal(const Json& left, const Json& right)
{
	if (const auto order = Compare(left, right); order.has_value())
	{
		return *order == 0;
	}
	return left == right;
}

bool Contains(const Json& haystack, const Json& needle)
{
	if (haystack.is_string() && needle.is_string())
	{
		return haystack.get_ref<const std::string&>().find(needle.get_ref<const std::string&>()) != std::string::npos;
	}
	if (haystack.is_array())
	{
		return std::ranges::any_of(haystack, [&needle](const Json& each) { return Equal(each, needle); });
	}
	if (haystack.is_object() && needle.is_string())
	{
		return haystack.contains(needle.get_ref<const std::string&>());
	}
	return false;
}

/// Writes the value at a dotted path into a result, making the objects on the way
void Insert(Json& result, std::string_view path, const Json& value)
{
	Json* at = &result;
	while (true)
	{
		const auto dot = path.find('.');
		const std::string part(path.substr(0, dot));
		if (dot == std::string_view::npos)
		{
			(*at)[part] = value;
			return;
		}
		path = path.substr(dot + 1);
		auto& next = (*at)[part];
		if (!next.is_object())
		{
			next = Json::object();
		}
		at = &next;
	}
}

size_t Bytes(const Json& value)
{
	return Dump(value).size();
}

/// An object result too large to send: what there was of it, to choose fields from
Json TooLarge(const Json& value, size_t bytes)
{
	Json keys = Json::array();
	if (value.is_object())
	{
		for (const auto& [key, unused] : value.items())
		{
			keys.push_back(key);
		}
	}
	return {
	    {"truncated", true},
	    {"bytes", bytes},
	    {"keys", std::move(keys)},
	    {"hint", "the result was larger than max_bytes: choose fields, or raise max_bytes"},
	};
}

Json Summary(const Json& items, const std::string& field)
{
	size_t count = 0;
	double least = std::numeric_limits<double>::infinity();
	double most = -std::numeric_limits<double>::infinity();
	double sum = 0.0;
	for (const auto& item : items)
	{
		const auto* value = Find(item, field);
		if (value == nullptr)
		{
			continue;
		}
		const auto number = Number(*value);
		if (!number.has_value())
		{
			continue;
		}
		++count;
		least = std::min(least, *number);
		most = std::max(most, *number);
		sum += *number;
	}
	Json summary = {{"field", field}, {"count", count}, {"total", items.size()}};
	if (count == 0)
	{
		summary["min"] = nullptr;
		summary["max"] = nullptr;
		summary["mean"] = nullptr;
	}
	else
	{
		summary["min"] = least;
		summary["max"] = most;
		summary["mean"] = sum / static_cast<double>(count);
	}
	return summary;
}

} // namespace

bool openblack::inspector::Matches(const Json& item, const Filter& filter)
{
	const auto* value = Find(item, filter.field);
	if (filter.op == FilterOp::Exists)
	{
		return value != nullptr && !value->is_null();
	}
	if (value == nullptr)
	{
		return filter.op == FilterOp::NotEqual;
	}
	switch (filter.op)
	{
	case FilterOp::Equal:
		return Equal(*value, filter.value);
	case FilterOp::NotEqual:
		return !Equal(*value, filter.value);
	case FilterOp::Contains:
		return Contains(*value, filter.value);
	default:
		break;
	}
	const auto order = Compare(*value, filter.value);
	if (!order.has_value())
	{
		return false;
	}
	switch (filter.op)
	{
	case FilterOp::Less:
		return *order < 0;
	case FilterOp::LessEqual:
		return *order <= 0;
	case FilterOp::Greater:
		return *order > 0;
	case FilterOp::GreaterEqual:
		return *order >= 0;
	default:
		return false;
	}
}

std::optional<std::array<double, 3>> openblack::inspector::ReadPoint(const Json& value, bool* planar)
{
	if (planar != nullptr)
	{
		*planar = false;
	}
	if (value.is_array() && (value.size() == 2 || value.size() == 3))
	{
		std::array<double, 3> point {};
		std::array<double, 3> read {};
		for (size_t i = 0; i < value.size(); ++i)
		{
			if (!value[i].is_number())
			{
				return std::nullopt;
			}
			read.at(i) = value[i].get<double>();
		}
		if (value.size() == 2)
		{
			// On the ground: x and z
			point = {read[0], 0.0, read[1]};
			if (planar != nullptr)
			{
				*planar = true;
			}
		}
		else
		{
			point = read;
		}
		return point;
	}
	if (value.is_object())
	{
		const auto x = NumberMember(value, "x");
		const auto y = NumberMember(value, "y");
		const auto z = NumberMember(value, "z");
		if (!x.has_value() || !z.has_value())
		{
			return std::nullopt;
		}
		if (!y.has_value() && planar != nullptr)
		{
			*planar = true;
		}
		return std::array<double, 3> {*x, y.value_or(0.0), *z};
	}
	return std::nullopt;
}

std::optional<double> openblack::inspector::Distance(const Json& item, const Near& near)
{
	if (!item.is_object())
	{
		return std::nullopt;
	}
	const auto it = item.find("position");
	if (it == item.end())
	{
		return std::nullopt;
	}
	const auto position = ReadPoint(*it);
	if (!position.has_value())
	{
		return std::nullopt;
	}
	const double dx = (*position)[0] - near.point[0];
	const double dy = near.planar ? 0.0 : (*position)[1] - near.point[1];
	const double dz = (*position)[2] - near.point[2];
	return std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
}

Json openblack::inspector::SelectFields(const Json& value, std::span<const std::string> fields)
{
	if (fields.empty())
	{
		return value;
	}
	Json result = Json::object();
	for (const auto& field : fields)
	{
		if (const auto* found = Find(value, field); found != nullptr)
		{
			Insert(result, field, *found);
		}
	}
	return result;
}

Json openblack::inspector::ShapeList(Json items, const QueryOptions& options)
{
	if (!items.is_array())
	{
		items = Json::array({std::move(items)});
	}

	// Filtered, then searched around the point, nearest first
	std::vector<std::pair<double, Json*>> kept;
	kept.reserve(items.size());
	for (auto& item : items)
	{
		if (!std::ranges::all_of(options.where, [&item](const Filter& filter) { return Matches(item, filter); }))
		{
			continue;
		}
		double distance = 0.0;
		if (options.near.has_value())
		{
			const auto measured = Distance(item, *options.near);
			if (!measured.has_value() || *measured > options.near->radius)
			{
				continue;
			}
			distance = *measured;
			item["distance"] = distance;
		}
		kept.emplace_back(distance, &item);
	}
	if (options.near.has_value())
	{
		std::ranges::stable_sort(kept, {}, &std::pair<double, Json*>::first);
	}

	const size_t total = kept.size();
	if (options.countOnly)
	{
		return {{"count", total}};
	}
	if (options.summary.has_value())
	{
		Json matched = Json::array();
		for (const auto& [unused, item] : kept)
		{
			matched.push_back(std::move(*item));
		}
		return Summary(matched, *options.summary);
	}

	const size_t first = std::min(options.cursor, total);
	const size_t last = std::min(first + options.limit, total);
	Json page = Json::array();
	for (size_t i = first; i < last; ++i)
	{
		page.push_back(SelectFields(*kept[i].second, options.fields));
	}

	// Cut to the size cap from the end, the page ending sooner and the next one starting there
	bool truncated = false;
	const auto envelope = [&](const Json& items, size_t end) {
		return Json {
		    {"items", items},
		    {"total", total},
		    {"next_cursor", end < total ? Json(end) : Json(nullptr)},
		    {"truncated", truncated},
		};
	};
	auto result = envelope(page, last);
	if (Bytes(result) <= options.maxBytes)
	{
		return result;
	}
	truncated = true;
	// Measure each item once, and keep what fits beside the envelope
	const size_t envelopeBytes = Bytes(envelope(Json::array(), last)) + 1;
	size_t bytes = envelopeBytes;
	size_t fits = 0;
	for (const auto& item : page)
	{
		const size_t itemBytes = Bytes(item) + 1;
		if (bytes + itemBytes > options.maxBytes)
		{
			break;
		}
		bytes += itemBytes;
		++fits;
	}
	page.erase(page.begin() + static_cast<std::ptrdiff_t>(fits), page.end());
	result = envelope(page, first + fits);
	if (fits == 0)
	{
		// The next page would start at the same place: say how to get past it
		result["hint"] = "an item is larger than max_bytes: choose fields, or raise max_bytes";
	}
	return result;
}

Json openblack::inspector::ShapeObject(const Json& value, const QueryOptions& options)
{
	auto result = SelectFields(value, options.fields);
	const size_t bytes = Bytes(result);
	if (bytes <= options.maxBytes)
	{
		return result;
	}
	return TooLarge(result, bytes);
}
