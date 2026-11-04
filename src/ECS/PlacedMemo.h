/******************************************************************************
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

#include <algorithm>
#include <functional>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "3D/MapReach.h"

/// A memo of the corners of a model's submesh where it is drawn. A model that does not move, with the same pose, shape
/// and land under it, has the same corners each frame, so the memo gives the corners that it keeps. The memo keeps
/// corners only while all the inputs of the calculation have the same bits, so its corners always equal a full
/// calculation.
namespace openblack::ecs::placed_memo
{
/// Which thing and which submesh geometry a set of corners is for
struct Slot
{
	entt::entity entity {entt::null};
	const void* geometry {nullptr};

	bool operator==(const Slot&) const = default;
};

struct SlotHash
{
	[[nodiscard]] size_t operator()(const Slot& slot) const noexcept
	{
		const auto a = std::hash<uint32_t> {}(static_cast<uint32_t>(entt::to_integral(slot.entity)));
		const auto b = std::hash<const void*> {}(slot.geometry);
		return a ^ (b + 0x9E3779B9u + (a << 6u) + (a >> 2u));
	}
};

/// Adds the bytes of a value to a key
template <typename T>
void Append(std::vector<std::byte>& key, const T& value)
{
	const auto bytes = std::as_bytes(std::span(&value, 1));
	key.insert(key.end(), bytes.begin(), bytes.end());
}
template <typename T>
void AppendAll(std::vector<std::byte>& key, std::span<const T> values)
{
	const auto bytes = std::as_bytes(values);
	key.insert(key.end(), bytes.begin(), bytes.end());
}

class Memo
{
public:
	/// Gives the corners of the slot.
	/// The key holds the bits of every input of the calculation, except the land.
	/// `place(corners)` makes the full calculation into the vector. It gives the reach of the map points where it read
	/// the land height, or none when it read no land.
	/// `readLand(reach, bytes)` adds the bytes that the land heights in the reach come from. It gives false when the land
	/// cannot give them; the memo then keeps nothing for the slot.
	template <typename Place, typename ReadLand>
	[[nodiscard]] std::span<const glm::vec3> Get(Slot slot, std::span<const std::byte> key, Place&& place, ReadLand&& readLand)
	{
		auto& entry = _entries[slot];
		entry.lastPass = _pass;
		if (entry.valid && std::ranges::equal(entry.key, key))
		{
			if (!entry.reach.has_value())
			{
				++_hits;
				return entry.corners;
			}
			_land.clear();
			if (readLand(*entry.reach, _land) && std::ranges::equal(_land, entry.land))
			{
				++_hits;
				return entry.corners;
			}
		}
		++_misses;
		entry.key.assign(key.begin(), key.end());
		entry.reach = place(entry.corners);
		entry.land.clear();
		entry.valid = !entry.reach.has_value() || readLand(*entry.reach, entry.land);
		return entry.corners;
	}

	/// Ends one pass of the pick. The memo forgets each slot that it did not use in this pass or the pass before it.
	void EndPass()
	{
		std::erase_if(_entries, [this](const auto& item) { return item.second.lastPass + 1 < _pass; });
		++_pass;
	}

	void Clear() { _entries.clear(); }

	/// Room that a caller can use to make a key, which the memo keeps from call to call
	[[nodiscard]] std::vector<std::byte>& KeyScratch() { return _key; }

	[[nodiscard]] size_t Size() const { return _entries.size(); }
	[[nodiscard]] uint64_t Hits() const { return _hits; }
	[[nodiscard]] uint64_t Misses() const { return _misses; }

private:
	struct Entry
	{
		std::vector<std::byte> key;
		std::optional<MapReach> reach;
		std::vector<std::byte> land;
		std::vector<glm::vec3> corners;
		uint64_t lastPass {0};
		bool valid {false};
	};

	std::unordered_map<Slot, Entry, SlotHash> _entries;
	std::vector<std::byte> _land;
	std::vector<std::byte> _key;
	uint64_t _pass {0};
	uint64_t _hits {0};
	uint64_t _misses {0};
};
} // namespace openblack::ecs::placed_memo
