/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureObjectBeliefs.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <functional>

using namespace openblack::creature_object_beliefs;

Store::Added Store::Add(entt::entity thing, uint32_t kind, glm::vec3 place)
{
	if (const auto known = _index.find(thing); known != _index.end())
	{
		auto& belief = *_pools.at(known->second.kind).slots.at(known->second.slot);
		belief.place = place;
		return {.belief = &belief, .isNew = false};
	}
	if (kind >= k_KindCount)
	{
		return {};
	}
	auto& pool = _pools.at(kind);
	const auto capacity = static_cast<size_t>(k_Capacity.at(kind));
	if (pool.slots.empty())
	{
		pool.slots.resize(capacity);
	}
	size_t slot = 0;
	if (pool.used < capacity)
	{
		const auto free = std::ranges::find_if(pool.slots, [](const auto& held) { return !held.has_value(); });
		slot = static_cast<size_t>(std::distance(pool.slots.begin(), free));
		if (!pool.oldest.has_value())
		{
			pool.oldest = slot;
		}
		++pool.used;
	}
	else
	{
		// Every place is taken: the belief at the oldest-kept marker gives way, and the marker stays where it is
		slot = pool.oldest.value_or(0);
		_index.erase(pool.slots.at(slot)->thing);
	}
	pool.slots.at(slot) = Belief {.thing = thing, .kind = kind, .place = place, .order = ++_made};
	_index[thing] = Place {.kind = kind, .slot = slot};
	return {.belief = &*pool.slots.at(slot), .isNew = true};
}

const Belief* Store::Find(entt::entity thing) const
{
	const auto known = _index.find(thing);
	return known != _index.end() ? &*_pools.at(known->second.kind).slots.at(known->second.slot) : nullptr;
}

Belief* Store::Find(entt::entity thing)
{
	const auto known = _index.find(thing);
	return known != _index.end() ? &*_pools.at(known->second.kind).slots.at(known->second.slot) : nullptr;
}

bool Store::Forget(entt::entity thing)
{
	const auto known = _index.find(thing);
	if (known == _index.end())
	{
		return false;
	}
	const auto [kind, slot] = known->second;
	_index.erase(known);
	auto& pool = _pools.at(kind);
	pool.slots.at(slot).reset();
	// The marker moves on to the next place, round to the first after the last
	if (pool.oldest == slot)
	{
		pool.oldest = slot + 1 < pool.slots.size() ? slot + 1 : 0;
	}
	if (--pool.used == 0)
	{
		pool.oldest.reset();
	}
	return true;
}

size_t Store::Count(uint32_t kind) const
{
	return kind < k_KindCount ? _pools.at(kind).used : 0;
}

void Store::JoinTo(entt::entity part, entt::entity whole)
{
	if (auto* belief = Find(part))
	{
		belief->partOf = whole;
	}
}

std::vector<Belief> Store::PartsOf(entt::entity whole) const
{
	auto parts = NewestFirst();
	std::erase_if(parts, [whole](const Belief& belief) { return belief.partOf != whole; });
	return parts;
}

std::vector<Belief> Store::NewestFirst() const
{
	auto all = All();
	std::ranges::sort(all, std::ranges::greater {}, &Belief::order);
	return all;
}

std::vector<Belief> Store::All() const
{
	std::vector<Belief> all;
	all.reserve(_index.size());
	for (const auto& pool : _pools)
	{
		for (const auto& held : pool.slots)
		{
			if (held.has_value())
			{
				all.push_back(*held);
			}
		}
	}
	return all;
}

uint32_t openblack::creature_object_beliefs::SweepCells(float shownSize)
{
	// Worked out as the game does, in the FPU's extended precision, then truncated
	const auto across = static_cast<uint32_t>(static_cast<int32_t>(std::sqrt(static_cast<double>(shownSize) * 60.0 + 160.0)));
	const auto side = (across >> 1) * 2 + 1;
	return side * side;
}

glm::ivec2 openblack::creature_object_beliefs::RingCell(glm::ivec2 centre, uint32_t ring, uint32_t step)
{
	if (ring < 2)
	{
		return centre;
	}
	const auto out = static_cast<int32_t>(ring - 1);
	const auto along = static_cast<int32_t>(step % (ring * 2 - 2));
	switch ((step * 4) / ((ring - 1) * 8))
	{
	case 0:
		return {centre.x + along - out, centre.y - out};
	case 1:
		return {centre.x + out, centre.y + along - out};
	case 2:
		return {centre.x + out - along, centre.y + out};
	default:
		return {centre.x - out, centre.y + out - along};
	}
}

std::vector<glm::ivec2> openblack::creature_object_beliefs::NextCells(Sweep& sweep, glm::ivec2 centre, float shownSize)
{
	const auto cells = SweepCells(shownSize) & 0xFFFFu;
	if (sweep.left == 0)
	{
		sweep = Sweep {.left = cells, .ring = 1, .step = 0};
	}
	constexpr uint32_t k_MostPerTurn = 9;
	const auto perTurn = std::min(cells / 9, k_MostPerTurn);
	std::vector<glm::ivec2> next;
	next.reserve(perTurn);
	for (uint32_t i = 0; i < perTurn; ++i)
	{
		next.push_back(RingCell(centre, sweep.ring, sweep.step));
		if (--sweep.left == 0)
		{
			break;
		}
		++sweep.step;
		if (sweep.ring == 1 || sweep.step >= sweep.ring * 8 - 8)
		{
			++sweep.ring;
			sweep.step = 0;
		}
	}
	return next;
}

bool openblack::creature_object_beliefs::CanSee(uint16_t looking, uint16_t direction, bool sameCell)
{
	constexpr int32_t k_HalfCircle = 0x400;
	constexpr int32_t k_Circle = 0x800;
	auto apart = std::abs(static_cast<int32_t>(looking) - static_cast<int32_t>(direction));
	if (apart > k_HalfCircle)
	{
		apart = k_Circle - apart;
	}
	return apart <= k_SightHalfAngle || sameCell;
}
