/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PSysManager.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <chrono>
#include <deque>
#include <iterator>
#include <list>
#include <memory>
#include <optional>
#include <string_view>
#include <tuple>
#include <unordered_map>

#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Audio/Services/SpellSounds.h"
#include "Camera/Camera.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Particles/Creators/Mist.h"
#include "Particles/PSysManagerState.h"
#include "TownBelief.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
/// The spot visual info (info.dat DETAIL_SPOT_VISUAL) -> PARTICLE_TYPE -> spell file, and the default life in turns.
/// Empty: no spell file (the effect is drawn by other code or not at all).
struct SpotVisual
{
	std::string_view file;
	int life;
};
constexpr std::array<SpotVisual, 50> k_SpotVisuals = {{
    {"", 0},                            // 0 NONE
    {"", 100},                          // 1 APPLY_SPELL_EFFECT
    {"SF_GripLandscape", 100},          // 2 GRIP_LANDSCAPE
    {"", 100},                          // 3 SUCEED_CAST
    {"SF_FailedApply", 100},            // 4 FAIL_CAST
    {"", 200},                          // 5 FIREWORK_SINGLE
    {"SF_FireWorks", 200},              // 6 FIREWORKS
    {"SF_FireWorks", 200},              // 7 FIREWORKS_PU1
    {"SF_FireWorks", 200},              // 8 FIREWORKS_PU2
    {"SF_MagicObjectCreated", 100},     // 9 MAGIC_OBJECT_CREATED
    {"SF_CreatureTarget", 100},         // 10 COMMAND_SUCCEED
    {"SF_MagicObjectCreated", 100},     // 11 COMMAND_FAIL
    {"SF_CreatureTarget", -1},          // 12 CREATURE_TARGET
    {"SF_SimpleBeamCreatureCast", -1},  // 13 CREATURE_CAST_VISUAL
    {"SF_TeleportVillager", 30},        // 14 VILLAGER_TELEPORT
    {"", 30},                           // 15 FIRE_FX
    {"", 30},                           // 16 FIRE_FX_ON_OBJECT
    {"SF_VolFX", 30},                   // 17 MAGIC_FX
    {"SF_VolFXArtifact", -1},           // 18 MAGIC_FX_ON_OBJECT
    {"SF_VolFXCitadel", -1},            // 19 MAGIC_FX_ON_CITADEL
    {"SF_SimpleBeam", 30},              // 20 MAGIC_BEAM
    {"SF_SimpleBeamCitadel", -1},       // 21 MAGIC_BEAM_ON_CITADEL
    {"SF_Steam", 30},                   // 22 STEAM
    {"SF_Smoke", 30},                   // 23 SMOKE
    {"", 30},                           // 24 DUST
    {"SF_Bonfire", 30},                 // 25 BONFIRE
    {"SF_EvilSmoke", 30},               // 26 EVIL_SMOKE
    {"SF_MagicObjectCreated2", 30},     // 27 OBJECT_APPEAR
    {"", 30},                           // 28 OBJECT_DISAPPEAR
    {"SF_SmokeExplode", 30},            // 29 BANG
    {"", 30},                           // 30 SING_STONES_GLOW
    {"SF_PlayerIconFountain", 60},      // 31 PLAYER_ICON_FOUNTAIN
    {"SF_BeamExplosionCitadel", -1},    // 32 EXPLOSION_CITADEL
    {"SF_HealChakra", 60},              // 33 HEAL_FX
    {"SF_HighlightOnObject", -1},       // 34 HIGHLIGHT_ON_OBJECT
    {"SF_LightningSingleStrike", 40},   // 35 LIGHTNING_STRIKE
    {"SF_BeamExplosionFX", 60},         // 36 BEAM_EXPLOSION_FX
    {"SF_Butterflies", 200},            // 37 BUTTERFLIES
    {"SF_ButterfliesOnObject", 200},    // 38 BUTTERFLIES_ON_OBJECT
    {"SF_Flies", 200},                  // 39 FLIES
    {"SF_FliesOnObject", 200},          // 40 FLIES_ON_OBJECT
    {"SF_SimpleBeamCreatureSwap", 200}, // 41 MAGIC_BEAM_CREATURE_SWAP
    {"SF_Flash", 50},                   // 42 FLASH
    {"SF_TickerTape", 100},             // 43 TICKER_TAPE
    {"SF_ForestCreated", 50},           // 44 FOREST_CREATED
    {"SF_SingingStonesHeal", 100},      // 45 SINGING_STONES_HEAL
    {"SF_SparklesFromObject", 100},     // 46 PILEFOOD_SPEEDUP
    {"SF_SeeThisBeam", 100},            // 47 SEE_THIS_BEAM
    {"", 100},                          // 48 SEE_THIS_BEAM2
    {"", 100},                          // 49 TEST
}};

using manager::Container;
using manager::EffectList;
using manager::Running;

/// The particle system's state (Locator::particleSystem)
manager::State& ManagerState()
{
	return Locator::particleSystem::value().GetState();
}

/// OPENBLACK_PSYS_TRACE's turn count, in the debug hooks' store (Locator::debugHooks)
struct ManagerDebugHooks
{
	uint32_t turn {0};
};

ManagerDebugHooks& ManagerDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("psys manager: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<ManagerDebugHooks>();
}
} // namespace

uint32_t manager::Start(const std::string& file, glm::vec3 origin, float magnitude, game_random::psys::NetGameType type)
{
	return Start(File::Load(file), origin, magnitude, type);
}

uint32_t manager::Start(std::shared_ptr<const File> data, glm::vec3 origin, float magnitude,
                        game_random::psys::NetGameType type)
{
	if (!data)
	{
		return 0;
	}
	const uint32_t id = ManagerState().nextId++;
	ManagerState().effects[id].effect = std::make_unique<Effect>(std::move(data), origin, magnitude, type);
	return id;
}

uint32_t manager::StartForSpell(const std::string& file, glm::vec3 origin, glm::vec3 direction, float magnitude,
                                SpellSink* sink, game_random::psys::NetGameType type)
{
	const uint32_t id = Start(file, origin, magnitude, type);
	if (id == 0)
	{
		return 0;
	}
	auto& running = ManagerState().effects[id];
	running.ownedBySpell = true;
	running.effect->SetDirection(direction);
	running.effect->SetSink(sink);
	return id;
}

bool manager::ProcessForSpell(uint32_t id, const ProcessInfo& info, float dt)
{
	const auto it = ManagerState().effects.find(id);
	if (it == ManagerState().effects.end())
	{
		return false;
	}
	auto& effect = *it->second.effect;
	effect.SetProcessInfo(info);
	effect.Step(dt);
	if (effect.Finished() || (effect.Closing() && effect.DeleteOnCloseDown()))
	{
		ManagerState().effects.erase(it);
		return false;
	}
	return true;
}

void manager::Delete(uint32_t id)
{
	ManagerState().effects.erase(id);
}

void manager::SetPerFrame(uint32_t id)
{
	if (const auto it = ManagerState().effects.find(id); it != ManagerState().effects.end())
	{
		it->second.perFrame = true;
	}
}

void manager::SetDrawPath(uint32_t id, DrawPath path)
{
	if (const auto it = ManagerState().effects.find(id); it != ManagerState().effects.end())
	{
		it->second.path = path;
	}
}

manager::DrawPath manager::GetDrawPath(uint32_t id)
{
	const auto it = ManagerState().effects.find(id);
	return it == ManagerState().effects.end() ? DrawPath::Sorted : it->second.path;
}

manager::DrawPath manager::SpotVisualDrawPath(uint32_t singleZSort)
{
	// SingleZSort == 1: the whole effect as one Z object
	return singleZSort == 1 ? DrawPath::Queued : DrawPath::Sorted;
}

Effect* manager::Find(uint32_t id)
{
	const auto it = ManagerState().effects.find(id);
	return it == ManagerState().effects.end() ? nullptr : it->second.effect.get();
}

uint32_t manager::IdOf(const Effect* effect)
{
	for (const auto& [id, running] : ManagerState().effects)
	{
		if (running.effect.get() == effect)
		{
			return id;
		}
	}
	return 0;
}

void manager::CloseDown(uint32_t id)
{
	if (const auto it = ManagerState().effects.find(id); it != ManagerState().effects.end())
	{
		it->second.effect->CloseDown();
	}
}

void manager::SetOrigin(uint32_t id, glm::vec3 origin)
{
	if (const auto it = ManagerState().effects.find(id); it != ManagerState().effects.end())
	{
		it->second.effect->SetOrigin(origin);
	}
}

namespace
{
/// The container of CreateSpotVisual / CreateSpotVisualTurns, for `turns` (nullopt: the entry's own life)
entt::entity CreateSpotVisualFor(int spotVisual, glm::vec3 position, std::optional<int> turns, entt::entity owner,
                                 float magnitude)
{
	if (spotVisual < 0 || spotVisual >= static_cast<int>(k_SpotVisuals.size()))
	{
		return entt::null;
	}
	const auto& info = k_SpotVisuals[static_cast<size_t>(spotVisual)];
	if (info.file.empty())
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "PSys: spot visual {} has no spell file", spotVisual);
		return entt::null;
	}
	// a synced effect (NET_GAME_TYPE 1), at the point of the map coordinates the container keeps
	const auto coords = map_coords::FromWorld(position);
	const uint32_t id =
	    manager::Start(std::string(info.file), map_coords::ToWorld(coords), magnitude, game_random::psys::NetGameType::Synced);
	if (id == 0)
	{
		return entt::null;
	}
	// the container draws its effect by the entry's SingleZSort.
	// (inferred) without the info block, 1: every entry of info.dat that has a spell file has SingleZSort 1
	const uint32_t singleZSort =
	    Locator::infoConstants::has_value()
	        ? Locator::infoConstants::value().spotVisual.at(static_cast<size_t>(spotVisual)).singleZSort
	        : 1u;
	manager::SetDrawPath(id, manager::SpotVisualDrawPath(singleZSort));
	auto& registry = Locator::entitiesRegistry::value();
	const auto object = registry.Create();
	registry.Assign<ecs::components::Transform>(object, position, glm::mat3(1.0f), glm::vec3(1.0f));
	const int life = turns.value_or(info.life);
	ManagerState().effects[id].inContainer = true;
	ManagerState().containers.push_front({id, object, owner, life, owner != entt::null, coords, position});
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "PSys: spot visual {} ({}) at ({:.1f}, {:.1f}, {:.1f}) for {} turns", spotVisual,
	                   info.file, position.x, position.y, position.z, life);
	return object;
}
} // namespace

entt::entity manager::CreateSpotVisual(int spotVisual, glm::vec3 position, float seconds, entt::entity owner, float magnitude)
{
	// turns = trunc(seconds x 1000 / turn ms); 0 takes the entry's life
	std::optional<int> turns;
	if (seconds < 0.0f)
	{
		turns = -1;
	}
	else if (seconds > 0.0f)
	{
		turns = static_cast<int>(seconds * 1000.0f / static_cast<float>(game_clock::MsPerTurn()));
	}
	return CreateSpotVisualFor(spotVisual, position, turns, owner, magnitude);
}

entt::entity manager::CreateSpotVisualTurns(int spotVisual, glm::vec3 position, int turns, entt::entity owner, float magnitude)
{
	// the turns go straight to the container, unlike CreateSpotVisual, which passes the entry's life for 0. Each Process:
	// < 0 forever, else decremented and closed when <= 0, so 0 closes it at its first Process (not the entry's life)
	return CreateSpotVisualFor(spotVisual, position, std::optional(turns < 0 ? -1 : turns), owner, magnitude);
}

void manager::CloseSpotVisual(entt::entity object)
{
	// the container's effect closes down on the next turn, as when a script deletes it (ProcessTurn)
	auto& registry = Locator::entitiesRegistry::value();
	if (object != entt::null && registry.Valid(object))
	{
		registry.Destroy(object);
	}
}

void manager::ProcessTurn(float turnSeconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	static const bool trace = std::getenv("OPENBLACK_PSYS_TRACE") != nullptr;
	// the turn count is only read by the trace
	const uint32_t turn = trace ? ++ManagerDebugHooksData().turn : 0;
	// an effect is deleted when finished, or at once on close-down with DeleteOnCloseDown
	const auto step = [turnSeconds, turn](uint32_t id, Effect& effect) {
		effect.Step(turnSeconds);
		if (trace && turn % 20 == 0)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "PSys trace: effect {} {} age {:.1f} atoms {} closing {}", id,
			                   effect.GetFile().name, effect.GetAge(), effect.AtomCount(), effect.Closing());
		}
		return effect.Finished() || (effect.Closing() && effect.DeleteOnCloseDown());
	};
	// the containers from the head (the newest), the next taken first; each closes its effect when its owner has gone
	// or its turns are over, sets its origin and steps it; a finished effect deletes the container with it
	for (auto it = ManagerState().containers.begin(); it != ManagerState().containers.end();)
	{
		const auto effect = ManagerState().effects.find(it->effect);
		if (effect == ManagerState().effects.end())
		{
			if (registry.Valid(it->object))
			{
				registry.Destroy(it->object);
			}
			it = ManagerState().containers.erase(it);
			continue;
		}
		const bool ownerGone = it->hadOwner && !ecs::IsAvailable(it->owner);
		const bool deleted = !registry.Valid(it->object);
		if (ownerGone || deleted)
		{
			effect->second.effect->CloseDown();
		}
		else if (it->turns >= 0 && --it->turns <= 0)
		{
			effect->second.effect->CloseDown();
		}
		// the effect at the container's own map coordinates, the y read from the land again; the owner is only tested
		// for its availability above. Nothing moves the coordinates, so the effect does not follow its owner
		if (!deleted)
		{
			if (const auto* transform = registry.TryGet<const ecs::components::Transform>(it->object);
			    transform != nullptr && transform->position != it->placed)
			{
				it->coords = map_coords::FromWorld(transform->position); // the container was moved (SetPos)
				it->placed = transform->position;
			}
			effect->second.effect->SetOrigin(map_coords::ToWorld(it->coords));
		}
		if (step(effect->first, *effect->second.effect))
		{
			ManagerState().effects.erase(effect);
			if (registry.Valid(it->object))
			{
				registry.Destroy(it->object);
			}
			it = ManagerState().containers.erase(it);
			continue;
		}
		++it;
	}
	// The effects of no container and no spell (the spell dispensers', the flying flock's cast, a test effect): stepped
	// here once a turn, newest first. (pending) the original steps those two when their owner draws, with the frame's
	// ms
	for (auto it = ManagerState().effects.begin(); it != ManagerState().effects.end();)
	{
		if (it->second.ownedBySpell || it->second.inContainer)
		{
			++it;
			continue;
		}
		if (step(it->first, *it->second.effect))
		{
			it = ManagerState().effects.erase(it);
			continue;
		}
		++it;
	}
}

void manager::RunDebugHooks()
{
	// OPENBLACK_TEST_PSYS="SF_Name,x,z[,height[,magnitude[,seconds]]]": that spell file at (x, ground + height, z);
	// seconds > 0 closes it down after that long
	if (ManagerState().debugDone || !Locator::terrainSystem::has_value())
	{
		return;
	}
	ManagerState().debugDone = true;
	const char* test = std::getenv("OPENBLACK_TEST_PSYS");
	if (test == nullptr)
	{
		return;
	}
	std::array<char, 64> name = {};
	float x = 0.0f, z = 0.0f, height = 0.0f, magnitude = 1.0f, seconds = -1.0f;
	if (std::sscanf(test, "%63[^,],%f,%f,%f,%f,%f", name.data(), &x, &z, &height, &magnitude, &seconds) >= 3)
	{
		const float ground = Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z));
		const auto id = Start(name.data(), glm::vec3(x, ground + height, z), magnitude);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "PSys test: {} at ({}, {}, {}) -> effect {}", name.data(), x, ground + height,
		                   z, id);
		if (id != 0 && seconds > 0.0f)
		{
			auto& registry = Locator::entitiesRegistry::value();
			const auto object = registry.Create();
			registry.Assign<ecs::components::Transform>(object, glm::vec3(x, ground + height, z), glm::mat3(1.0f),
			                                            glm::vec3(1.0f));
			ManagerState().effects[id].inContainer = true;
			ManagerState().containers.push_front({id, object, entt::null, game_clock::TicksForSeconds(seconds), false});
		}
	}
}

void manager::Clear()
{
	ManagerState().effects.clear();
	ManagerState().containers.clear();
	ManagerState().debugDone = false;
	town_belief::Clear();
	audio::spell_sounds::Clear();
}

namespace
{
std::vector<manager::DrawableSource>& DrawableSources()
{
	return ManagerState().drawableSources;
}
} // namespace

manager::FrameInputs manager::LiveInputs()
{
	FrameInputs inputs;
	inputs.turnFraction = game_clock::TurnFraction();
	if (Locator::camera::has_value())
	{
		inputs.camera = Locator::camera::value().GetOrigin();
	}
	if (Locator::handSystem::has_value())
	{
		inputs.hand = glm::vec3(Locator::handSystem::value().GetHandMatrix()[3]);
	}
	return inputs;
}

std::vector<manager::Drawable> manager::Collect(Creator::Kind kind, const FrameInputs& inputs)
{
	// the fraction of the game turn the effects are drawn at
	const float t = inputs.turnFraction;
	std::vector<Drawable> result;
	for (const auto& [id, running] : ManagerState().effects)
	{
		Drawable drawable {running.effect->GetOrigin(), {}, running.perFrame ? 1.0f : t, running.path, id};
		running.effect->Collect(drawable.t, drawable.atoms, kind, &inputs.hand);
		if (!drawable.atoms.empty())
		{
			result.push_back(std::move(drawable));
		}
	}
	if (kind == Creator::Kind::Sprite && inputs.camera.has_value() && Locator::entitiesRegistry::has_value())
	{
		town_belief::Collect(*inputs.camera, result);
	}
	for (const auto source : DrawableSources())
	{
		source(result);
	}
	return result;
}

std::vector<Effect::DrawChain> manager::CollectChains(const FrameInputs& inputs)
{
	// the fraction of the game turn the effects are drawn at
	const float t = inputs.turnFraction;
	std::vector<Effect::DrawChain> result;
	for (const auto& [id, running] : ManagerState().effects)
	{
		const size_t first = result.size();
		running.effect->CollectChains(running.perFrame ? 1.0f : t, result, &inputs.hand);
		for (size_t i = first; i < result.size(); ++i)
		{
			result[i].path = running.path;
			result[i].effect = id;
		}
	}
	return result;
}

void manager::AddDrawableSource(DrawableSource source)
{
	DrawableSources().push_back(source);
}

namespace
{
/// The town belief and the DrawableSources, as Collect appends them to the sprites
std::vector<manager::Drawable> CollectSources(const manager::FrameInputs& inputs)
{
	std::vector<manager::Drawable> result;
	if (inputs.camera.has_value() && Locator::entitiesRegistry::has_value())
	{
		town_belief::Collect(*inputs.camera, result);
	}
	for (const auto source : DrawableSources())
	{
		source(result);
	}
	return result;
}

/// The ordered walk (Effect::CollectOrdered) of every effect of one path
std::vector<manager::OrderedEffect> CollectOrderedOf(manager::DrawPath path, const manager::FrameInputs& inputs)
{
	// the fraction of the game turn the effects are drawn at
	const float t = inputs.turnFraction;
	std::vector<manager::OrderedEffect> result;
	for (const auto& [id, running] : ManagerState().effects)
	{
		if (running.path != path)
		{
			continue;
		}
		manager::OrderedEffect effect {id, path, running.effect->GetOrigin(), running.perFrame ? 1.0f : t, {}, {}};
		running.effect->CollectOrdered(effect.t, effect.items, effect.chains, &inputs.hand);
		for (auto& chain : effect.chains)
		{
			chain.path = path;
			chain.effect = id;
		}
		if (!effect.items.empty())
		{
			result.push_back(std::move(effect));
		}
	}
	return result;
}
} // namespace

manager::SortedFrame manager::CollectSorted(const FrameInputs& inputs)
{
	// the fraction of the game turn the effects are drawn at
	const float t = inputs.turnFraction;
	SortedFrame frame;
	const auto add = [&frame](const Effect::DrawAtom& atom, uint32_t effect, float drawT) {
		const auto* creator = atom.creator;
		if (atom.atom != nullptr && (creator->kind == Creator::Kind::MeshPiece || creator->className == "ZR_SurfRevol"))
		{
			frame.atOnce.push_back(atom.atom);
		}
		if (creator->kind == Creator::Kind::Sprite)
		{
			// the sprite's position is the atom's, raised by the height x the size x 0.5 with CentreAtBase; the size is
			// the scale, at least 0.0001, the height the stretch
			glm::vec3 key = atom.position;
			if (creator->centreAtBase)
			{
				key.y += atom.stretch * std::max(atom.scale, 1e-4f) * 0.5f;
			}
			frame.sprites.push_back({key, atom, effect, drawT});
		}
		else if (creator->kind == Creator::Kind::Mesh)
		{
			frame.meshes.push_back({atom.position, atom, effect, drawT});
		}
		else if (dynamic_cast<const MistCreator*>(creator) != nullptr)
		{
			frame.mists.push_back({atom.position, atom, effect, drawT});
		}
		else if (creator->className == "ZR_SurfRevol")
		{
			frame.surfaces.push_back({atom.position, atom, effect, drawT});
		}
		else
		{
			frame.others.push_back({atom.position, atom, effect, drawT});
		}
	};
	for (const auto& [id, running] : ManagerState().effects)
	{
		if (running.path != DrawPath::Sorted)
		{
			continue;
		}
		const float drawT = running.perFrame ? 1.0f : t;
		std::vector<Effect::OrderedItem> items;
		std::vector<Effect::DrawChain> chains;
		running.effect->CollectOrdered(drawT, items, chains, &inputs.hand);
		for (const auto& item : items)
		{
			if (item.chain >= 0)
			{
				// the joint n / 2 (the item's atom, Effect::CollectOrdered)
				auto& chain = chains[static_cast<size_t>(item.chain)];
				chain.path = DrawPath::Sorted;
				chain.effect = id;
				frame.chains.push_back({item.atom.position, std::move(chain), id, drawT});
				continue;
			}
			add(item.atom, id, drawT);
		}
	}
	for (const auto& drawable : CollectSources(inputs))
	{
		if (drawable.path != DrawPath::Sorted)
		{
			continue;
		}
		for (const auto& atom : drawable.atoms)
		{
			add(atom, drawable.effect, drawable.t);
		}
	}
	return frame;
}

std::vector<manager::OrderedEffect> manager::CollectQueued(const FrameInputs& inputs)
{
	auto result = CollectOrderedOf(DrawPath::Queued, inputs);
	for (auto& drawable : CollectSources(inputs))
	{
		if (drawable.path != DrawPath::Queued || drawable.atoms.empty())
		{
			continue;
		}
		OrderedEffect effect {drawable.effect, DrawPath::Queued, drawable.origin, drawable.t, {}, {}};
		for (auto& atom : drawable.atoms)
		{
			effect.items.push_back({atom, -1});
		}
		result.push_back(std::move(effect));
	}
	return result;
}

std::vector<manager::OrderedEffect> manager::HandEffects(const FrameInputs& inputs)
{
	return CollectOrderedOf(DrawPath::Immediate, inputs);
}
