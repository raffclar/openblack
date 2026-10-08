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

#include <deque>
#include <list>
#include <memory>
#include <optional>
#include <set>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"
#include "Particles/PSys.h"
#include "Particles/PSysManager.h"
#include "Particles/PSysRegistry.h"
#include "Particles/Rules/ExplodeObject.h"
#include "Particles/Rules/Shield.h"
#include "Particles/Utility.h"

namespace openblack::psys::manager
{
struct Running
{
	std::unique_ptr<Effect> effect;
	bool ownedBySpell {false};        ///< stepped by its spell (StartForSpell), not by ProcessTurn
	bool perFrame {false};            ///< stepped every frame (the hand's and the interface's effects): drawn as last stepped
	bool inContainer {false};         ///< a spot visual container's (CreateSpotVisual): stepped by its container in ProcessTurn
	DrawPath path {DrawPath::Sorted}; ///< SetDrawPath; Sorted, as a spell draws it, until changed
};

struct Container
{
	uint32_t effect {0};
	entt::entity object {entt::null}; ///< the script's handle
	entt::entity owner {entt::null};
	int turns {-1}; ///< -1: forever
	bool hadOwner {false};
	/// the map coordinates it was created at: its effect's place every turn. Nothing moves it
	map_coords::MapCoords coords {};
	/// the script handle's Transform when `coords` was last taken from it: setting the container's position (e.g. a
	/// script's) moves `coords`, which openblack sees as that Transform changing
	glm::vec3 placed {0.0f};
};

/// The running effects in the order the game's owner lists walk them: every owner (the containers, the spells, the
/// map shields, the seed graphics, the vortices) puts a new one at the head and walks from the head, so the newest comes
/// first; looked up by id
class EffectList
{
public:
	using List = std::list<std::pair<const uint32_t, Running>>;
	using iterator = List::iterator;

	/// the effect of `id`, made at the head when there is none
	Running& operator[](uint32_t id)
	{
		if (const auto it = find(id); it != end())
		{
			return it->second;
		}
		_list.emplace_front(std::piecewise_construct, std::forward_as_tuple(id), std::forward_as_tuple());
		_index[id] = _list.begin();
		return _list.front().second;
	}
	iterator find(uint32_t id)
	{
		const auto it = _index.find(id);
		return it == _index.end() ? _list.end() : it->second;
	}
	iterator erase(iterator it)
	{
		_index.erase(it->first);
		return _list.erase(it);
	}
	void erase(uint32_t id)
	{
		if (const auto it = find(id); it != end())
		{
			erase(it);
		}
	}
	void clear()
	{
		_list.clear();
		_index.clear();
	}
	iterator begin() { return _list.begin(); }
	iterator end() { return _list.end(); }

private:
	List _list;
	std::unordered_map<uint32_t, iterator> _index;
};

/// What psys::manager and the shield rules keep between calls (ecs::systems::ParticleSystemInterface owns it). The
/// spheres come first so that they are destroyed after the effects, whose destructors remove theirs
struct State
{
	std::vector<shields::DefensiveSphere> spheres; ///< the newest first
	uint32_t nextSphere {1};
	std::vector<DrawableSource> drawableSources;
	EffectList effects;
	/// the spot visual containers: a new one at the head
	std::deque<Container> containers;
	/// not reset by Clear: effect ids keep counting across lands
	uint32_t nextId {1};
	bool debugDone {false};

	// the rules' state
	std::vector<explode_object::QueuedMesh> explodeQueue;  ///< UR_ExplodeObject's meshes waiting for their effect
	std::vector<explode_object::QueuedMesh> explodeQueue2; ///< UR_ExplodeObject2's
	uint32_t explodeEffect {0};                            ///< the EXPLODE_OBJECT effect, made when a mesh is queued
	utility::UtilityEffects utility;
	std::vector<magic::gestures::RecognisedGesture> pendingGestures; ///< for UR_GesturingRecognised

	// the rule and creator factories, registered on first use (PSysRegistry.cpp); not reset by Clear
	FactoryRegistry factories;
	bool factoriesFilled {false};
	/// The town centre abode types (TownBelief.cpp), from the info constants on first use; not reset by Clear
	std::optional<std::set<AbodeNumber>> townCentreTypes;
};
} // namespace openblack::psys::manager
