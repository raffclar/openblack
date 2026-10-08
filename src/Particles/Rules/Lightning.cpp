/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The lightning bolt's rules (SF_LightningBolt / PUOne / PUTwo, SF_LightningStrike, SF_LightningStorm): UR_Lightning
// picks the objects inside a cone in front of the hand and each step grows a tree of forks (chains of joints,
// Creators/Chain.cpp) to the ones it strikes: a trunk that splits in two until every branch holds one target (Fork).
// It drops a light map and sends the spell a "landed" event at each struck tip, which is what burns and kills. Two
// bolts cast from the hand that meet are linked (FindClash): both trunks end at the clash point, where a glow sits, and
// the older one goes on from there, three times as thick, to its target. UR_LightningStrike is the one-shot script /
// climate strike. See docs/bw1-notes/miracles.md, "Rayo".

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <chrono>
#include <memory>
#include <numbers>
#include <optional>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Audio/Services/SpellSounds.h"
#include "Camera/Camera.h"
#include "Debug/DebugEnv.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerScript.h"
#include "GameClock.h"
#include "Locator.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysManager.h"
#include "Particles/PSysRegistry.h"
#include "Particles/Rules/Shield.h"
#include "Particles/SoundAction.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
/// The scale of the fork after a clash point: fixed, set by no property
constexpr float k_ClashScale = 3.0f;
/// The margin of the fork's shield test and of FindIntersect
constexpr float k_ShieldMargin = 2.5f;

float LandAt(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

/// The objects of one 10 m map cell in the original's order (the fixed list, then the mobile one, each from its head;
/// ecs::map_cells); nothing outside the map
void CellObjects(const map_coords::MapCoords& coords, std::vector<entt::entity>& out)
{
	out.clear();
	if (!map_coords::InBounds(coords))
	{
		return;
	}
	out = ecs::map_cells::ObjectsInCell(map_coords::Cell(coords));
}

/// The effect's random stream (game or local): 0 for n == 0 without a draw, else a draw modulo n as an UNSIGNED
/// number: a negative n draws from 0 .. 2^32 - |n| - 1, kept in a signed int (INT_MIN, the truncation of an infinite
/// step count, gives 0 .. 2^31 - 1)
int32_t RandomBelow(Effect& effect, int32_t n)
{
	return effect.Rand(n);
}

/// Not being deleted (ecs::IsAvailable), and for a villager its final state is not DYING
/// (ecs::villager::IsAvailable)
bool IsAvailable(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::IsAvailable(object))
	{
		return false;
	}
	return !registry.AllOf<ecs::components::Villager>(object) || ecs::villager::IsAvailable(object);
}

/// A spell seed is never struck (the bolt must not shoot at the miracle icons lying about)
bool CanBeStruck(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::IsAvailable(object) || !registry.AllOf<ecs::components::Transform>(object))
	{
		return false;
	}
	// TODO: a creature is also skipped when one of its values is below 0.5 (UNVERIFIED meaning)
	return !registry.AllOf<ecs::components::SpellSeed>(object);
}

/// One thing the bolt may strike this cast
struct Target
{
	entt::entity object {entt::null}; ///< null: a ground point
	glm::vec3 ground {0.0f};
	bool isObject {false};
	bool active {false};       ///< it may be struck
	bool lightMapDone {false}; ///< its "impressive" report is in
	/// Written when the targets are found and at each strike, ticked down every step but never read to skip a
	/// target: kept as data only
	int cooldown {0};

	/// An object's tip is its MapCoords as a point (the land's height plus its altitude) raised by GetHeight; a ground
	/// point is itself
	[[nodiscard]] glm::vec3 Tip() const
	{
		if (!isObject)
		{
			return ground;
		}
		auto& registry = Locator::entitiesRegistry::value();
		if (!ecs::IsAvailable(object) || !registry.AllOf<ecs::components::Transform>(object))
		{
			return ground;
		}
		auto tip = map_coords::ToWorld(ecs::object::MapCoordsOf(object));
		tip.y = ecs::object::GetHeight(object) + tip.y;
		return tip;
	}
};

/// What Link does to the bolt's fork 0, done by that bolt's own next step
enum class GlowChange
{
	None,
	Add,    ///< A CommonGlowGroup sub-collection on its last joint when it has none
	Remove, ///< Every sub-collection of its last joint goes
};

/// What the rule keeps for one collection between steps
struct Data
{
	uint32_t key {0};              ///< the port's name of it (DataFor)
	uint32_t link {0};             ///< the bolt this one is linked to (it aims at that one's clash), 0 none
	uint32_t linkedBy {0};         ///< the bolt linked to this one, 0 none
	glm::vec3 origin {0.0f};       ///< where the forks start (the gesture position, or the parent atom)
	glm::vec3 searchOrigin {0.0f}; ///< the origin of the last target search
	glm::vec3 centroid {0.0f};     ///< the targets' tips summed over their number (FindTargets)
	float heading {0.0f};          ///< atan2(cameraForward.z, cameraForward.x)
	bool first {true};             ///< the fork structure is not built yet
	bool fromHand {false};         ///< the rule's CastingFromHand when the data was made
	float life {0.0f};             ///< the age since the last target search (reset by FindTargets)
	/// |centroid - origin| (FindTargets), 1.0 until the first search
	float centroidDistance {1.0f};
	std::vector<Target> targets;
	std::vector<int> striking; ///< the targets picked this step
	float forkScale {1.0f};    ///< ForkScale x FP_ForkScale
	uint32_t createdTurn {0};  ///< the game turn the data was made
	glm::vec3 clash {0.0f};    ///< the clash point, when another bolt is linked to this one (FindClash)
	Atom* root {nullptr};      ///< the single atom of the collection; its sub-collections are the forks
	// port bookkeeping: the effect that steps it (to send the linked bolt's events), the turn of its last step, the
	// glow Link asked for, and the wall clock of its last step
	Effect* effect {nullptr};
	uint32_t effectId {0};
	uint32_t steppedTurn {0};
	GlowChange glow {GlowChange::None};
	int glowGroup {-1};
	std::chrono::steady_clock::time_point touched;
};

/// What this module keeps between calls (Locator::particleSystem)
struct LightningState
{
	/// The data of a live collection, keyed as the fireball's balls are: a float in the slot names it, so nothing
	/// dangles when the collection goes. The original keeps them in a global list, newest first, and a
	/// collection's data leaves it when the data is destroyed. Here an entry leaves when its collection's step
	/// stops coming (no step this turn nor the last, or ten seconds of wall clock without one: port bookkeeping, a
	/// pause longer than that resets a live bolt's targets)
	uint32_t nextKey {1};
	std::unordered_map<uint32_t, Data> data {};
	std::vector<uint32_t> order {}; ///< the keys, newest first
};

LightningState& LightningData()
{
	return openblack::Locator::particleSystem::value().Module<LightningState>();
}

Data* Find(uint32_t key)
{
	auto& state = LightningData();
	if (key == 0)
	{
		return nullptr;
	}
	const auto it = state.data.find(key);
	return it != state.data.end() ? &it->second : nullptr;
}

/// `bolt` is linked to `other`, or unlinked with 0. The old partner forgets it (linkedBy = 0); a new link gives the
/// other linkedBy = bolt. Linking from none puts a CommonGlowGroup collection on the last joint of the bolt's fork 0,
/// unlinking takes that joint's sub-collections off. The original changes the other bolt's atoms at once; here that
/// waits for the bolt's own step (ApplyGlow), so the glow may come or go one turn late (approximate): the other bolt's
/// atoms are not safe to touch from this one's step
void Link(Data& bolt, uint32_t other, int group)
{
	if (bolt.link == other)
	{
		return;
	}
	if (auto* old = Find(bolt.link); old != nullptr)
	{
		old->linkedBy = 0;
	}
	if (other == 0)
	{
		if (bolt.link != 0)
		{
			bolt.glow = GlowChange::Remove;
		}
		bolt.link = 0;
		return;
	}
	if (auto* target = Find(other); target != nullptr)
	{
		target->linkedBy = bolt.key;
	}
	if (bolt.link == 0)
	{
		bolt.glow = GlowChange::Add;
		bolt.glowGroup = group;
	}
	bolt.link = other;
}

/// The data goes: a bolt linked to this one is unlinked and the one this is linked to forgets it; then it leaves the
/// list
void Destroy(Data& data)
{
	if (auto* by = Find(data.linkedBy); by != nullptr)
	{
		Link(*by, 0, -1);
	}
	if (auto* to = Find(data.link); to != nullptr)
	{
		to->linkedBy = 0;
	}
	std::erase(LightningData().order, data.key);
}

Data& DataFor(Effect& effect, Collection::Slot& slot, bool fromHand)
{
	auto& state = LightningData();
	const auto now = std::chrono::steady_clock::now();
	const auto turn = game_clock::Turn();
	std::vector<uint32_t> gone;
	for (const auto& [key, entry] : state.data)
	{
		if (now - entry.touched > std::chrono::seconds(10) || entry.steppedTurn + 1 < turn)
		{
			gone.push_back(key);
		}
	}
	for (const auto key : gone)
	{
		Destroy(state.data[key]);
		state.data.erase(key);
	}
	auto key = static_cast<uint32_t>(slot.extra.x);
	if (key == 0 || !state.data.contains(key))
	{
		key = state.nextKey++ & 0xFFFFFF;
		slot.extra.x = static_cast<float>(key);
		auto& created = state.data[key];
		created = Data {};
		created.key = key;
		created.fromHand = fromHand;
		created.createdTurn = turn;
		state.order.insert(state.order.begin(), key);
	}
	auto& data = state.data[key];
	data.touched = now;
	data.steppedTurn = turn;
	data.effect = &effect;
	data.effectId = manager::IdOf(&effect);
	return data;
}

/// The effect of a linked bolt, for its events (the collection's manager). A running
/// effect is checked against the manager; one the manager does not run (the tests) only by its step this turn
Effect* EffectOf(const Data& data)
{
	if (data.effect == nullptr)
	{
		return nullptr;
	}
	if (data.effectId != 0)
	{
		return manager::Find(data.effectId) == data.effect ? data.effect : nullptr;
	}
	return data.steppedTurn == game_clock::Turn() ? data.effect : nullptr;
}

/// UR_Lightning
class Lightning final: public Modifier
{
public:
	explicit Lightning(const Object& object)
	    : creator(object.String("PCreator"))
	    , lightMapCreator(object.String("PCreatorLightMapAtom"))
	    , forkGroup(object.Int("ForkGroup", -1))
	    , commonGlowGroup(object.Int("CommonGlowGroup", -1))
	    , lightMapGroup(object.Int("LightMapGroup", -1))
	    , maxObjects(std::max(1, object.Int("MaxLightningObjects", 50)))
	    , minObjects(std::max(1, object.Int("MinLightningObjects", 3)))
	    , atOnce(std::max(1, object.Int("MaxLightningObjectsAtOnce", 10)))
	    , maxJoints(std::max(2, object.Int("MaxJointsPerFork", 10)))
	    , splitAngle(object.Float("SplitAngle", std::numbers::pi_v<float> / 2.0f))
	    , randomFrac(object.Float("RandomFrac", 0.1f))
	    , forkScale(object.Float("ForkScale", 1.0f))
	    , forkScaleProvider(object.String("FP_ForkScale"))
	    , averageLightmapLife(object.Float("AverageLightmapLife", 0.5f))
	    , searchRadius(object.String("SearchRadius"))
	    , defaultSearchRadius(object.Float("DefaultSearchRadius", 20.0f))
	    , castingFromHand(object.Bool("CastingFromHand", true))
	    , takeTargetsFromManager(object.Bool("TakeTargetsFromManager", false))
	    , renewTargetsOnMove(object.Bool("RenewTargetsOnMove", false))
	    , renewTargetsOnMoveFrac(object.Float("RenewTargetsOnMoveFrac", 0.5f))
	    , renewSearchEvery(object.Float("RenewSearchEvery", 1.0f))
	    , numTexturesToTile(object.Int("NumTexturesToTile", -1))
	    , sound(ReadSoundAction(object, "SoundLightning"))
	{
	}

	[[nodiscard]] bool Creates() const override { return true; }

	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& slot) const override
	{
		auto& data = DataFor(effect, slot, castingFromHand);
		data.life += effect.GetDt();
		// the origin is the gesture position when cast from the hand, else the parent atom
		data.origin = castingFromHand                ? effect.GetProcessInfo().handPos
		              : collection.parent != nullptr ? effect.GlobalPosition(*collection.parent)
		                                             : effect.GetOrigin();
		// the heading the cone points at is the camera's forward, not the hand's velocity
		const auto& forward = effect.GetProcessInfo().cameraForward;
		if (forward.x != 0.0f || forward.z != 0.0f)
		{
			data.heading = std::atan2(forward.z, forward.x);
		}
		// ForkScale x FP_ForkScale
		data.forkScale = forkScale * (forkScaleProvider.empty() ? 1.0f : effect.FloatProvider(forkScaleProvider, 1.0f));
		if (effect.Closing())
		{
			// closing down: the forks go
			collection.atoms.clear();
			data.root = nullptr;
			data.first = true;
			return true;
		}
		if (data.first)
		{
			FindTargets(effect, data);
			CreateForkStructure(effect, collection, data);
			data.first = false;
		}
		ApplyGlow(effect, collection, data);
		// the clash with another bolt
		FindClash(effect, data);
		// the targets are searched again but the fork structure built on the first step stays; when there are more
		// targets than forks the recursion simply stops
		if (Renew(effect, data))
		{
			FindTargets(effect, data);
		}
		// with SoundLightning not NO_SOUND, every step the root atom's sound of that action is started when it has none
		// while the effect is enabled, and stopped when it has one otherwise
		if (sound.action != -1 && Valid(collection, data))
		{
			auto* playing = audio::spell_sounds::GetSoundOfAction(*data.root, sound.action);
			if (effect.GetProcessInfo().enabled)
			{
				if (playing == nullptr)
				{
					audio::spell_sounds::StartSound(effect, *data.root, sound);
				}
			}
			else if (playing != nullptr)
			{
				audio::spell_sounds::StopSound(*data.root, *playing);
			}
		}
		// every fork is detached from the root at every step, whatever the state: the port hides their joints. The
		// recursion attaches the ones it uses again
		if (Valid(collection, data))
		{
			for (auto& fork : data.root->subCollections)
			{
				for (auto& atom : fork->atoms)
				{
					atom->visible = false;
				}
			}
		}
		// UpdateForkStructure only while the effect is enabled
		if (effect.GetProcessInfo().enabled)
		{
			UpdateForkStructure(effect, collection, data);
		}
		return true;
	}

private:
	/// The root atom is still the collection's (a remove rule may have taken it)
	[[nodiscard]] static bool Valid(const Collection& collection, const Data& data)
	{
		return data.root != nullptr && !collection.atoms.empty() && collection.atoms.front().get() == data.root;
	}

	/// What Link asked for this bolt's fork 0, on the last joint of the first fork
	void ApplyGlow(Effect& effect, const Collection& collection, Data& data) const
	{
		const auto change = data.glow;
		data.glow = GlowChange::None;
		if (change == GlowChange::None || !Valid(collection, data) || data.root->subCollections.empty() ||
		    data.root->subCollections.front()->atoms.empty())
		{
			return;
		}
		auto& joint = *data.root->subCollections.front()->atoms.back();
		if (change == GlowChange::Remove)
		{
			joint.subCollections.clear();
			return;
		}
		// only on a joint with no sub-collection; a group of -1 is skipped. The new collection is drawn without
		// interpolation
		if (joint.subCollections.empty() && data.glowGroup != -1)
		{
			effect.AddSubCollections(joint, {data.glowGroup});
			if (!joint.subCollections.empty())
			{
				joint.subCollections.back()->flags &= static_cast<uint8_t>(~2u);
			}
		}
	}

	/// A bolt cast from the hand that is not linked yet looks for an older one (smaller createdTurn) in the list, also
	/// cast from the hand and with targets, not linked or linked to it. With d1 = that one's centroid - its origin and
	/// d2 = its centroid - this origin, the clash point is T = its centroid - 0.7 x 0.5 x (normalize(d1) + (cos h, 0,
	/// sin h)) x min(|d1|, |d2|), h this heading. They are linked (Link(older, this, CommonGlowGroup), this clash = T)
	/// when their headings agree (cos h' cos h + sin h' sin h > 0), T is closer than SearchRadius and
	/// normalize(T - this origin) lies within 1.0367 rad of h. An older bolt that fails the test is unlinked if it was
	/// linked. The first link ends the search
	void FindClash(Effect& effect, Data& data) const
	{
		if (!data.fromHand || data.link != 0)
		{
			return;
		}
		data.clash = glm::vec3(0.0f);
		const auto order = LightningData().order; // Link does not change the list, but a copy keeps the walk simple
		for (const auto key : order)
		{
			auto* other = Find(key);
			// cast from the hand, with targets, not linked to a third one
			if (other == nullptr || !other->fromHand || other->targets.empty() || (other->link != data.key && other->link != 0))
			{
				continue;
			}
			if (other->createdTurn < data.createdTurn) // the same turn does not clash
			{
				if (const auto clash = ClashPoint(effect, *other, data); clash.has_value())
				{
					data.clash = *clash;
					Link(*other, data.key, commonGlowGroup);
					return;
				}
			}
			if (other->link != 0)
			{
				Link(*other, 0, -1);
			}
		}
	}

	/// The geometry of FindClash; nullopt when the two do not meet
	[[nodiscard]] std::optional<glm::vec3> ClashPoint(const Effect& effect, const Data& older, const Data& data) const
	{
		// a zero vector stays zero with length 0
		const auto normalise = [](glm::vec3& v) {
			if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f)
			{
				return 0.0f;
			}
			const float length = std::sqrt((v.z * v.z + v.y * v.y) + v.x * v.x);
			const float inverse = 1.0f / length;
			v = glm::vec3(v.x * inverse, v.y * inverse, v.z * inverse);
			return length;
		};
		auto d1 = older.centroid - older.origin;
		auto d2 = older.centroid - data.origin;
		const float length1 = normalise(d1);
		const float length2 = normalise(d2);
		const float c = std::cos(data.heading);
		const float s = std::sin(data.heading);
		if (!(std::cos(older.heading) * c + std::sin(older.heading) * s > 0.0f))
		{
			return std::nullopt;
		}
		// (d1 + (c, 0, s)) x 0.5 x 0.7 x the shorter length (length1 when it is the smaller, else length2)
		const glm::vec3 sum(d1.x + c, d1.y, d1.z + s);
		const glm::vec3 half = sum * 0.5f;
		const glm::vec3 scaled = half * 0.7f;
		const float shorter = length1 < length2 ? length1 : length2;
		const glm::vec3 clash = older.centroid - scaled * shorter;
		auto toClash = clash - data.origin;
		const float distance = normalise(toClash); // normalises and returns the length
		if (!(SearchRadius(effect) > distance))
		{
			return std::nullopt;
		}
		// linked when the cosine of the limit angle (1.0367 rad, as a double) is below the dot
		if (!(static_cast<float>(std::cos(1.036725640296936)) < toClash.z * s + toClash.x * c))
		{
			return std::nullopt;
		}
		return clash;
	}

	/// The turns of AverageLightmapLife seconds, truncated, passed to RandomBelow as it is (a 0 ms turn gives +inf, which
	/// truncates to INT_MIN)
	[[nodiscard]] int LightmapSteps() const
	{
		return map_coords::FtoL(averageLightmapLife /
		                        (static_cast<float>(game_clock::MsPerTurn()) * game_clock::k_SecondsPerMs));
	}

	[[nodiscard]] float SearchRadius(const Effect& effect) const
	{
		return searchRadius.empty() ? defaultSearchRadius : effect.FloatProvider(searchRadius, defaultSearchRadius);
	}

	/// The targets are searched again when RenewSearchEvery > 0 and the age since the last search is past it, or with
	/// RenewTargetsOnMove when the origin moved more than RenewTargetsOnMoveFrac x SearchRadius from where it was
	/// searched
	[[nodiscard]] bool Renew(const Effect& effect, const Data& data) const
	{
		if (renewSearchEvery > 0.0f && data.life > renewSearchEvery)
		{
			return true;
		}
		if (!renewTargetsOnMove)
		{
			return false;
		}
		// d = searchOrigin - origin squared as (dz dz + dy dy) + dx dx, against (R x frac)^2: no root
		const auto d = data.searchOrigin - data.origin;
		const float distanceSq = (d.z * d.z + d.y * d.y) + d.x * d.x;
		const float limit = SearchRadius(effect) * renewTargetsOnMoveFrac;
		return limit * limit < distanceSq;
	}

	/// The three target modes, tested in this order (CastingFromHand, then TakeTargetsFromManager): from the hand (the
	/// cone), from the manager's spell targets and around the parent atom (the storm: no cone, a circle of the radius)
	void FindTargets(Effect& effect, Data& data) const
	{
		data.targets.clear();
		data.life = 0.0f;
		data.searchOrigin = data.origin;
		if (castingFromHand)
		{
			SearchAround(effect, data, true);
		}
		else if (takeTargetsFromManager)
		{
			for (const auto target : effect.GetTargets())
			{
				if (static_cast<int>(data.targets.size()) >= maxObjects)
				{
					break;
				}
				if (CanBeStruck(target))
				{
					data.targets.push_back({target, glm::vec3(0.0f), true, true, false, 0});
				}
			}
		}
		else
		{
			SearchAround(effect, data, false);
		}
		// (inferred) the manager mode gets the ground points too: the original's manager search was not read
		AddGroundPoints(effect, data, castingFromHand || takeTargetsFromManager);
		// every target starts with cooldown = RandomBelow(LightmapSteps())
		// with it, centroid = the tips of every target summed, x 1 / their number, and
		// centroidDistance = sqrt((dz dz + dy dy) + dx dx) from the origin to it
		const auto steps = LightmapSteps();
		glm::vec3 sum(0.0f);
		for (auto& target : data.targets)
		{
			target.cooldown = RandomBelow(effect, steps);
			sum += target.Tip();
		}
		const float inverse = 1.0f / static_cast<float>(data.targets.size());
		data.centroid = glm::vec3(inverse * sum.x, sum.y * inverse, sum.z * inverse);
		const auto d = data.centroid - data.origin;
		data.centroidDistance = std::sqrt((d.z * d.z + d.y * d.y) + d.x * d.x);
	}

	/// The spiral of 4 ceil(R/10)^2 cells around the origin; an object counts when it is available, not a spell seed,
	/// and its horizontal direction is inside the cone of half-angle SplitAngle about the heading. Without `cone`:
	/// ceil(R/10)^2 cells only (no x4), and an object counts when dx^2 + dz^2 < R^2. dx, dz are the object's MapCoords
	/// in metres minus the origin
	void SearchAround(Effect& effect, Data& data, bool cone) const
	{
		const float radius = SearchRadius(effect);
		const auto side = map_coords::FtoL(std::ceil(radius / 10.0f));
		const int cells = cone ? 4 * side * side : side * side;
		const float limit = std::cos(splitAngle);
		const glm::vec2 heading(std::cos(data.heading), std::sin(data.heading));
		// the origin's MapCoords (ToFixed of x and z), walked by a Spiral from dir = count = 1, one cell at a time
		auto cell = map_coords::FromMetres(glm::vec2(data.origin.x, data.origin.z));
		map_coords::Spiral spiral;
		std::vector<entt::entity> objects;
		for (int i = 0; i < cells && static_cast<int>(data.targets.size()) < maxObjects; ++i)
		{
			CellObjects(cell, objects);
			for (const auto object : objects)
			{
				if (static_cast<int>(data.targets.size()) >= maxObjects)
				{
					break;
				}
				// available, then the own cell and not a spell seed
				if (!IsAvailable(object) || !CanBeStruck(object))
				{
					continue;
				}
				const auto& transform = Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(object);
				// only in the cell its own MapCoords is in, so an object listed in several cells is found once
				const auto own = ecs::object::MapCoordsOf(object);
				if (!ecs::map_cells::IsOwnCell(own, map_coords::Cell(cell)))
				{
					continue;
				}
				float dx = map_coords::ToMetres(own.x) - data.origin.x;
				float dz = map_coords::ToMetres(own.z) - data.origin.z;
				bool outside = false;
				if (cone)
				{
					// (dx, 0, dz) normalised unless it is zero, then counted when
					// dx cos(heading) + sin(heading) dz > cos(SplitAngle)
					if (dx != 0.0f || dz != 0.0f)
					{
						const float inverse = 1.0f / std::sqrt(dx * dx + dz * dz);
						dx = dx * inverse;
						dz = dz * inverse;
					}
					outside = !(dx * heading.x + heading.y * dz > limit);
				}
				else
				{
					// dz dz + dx dx < R R
					outside = !(dz * dz + dx * dx < radius * radius);
				}
				if (outside)
				{
					continue;
				}
				data.targets.push_back({object, transform.position, true, true, false, 0});
			}
			map_coords::AddCells(cell, spiral.Next());
		}
	}

	/// When fewer than MinLightningObjects were found, the rest are ground points at heading + rand(pi/4) and
	/// rand(0.6 R) away, two metres above the land. Not `aimed` (the storm): at rand(2 pi) instead
	void AddGroundPoints(Effect& effect, Data& data, bool aimed) const
	{
		const float radius = SearchRadius(effect);
		while (static_cast<int>(data.targets.size()) < minObjects)
		{
			const float angle = aimed ? data.heading + effect.Random(std::numbers::pi_v<float> / 4.0f)
			                          : effect.Random(2.0f * std::numbers::pi_v<float>);
			const float distance = effect.Random(0.6f * radius);
			const glm::vec3 point(data.origin.x + distance * std::cos(angle), 0.0f, data.origin.z + distance * std::sin(angle));
			// the land's height at the point's MapCoords (ToFixed), + 2; x and z stay the float point
			const float ground = map_coords::ToWorld(map_coords::FromMetres(glm::vec2(point.x, point.z))).y;
			data.targets.push_back({entt::null, glm::vec3(point.x, ground + 2.0f, point.z), false, true, false, 0});
		}
	}

	/// One atom in the collection, with two fork sub-collections per target (the ForkGroup list of 2N), each holding
	/// MaxJointsPerFork chain joints
	void CreateForkStructure(Effect& effect, Collection& collection, Data& data) const
	{
		collection.atoms.clear();
		const std::vector<int> groups(2 * data.targets.size(), forkGroup);
		auto& root = effect.NewAtom(collection, effect.FindCreator(creator), {});
		root.visible = false; // (inferred) the root itself is not drawn: it only carries the forks
		effect.AddSubCollections(root, groups);
		const auto* joints = effect.FindCreator(creator);
		// cast from the player's own hand (HandOffset), every joint of every fork gets a draw offset, so the bolt's
		// start follows the hand between the steps
		const bool offset = HandOffset(effect);
		for (auto& fork : root.subCollections)
		{
			// the forks are drawn as the last step left them, not interpolated, so each step the bolt jumps to its new
			// shape
			fork->flags &= static_cast<uint8_t>(~2u);
			for (int i = 0; i < maxJoints; ++i)
			{
				auto& joint = effect.NewAtom(*fork, joints, {});
				if (offset)
				{
					joint.drawOffset.emplace();
				}
			}
		}
		data.root = &root;
	}

	/// CastingFromHand and the spell cast by this player's own hand (true without a spell)
	[[nodiscard]] bool HandOffset(const Effect& effect) const { return castingFromHand && effect.IsMyInterfaceCasting(); }

	/// The cooldowns tick down, a set of targets is picked, and the recursion (Fork) lays the joints of their forks out
	/// and fires the events
	void UpdateForkStructure(Effect& effect, Collection& collection, Data& data) const
	{
		if (!Valid(collection, data))
		{
			data.first = true; // the atoms went (a remove rule): build them again next step
			return;
		}
		// the cooldown counts down; nothing reads it to skip a target
		for (auto& target : data.targets)
		{
			if (target.cooldown > 0)
			{
				--target.cooldown;
			}
		}
		data.striking.clear();
		const auto total = static_cast<int>(data.targets.size());
		if (data.linkedBy != 0 || data.link != 0)
		{
			// a bolt in a clash strikes ONE target, the first active one from RandomBelow(N) on
			const int start = effect.Rand(total);
			for (int i = 0; i < total; ++i)
			{
				const int index = (start + i) % total;
				if (data.targets[static_cast<size_t>(index)].active)
				{
					data.striking.push_back(index);
					break;
				}
			}
		}
		else if (total > 0)
		{
			// each active target with probability min(MaxLightningObjectsAtOnce, (N + 1) / 2) / N, until that many
			// are picked
			const int limit = std::min(atOnce, (total + 1) / 2);
			const float probability = static_cast<float>(limit) / static_cast<float>(total);
			for (int i = 0; i < total && static_cast<int>(data.striking.size()) < limit; ++i)
			{
				if (data.targets[static_cast<size_t>(i)].active && effect.Random(1.0f) < probability)
				{
					data.striking.push_back(i);
				}
			}
		}
		// no forks or no target picked, no recursion (and so nothing drawn this step)
		if (data.root->subCollections.empty() || data.striking.empty())
		{
			return;
		}
		// fork 0 is the trunk, the next free fork is 1, depth 0, scale 1.0. The original also picks the collection of
		// fork RandomBelow(2 x forks) or none, which the recursion never uses: only its draw from the random stream is kept
		static_cast<void>(effect.Rand(static_cast<int32_t>(2 * data.root->subCollections.size())));
		size_t nextFork = 1;
		size_t used = 0;
		Fork(effect, data, 0, *data.root->subCollections.front(), data.origin, data.striking, 1.0f, nextFork, used);
		if (debug_env::SpellTrace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Lightning {}: {} targets ({} objects), {} struck, {} of {} forks drawn from ({:.1f}, {:.1f}, "
			                   "{:.1f}) heading {:.2f} rad, fork scale {:.2f}, link {} linked by {} clash ({:.1f}, {:.1f}, "
			                   "{:.1f})",
			                   data.key, total, std::ranges::count_if(data.targets, [](const Target& t) { return t.isObject; }),
			                   data.striking.size(), used, data.root->subCollections.size(), data.origin.x, data.origin.y,
			                   data.origin.z, data.heading, data.forkScale, data.link, data.linkedBy, data.clash.x,
			                   data.clash.y, data.clash.z);
		}
	}

	/// One fork of the tree, from `origin` to the split point of
	/// its targets, then two child forks from there, or the strike when it has a single target
	void Fork(Effect& effect, Data& data, int depth, Collection& fork, const glm::vec3& origin, const std::vector<int>& targets,
	          float scale, size_t& nextFork, size_t& used) const
	{
		// no fork (the structure ran out) or no target: nothing. The `joints < 2` is a port guard (MaxJointsPerFork
		// < 2): the original goes on with 0 atoms and would read the child origin from a null atom, and divides by
		// n - 1 = 0 with 1; no spell file gives fewer than 2
		const auto joints = static_cast<int>(fork.atoms.size());
		if (targets.empty() || joints < 2)
		{
			return;
		}
		// the fork is attached to the root again: from here it is drawn, with the joints the last step that laid it
		// out left, even when this one returns before laying it out (a missing draw offset; in the original also the
		// land)
		for (auto& atom : fork.atoms)
		{
			atom->visible = true;
		}
		++used;
		// cast from the player's own hand (HandOffset), each joint's draw offset gets SetReference with this step's origin
		// and the weight w - w i / (n - 1), w = 1 on the trunk (depth 0) and 0 on the child forks; a joint without one
		// ends the fork
		if (HandOffset(effect))
		{
			const float w = depth == 0 ? 1.0f : 0.0f;
			const float inverse = 1.0f / (static_cast<float>(joints) - 1.0f);
			for (int i = 0; i < joints; ++i)
			{
				auto& atom = *fork.atoms[static_cast<size_t>(i)];
				if (!atom.drawOffset.has_value())
				{
					return;
				}
				atom.drawOffset->SetReference(data.origin, -w * static_cast<float>(i) * inverse + w);
			}
		}

		// the centroid of the list: the tips of its active targets summed, over the list's size
		glm::vec3 centroid(0.0f);
		for (const int index : targets)
		{
			const auto& target = data.targets[static_cast<size_t>(index)];
			if (target.active)
			{
				centroid += target.Tip();
			}
		}
		centroid /= static_cast<float>(targets.size());
		// stop: the fork ends where it is laid out (no children, no strike); clashChild: the clash fork follows;
		// childScale: the scale the children get
		bool stop = false;
		bool clashChild = false;
		float childScale = scale;
		glm::vec3 split = centroid;
		const auto* linked = Find(data.link);
		if (depth == 0 && data.linkedBy != 0)
		{
			// a bolt another one is linked to ends its trunk at its own clash point
			split = data.clash;
			stop = true;
		}
		else if (depth == 0 && linked != nullptr)
		{
			// a bolt linked to another one ends its trunk at that one's clash point, and the fork after it gets the
			// scale x 3 (set by no property)
			split = linked->clash;
			childScale = scale * k_ClashScale;
			clashChild = true;
		}
		else if (targets.size() >= 2)
		{
			// with two or more targets the fork stops at origin + (centroid - origin) x (0.2 + rand(0.4)); with one it
			// goes all the way to it
			const float f = 0.2f + effect.Random(0.4f);
			split = origin + (centroid - origin) * f;
		}
		// LandIslandInterface::RayCast(origin, split): the RAY from the origin through the split point to the map's
		// edge, or its y = 0 point within 7500 m of the camera. When it meets the land closer to the origin in x z than
		// the split point, `(hz - oz)^2 + (hx - ox)^2 < (sz - oz)^2 + (sx - ox)^2`, the whole fork ends: it is not laid
		// out again (it keeps the joints of the last step), no children, no strike, no shield test
		if (Locator::terrainSystem::has_value())
		{
			// the camera for the y = 0 point; (openblack) the origin when there is no camera (tests)
			const glm::vec3 camera = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : origin;
			glm::vec2 land(0.0f);
			if (Locator::terrainSystem::value().RayCast(origin, split, land, camera))
			{
				const float splitX = split.x - origin.x;
				const float splitZ = split.z - origin.z;
				const float landX = land.x - origin.x;
				const float landZ = land.y - origin.z;
				if (landZ * landZ + landX * landX < splitZ * splitZ + splitX * splitX)
				{
					if (debug_env::SpellTrace())
					{
						SPDLOG_LOGGER_INFO(spdlog::get("game"),
						                   "Lightning {}: fork at depth {} from ({:.1f}, {:.1f}, {:.1f}) cut by the land at "
						                   "({:.1f}, {:.1f}) before its split point ({:.1f}, {:.1f}, {:.1f})",
						                   data.key, depth, origin.x, origin.y, origin.z, land.x, land.y, split.x, split.y,
						                   split.z);
					}
					return;
				}
			}
		}

		// a split point inside a shield (margin 2.5): the segment's way in (FindIntersect from the origin), a HitSpell
		// event there {no movement, strength 1 (2 when linked), no shield test, target = the shield's spell} to this
		// bolt's spell; a 0 (the shield held) ends the fork there (split = the hit, stop). The shield gets its spark
		// either way
		if (const auto* sphere = shields::FindShieldContainingPoint(split, k_ShieldMargin); sphere != nullptr)
		{
			glm::vec3 hit = split;
			shields::FindIntersect(*sphere, origin, split, k_ShieldMargin, hit);
			const SpellEventInfo event {.type = SpellEventInfo::Type::HitSpell,
			                            .position = hit,
			                            .velocity = glm::vec3(0.0f),
			                            .strength = (data.link != 0 ? 2.0f : 1.0f) * 1.0f,
			                            .checkShields = false,
			                            .target = shields::SpellOf(*sphere)};
			if (effect.SendSpellEvent(event) == 0)
			{
				split = hit;
				stop = true;
			}
			shields::AddImpactTarget(*sphere, hit);
		}

		// the fork's scale runs from S / (depth + 1) to S / (depth + 2), S = scale x ForkScale x FP_ForkScale
		const float s = scale * data.forkScale;
		const float scaleFrom = s / static_cast<float>(depth + 1);
		const float scaleTo = s / static_cast<float>(depth + 2);
		const auto direction = split - origin;
		// sqrt((dz dz + dy dy) + dx dx), in this order
		const float length = std::sqrt((direction.z * direction.z + direction.y * direction.y) + direction.x * direction.x);
		// with NumTexturesToTile != -1 the fork's chain is cut in max(1, trunc(NumTexturesToTile x length /
		// centroidDistance)) repeats; centroidDistance is the distance from the origin to the targets' centroid
		// (FindTargets). SF_LightningStrike / SF_LightningStormPush give 15
		if (numTexturesToTile != -1)
		{
			fork.chainTextures =
			    std::max(1, map_coords::FtoL(static_cast<float>(numTexturesToTile) * length / data.centroidDistance));
		}
		// the alpha is 255 only when the scale is > 1 (a clash), else 128 + RandomBelow(127)
		const bool opaque = scale > 1.0f;
		const float step = 1.0f / static_cast<float>(joints - 1);
		for (int i = 0; i < joints; ++i)
		{
			auto& atom = *fork.atoms[static_cast<size_t>(i)];
			const float t = static_cast<float>(i) * step;
			// origin + (split - origin) x i / (n - 1)
			auto position = origin + direction * t;
			if (i != 0 && i != joints - 1)
			{
				// the inner joints move by (rand(2 RandomFrac) - RandomFrac) x length in X and Z
				position.x += (effect.Random(2.0f * randomFrac) - randomFrac) * length;
				position.z += (effect.Random(2.0f * randomFrac) - randomFrac) * length;
			}
			atom.position = position;
			atom.ruleScale = scaleFrom + (scaleTo - scaleFrom) * t;
			// the colour's alpha byte: 255, or RandomBelow(127) + 128
			atom.colour[3] = opaque ? 255 : static_cast<uint8_t>(effect.Rand(0x7F) + 0x80);
		}
		const glm::vec3 end = fork.atoms.back()->position; // the last joint
		if (targets.size() >= 2 && !stop)
		{
			// the targets are split by the sign of dot((tip - split).xz, (centroid - origin).xz):
			// > 0 to the first list, else to the second
			std::vector<int> ahead;
			std::vector<int> behind;
			const glm::vec2 axis(centroid.x - origin.x, centroid.z - origin.z);
			for (const int index : targets)
			{
				const auto tip = data.targets[static_cast<size_t>(index)].Tip();
				const float dot = (tip.x - split.x) * axis.x + (tip.z - split.z) * axis.y;
				(dot > 0.0f ? ahead : behind).push_back(index);
			}
			// an empty list takes the last target of the other
			if (ahead.empty())
			{
				ahead.push_back(behind.back());
				behind.pop_back();
			}
			else if (behind.empty())
			{
				behind.push_back(ahead.back());
				ahead.pop_back();
			}
			// each list gets the next free fork, depth first, with childScale; when the forks run out the recursion
			// stops there
			for (const auto* list : {&ahead, &behind})
			{
				if (nextFork >= data.root->subCollections.size())
				{
					return;
				}
				auto& child = *data.root->subCollections[nextFork++];
				Fork(effect, data, depth + 1, child, end, *list, childScale, nextFork, used);
			}
			return;
		}
		if (targets.size() == 1 && clashChild)
		{
			// the linked bolt goes on from the clash point to its one target: a new list with it, the next free fork
			// (none: stop), depth + 1, the scale x 3 (so opaque and thick)
			if (nextFork >= data.root->subCollections.size())
			{
				return;
			}
			auto& child = *data.root->subCollections[nextFork++];
			Fork(effect, data, depth + 1, child, end, targets, childScale, nextFork, used);
			return;
		}
		// ended by a clash or a shield
		if (stop)
		{
			return;
		}
		// one target: the strike
		StrikeTarget(effect, data, data.targets[static_cast<size_t>(targets.front())], centroid);
	}

	/// The tip of a fork: the cooldown, the light map atom at the list's centroid, the "impressive" report once 0.2 s
	/// passed since the search and the event that actually damages
	void StrikeTarget(Effect& effect, Data& data, Target& target, const glm::vec3& centroid) const
	{
		// cooldown = RandomBelow(LightmapSteps()); data only
		const auto steps = LightmapSteps();
		target.cooldown = RandomBelow(effect, steps);
		if (lightMapGroup >= 0)
		{
			if (auto* atom = effect.NewAtomInGroup(lightMapGroup, effect.FindCreator(lightMapCreator)); atom != nullptr)
			{
				// the original blits the light map into the landscape's light texture under the point, so it always
				// ends up on the ground however high the tip is; the +0.1 is the port's (inferred)
				atom->position = glm::vec3(centroid.x, LandAt(centroid.x, centroid.z) + 0.1f, centroid.z);
			}
		}
		// once per target, when the search is older than 0.2 s
		if (!target.lightMapDone && data.life > 0.2f)
		{
			// TODO: the "impressive" report of the struck object (its impressive intensity)
			target.lightMapDone = true;
		}
		// a Landed SpellEventInfo at the target's tip, no movement, no shield test; strength 1, 2 when the bolt is
		// linked to another one
		const SpellEventInfo event {.type = SpellEventInfo::Type::Landed,
		                            .position = target.Tip(),
		                            .velocity = glm::vec3(0.0f),
		                            .strength = data.link != 0 ? 2.0f : 1.0f,
		                            .checkShields = false};
		if (effect.GetSink() != nullptr)
		{
			effect.SendSpellEvent(event);
		}
		// TODO: without a spell (a script / climate strike) the original applies a fixed set of effect values at
		// the tip. UNVERIFIED which effect info row that is.

		// the same event also goes to the manager of the bolt this one is linked to
		if (const auto* linked = Find(data.link); linked != nullptr)
		{
			if (auto* other = EffectOf(*linked); other != nullptr)
			{
				other->SendSpellEvent(event);
			}
		}
	}

	std::string creator;
	std::string lightMapCreator;
	int forkGroup;
	int commonGlowGroup;
	int lightMapGroup;
	int maxObjects;
	int minObjects;
	int atOnce;
	int maxJoints;
	float splitAngle;
	float randomFrac;
	float forkScale;
	std::string forkScaleProvider;
	float averageLightmapLife;
	std::string searchRadius;
	float defaultSearchRadius;
	bool castingFromHand;
	bool takeTargetsFromManager;
	bool renewTargetsOnMove;
	float renewTargetsOnMoveFrac;
	float renewSearchEvery;
	int numTexturesToTile; ///< -1 by default
	SoundAction sound;
};

/// UR_LightningStrike (SF_LightningStrike / SF_LightningSingleStrike, the script and climate strike): one atom
/// with its NextGroups (where the UR_Lightning of the strike lives) and SOUND_SPELL_LIGHTNING, once
class LightningStrike final: public Modifier
{
public:
	explicit LightningStrike(const Object& object)
	    : creator(object.String("PCreator"))
	    , nextGroups(object.Array("NextGroups"))
	    , sound(ReadSoundAction(object, "SoundLightning"))
	{
	}
	[[nodiscard]] bool Creates() const override { return true; }
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		auto& atom = effect.NewAtom(collection, effect.FindCreator(creator), nextGroups);
		audio::spell_sounds::StartSound(effect, atom, sound);
		return false;
	}
	std::string creator;
	std::vector<int> nextGroups;
	SoundAction sound;
};
} // namespace

void openblack::psys::RegisterLightningRules()
{
	RegisterModifier("UR_Lightning", MakeModifierOf<Lightning>);
	RegisterModifier("UR_LightningStrike", MakeModifierOf<LightningStrike>);
}
