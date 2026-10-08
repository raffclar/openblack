/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <vector>

#include <glm/vec3.hpp>

/// The single queue of everything blended in the world view (models with alpha and fading ones, sprites, mists and
/// clouds, chimney smoke, particle effects, rain tiles, the hand...). Every caller sends one Z object with its own key;
/// the frame's start empties the queue and the frame's end draws it once, far to near, after everything drawn at once
/// in the frame. Wiki: docs/bw1-notes/rendering-objects.md, "The single transparent queue".
///
/// The original's queue is a linked list in a static buffer of 2048 entries; its sorter object is never read, so it is
/// not ported. Queue keeps the same list, with the caller's own payload in place of the (object, callback) pair.
namespace openblack::graphics::zsort
{

/// A full queue drops the new entry, whatever its key
constexpr uint32_t k_Capacity = 0x800;

/// The order in which a caller adds the squares. The original inlines the key in every caller and the x87 sum follows
/// the order of its loads: with the FPU at 24 bits each step rounds to a float, so the two orders can differ in the
/// last bit. The precision is set at start-up and again after every frame's end. (inferred) that nothing between it and
/// the callers sets the precision back (D3D7 without FPUPRESERVE also leaves it at 24 bits)
enum class SumOrder : uint8_t
{
	/// (x^2 + y^2) + z^2: z, y, x loaded (sprites, mists, smoke, particle effects, the hand, particle meshes)
	XYZ,
	/// (x^2 + z^2) + y^2: y, z, x loaded (models, the rain)
	XZY,
};

/// The sort key: |point - camera|^2 in float, not the distance (the square root keeps the order but merges keys that
/// differ in the last bit)
[[nodiscard]] float Key(const glm::vec3& point, const glm::vec3& camera, SumOrder order = SumOrder::XYZ) noexcept;

/// The rain's user data K: ((alpha << 8) - trunc(z x -0.0125)) << 8 - trunc(x x -0.0125): alpha x 65536 +
/// 256 trunc(z / 80) + trunc(x / 80) inside the map (outside [0, 20480) the bytes run into each other, as in the
/// original). The caller clamps alpha to <= 0xFF and queues nothing with a negative one
[[nodiscard]] uint32_t PackRainUser(float x, float z, int32_t alpha) noexcept;

/// What the rain's draw reads back from K: byte 0 the tile x, byte 1 the tile z (each x 80 to get the tile's corner)
/// and byte 2 the alpha
struct RainUser
{
	int32_t tileX;
	int32_t tileZ;
	int32_t alpha;
};
[[nodiscard]] RainUser UnpackRainUser(uint32_t user) noexcept;

/// The queue. Item is what the caller needs to draw the entry later (the original keeps an object and a member function
/// to call on it)
template <typename Item>
class Queue
{
public:
	Queue() { _nodes.reserve(k_Capacity); }

	/// head = 0, count = 0. The frame's start also clears the "drained" flag
	void Begin() noexcept
	{
		_nodes.clear();
		_head = k_None;
		_drained = false;
		_dropped = 0;
	}

	/// A new Z object. The entry goes before the first one whose key is strictly smaller, or last: far to near, and with
	/// equal keys the one that came first is drawn first. The original's compare also says "smaller" when either key is
	/// NaN, which !(cur >= key) keeps. Returns false when the queue is full and the entry was dropped
	bool Submit(const Item& item, float key, uint32_t user = 0)
	{
		if (_nodes.size() >= k_Capacity)
		{
			++_dropped;
			return false;
		}
		const auto added = static_cast<uint32_t>(_nodes.size());
		_nodes.push_back({item, user, key, k_None});
		uint32_t previous = k_None;
		for (uint32_t current = _head; current != k_None; current = _nodes[current].next)
		{
			if (!(_nodes[current].key >= key))
			{
				_nodes[added].next = current;
				(previous == k_None ? _head : _nodes[previous].next) = added;
				return true;
			}
			previous = current;
		}
		(previous == k_None ? _head : _nodes[previous].next) = added;
		return true;
	}

	/// An entry as the drain hands it out: the caller's item, its user data K and its key
	struct Entry
	{
		const Item* item;
		uint32_t user;
		float key;
	};

	/// At the frame's end, only while not drained yet: marked drained, then from the head (the farthest) to the end,
	/// the user data made current and the callback called. Here the entries come back in that order for the caller to
	/// draw each one (its callback), with K in Entry::user; empty when the queue was already drained since Begin. The
	/// queue is not emptied (Begin does that) and no render state is touched: each callback sets its own. (inferred) the
	/// guard against a second drain: nothing else sets the flag
	[[nodiscard]] std::vector<Entry> Drain()
	{
		if (_drained)
		{
			return {};
		}
		_drained = true;
		return Ordered();
	}

	/// The entries in draw order, without draining (for traces)
	[[nodiscard]] std::vector<Entry> Ordered() const
	{
		std::vector<Entry> ordered;
		ordered.reserve(_nodes.size());
		for (uint32_t current = _head; current != k_None; current = _nodes[current].next)
		{
			ordered.push_back({&_nodes[current].item, _nodes[current].user, _nodes[current].key});
		}
		return ordered;
	}

	[[nodiscard]] uint32_t Size() const noexcept { return static_cast<uint32_t>(_nodes.size()); }
	[[nodiscard]] bool Empty() const noexcept { return _nodes.empty(); }
	/// (openblack) how many entries the full queue dropped since Begin; the original keeps no count
	[[nodiscard]] uint32_t Dropped() const noexcept { return _dropped; }

private:
	static constexpr uint32_t k_None = UINT32_MAX;
	/// The caller's item (the original's object and callback), K (user), the key and the next entry
	struct Node
	{
		Item item;
		uint32_t user;
		float key;
		uint32_t next;
	};
	std::vector<Node> _nodes; ///< the count is its size
	uint32_t _head {k_None};
	bool _drained {false};
	uint32_t _dropped {0};
};

} // namespace openblack::graphics::zsort
