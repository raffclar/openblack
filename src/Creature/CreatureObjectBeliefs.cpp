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
#include <iterator>
#include <span>
#include <utility>
#include <vector>

#include "CreatureDecisionTree.h"

using namespace openblack::creature_object_beliefs;

bool openblack::creature_object_beliefs::InListOfWholeThings(uint32_t kind)
{
	namespace kinds = creature_tree::belief_types;
	return kind == kinds::k_Town || kind == kinds::k_Forest || kind == kinds::k_Citadel || kind == kinds::k_Creature ||
	       kind == kinds::k_Flock;
}

bool openblack::creature_object_beliefs::TakesInAgain(uint32_t kind)
{
	return kind != creature_tree::belief_types::k_Abode;
}

void openblack::creature_object_beliefs::TakeIn(Belief& belief, bool isNew, const creature_tree::Belief& now)
{
	// A belief with no attributes yet takes all of them, as a new belief does
	if (isNew || !belief.attributes.has_value())
	{
		belief.attributes = now;
		return;
	}
	if (TakesInAgain(belief.kind))
	{
		belief.attributes = creature_tree::WithFirstSight(*belief.attributes, now);
	}
}

bool openblack::creature_object_beliefs::BlockedRecently(const Belief& belief, uint32_t turn, bool fixed)
{
	return belief.blockedTurn.has_value() && turn - *belief.blockedTurn <= (fixed ? k_BlockedTurnsFixed : k_BlockedTurns);
}

void Store::MarkBlocked(entt::entity thing, uint32_t turn)
{
	if (auto* belief = Find(thing))
	{
		belief->blockedTurn = turn;
	}
}

Store::Added Store::Add(entt::entity thing, uint32_t kind, map_coords::MapCoords place)
{
	if (auto* known = Find(thing))
	{
		known->place = place;
		return {.belief = known, .isNew = false};
	}
	if (kind >= k_KindCount)
	{
		return {};
	}
	auto& pool = _contents.pools.at(kind);
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
		const auto replaced = pool.slots.at(slot)->thing;
		_contents.index.erase(replaced);
		_places.Erase(replaced);
	}
	pool.slots.at(slot) = Belief {.thing = thing, .kind = kind, .place = place, .order = ++_contents.made};
	_contents.index[thing] = Place {.kind = kind, .slot = slot};
	_places.Set(thing, Place {.kind = kind, .slot = slot});
	return {.belief = &*pool.slots.at(slot), .isNew = true};
}

const Belief* Store::Find(entt::entity thing) const
{
	// The fast index does not keep the no-thing value
	if (thing == entt::null)
	{
		const auto known = _contents.index.find(thing);
		return known != _contents.index.end() ? &*_contents.pools.at(known->second.kind).slots.at(known->second.slot) : nullptr;
	}
	const auto known = _places.Find(thing);
	return known.has_value() ? &*_contents.pools.at(known->kind).slots.at(known->slot) : nullptr;
}

Belief* Store::Find(entt::entity thing)
{
	return const_cast<Belief*>(std::as_const(*this).Find(thing));
}

void Store::Reindex()
{
	_places.Clear();
	for (const auto& [thing, at] : _contents.index)
	{
		_places.Set(thing, at);
	}
}

namespace
{
/// The number of entries in the smallest table. A table holds at most half as many things as it has entries, thus a
/// search stops soon at a free entry.
constexpr size_t k_LeastEntries = 16;
/// The multiplier of the golden ratio spreads the numbers of things that come one after another over the table
constexpr uint32_t k_Spread = 0x9E3779B1u;
static_assert(k_KindCount <= UINT16_MAX && std::ranges::max(k_Capacity) <= UINT16_MAX,
              "a kind and a place among the places of the kind fit in an entry of the fast index");
} // namespace

size_t ThingIndex::Home(entt::entity thing) const
{
	const auto mixed = static_cast<uint32_t>(entt::to_integral(thing)) * k_Spread;
	return static_cast<size_t>(mixed) & (_entries.size() - 1);
}

size_t ThingIndex::Seek(entt::entity thing) const
{
	const auto mask = _entries.size() - 1;
	auto at = Home(thing);
	while (_entries[at].thing != entt::null && _entries[at].thing != thing)
	{
		at = (at + 1) & mask;
	}
	return at;
}

std::optional<BeliefPlace> ThingIndex::Find(entt::entity thing) const
{
	if (_count == 0 || thing == entt::null)
	{
		return std::nullopt;
	}
	const auto& entry = _entries[Seek(thing)];
	return entry.thing == thing ? std::optional(BeliefPlace {.kind = entry.kind, .slot = entry.slot}) : std::nullopt;
}

void ThingIndex::Set(entt::entity thing, BeliefPlace place)
{
	if (thing == entt::null)
	{
		return;
	}
	if ((_count + 1) * 2 > _entries.size())
	{
		Grow();
	}
	auto& entry = _entries[Seek(thing)];
	if (entry.thing == entt::null)
	{
		entry.thing = thing;
		++_count;
	}
	entry.kind = static_cast<uint16_t>(place.kind);
	entry.slot = static_cast<uint16_t>(place.slot);
}

void ThingIndex::Erase(entt::entity thing)
{
	if (_count == 0 || thing == entt::null)
	{
		return;
	}
	const auto mask = _entries.size() - 1;
	auto gap = Seek(thing);
	if (_entries[gap].thing != thing)
	{
		return;
	}
	// The things after the gap move back into it when their own entry is not between the gap and where they are, so
	// that a search for each of them still finds it before a free entry
	for (auto next = (gap + 1) & mask; _entries[next].thing != entt::null; next = (next + 1) & mask)
	{
		const auto home = Home(_entries[next].thing);
		const bool staysPut = gap <= next ? (gap < home && home <= next) : (gap < home || home <= next);
		if (!staysPut)
		{
			_entries[gap] = _entries[next];
			gap = next;
		}
	}
	_entries[gap] = {};
	--_count;
}

void ThingIndex::Clear()
{
	_entries.clear();
	_count = 0;
}

void ThingIndex::Grow()
{
	auto old = std::exchange(_entries, std::vector<Entry>(std::max(k_LeastEntries, _entries.size() * 2)));
	for (const auto& entry : old)
	{
		if (entry.thing != entt::null)
		{
			_entries[Seek(entry.thing)] = entry;
		}
	}
}

bool Store::Forget(entt::entity thing)
{
	const auto known = _contents.index.find(thing);
	if (known == _contents.index.end())
	{
		return false;
	}
	const auto [kind, slot] = known->second;
	_contents.index.erase(known);
	_places.Erase(thing);
	auto& pool = _contents.pools.at(kind);
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
	return kind < k_KindCount ? _contents.pools.at(kind).used : 0;
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

namespace
{
/// Every belief that the pools hold, the newest first. The sort moves pointers to the beliefs, not the beliefs. It
/// compares the same values in the same sequence as a sort of the beliefs, thus it gives the same order.
std::vector<const Belief*> NewestFirstOf(const std::array<BeliefPool, k_KindCount>& pools, size_t count)
{
	std::vector<const Belief*> held;
	held.reserve(count);
	for (const auto& pool : pools)
	{
		for (const auto& slot : pool.slots)
		{
			if (slot.has_value())
			{
				held.push_back(&*slot);
			}
		}
	}
	std::ranges::sort(held, std::ranges::greater {}, [](const Belief* belief) { return belief->order; });
	return held;
}

std::vector<Belief> Copies(std::span<const Belief* const> held)
{
	std::vector<Belief> copies;
	copies.reserve(held.size());
	std::ranges::transform(held, std::back_inserter(copies), [](const Belief* belief) { return *belief; });
	return copies;
}
} // namespace

std::vector<Belief> Store::NewestFirst() const
{
	return Copies(HeldNewestFirst());
}

std::vector<Belief> Store::InListOrder() const
{
	return Copies(HeldInListOrder());
}

std::vector<const Belief*> Store::HeldNewestFirst() const
{
	return NewestFirstOf(_contents.pools, _contents.index.size());
}

std::vector<const Belief*> Store::HeldInListOrder() const
{
	auto held = NewestFirstOf(_contents.pools, _contents.index.size());
	std::ranges::stable_partition(held, [](const Belief* belief) { return InListOfWholeThings(belief->kind); });
	return held;
}

std::vector<Belief> Store::All() const
{
	std::vector<Belief> all;
	all.reserve(_contents.index.size());
	for (const auto& pool : _contents.pools)
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

int32_t openblack::creature_object_beliefs::TownCellDistance(glm::ivec2 a, glm::ivec2 b)
{
	const auto dx = std::abs(a.x - b.x);
	const auto dz = std::abs(a.y - b.y);
	return dx < dz ? (dx >> 1) + dz : (dz >> 1) + dx;
}

std::optional<uint32_t> openblack::creature_object_beliefs::NearestOtherTown(glm::ivec2 from, std::span<const TownCell> towns,
                                                                             std::optional<uint32_t> last)
{
	// No town further than this counts
	constexpr int32_t k_FurthestTown = 10'000'000;
	std::optional<uint32_t> nearest;
	auto best = k_FurthestTown;
	for (const auto& town : towns)
	{
		const auto distance = TownCellDistance(from, town.cell);
		if (distance < best && town.id != last)
		{
			nearest = town.id;
			best = distance;
		}
	}
	return nearest;
}
