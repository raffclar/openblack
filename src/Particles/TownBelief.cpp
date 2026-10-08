/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownBelief.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <chrono>
#include <numbers>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownBeliefSymbols.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownQueries.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Particles/PSysManagerState.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
// SF_TownBelief's UR_TownCentreBelief0
constexpr float k_HeightAt1 = 2.0f;
constexpr float k_HeightPerLevel = 2.5f;
constexpr float k_RadiusAt0 = 0.3f, k_RadiusAt1 = 0.6f;
constexpr float k_ScaleAt0 = 0.5f, k_ScaleAt1 = 1.0f;
constexpr float k_SpeedAt0 = 1.0f, k_SpeedAt1 = 2.0f;
constexpr float k_PhaseSpeed = 0.2f;
constexpr float k_SpeedUpDuringFight = 2.0f;
constexpr float k_FightAt0 = 1.5f, k_FightAt1 = 3.0f;
constexpr float k_BetweenFightsAt0 = 10.0f, k_BetweenFightsAt1 = 3.0f;
/// fmod's 2 pi: the float 2 pi widened to double, as the rule's wraps use it
constexpr double k_FmodTwoPi = 6.2831854820251465;

/// the player colours (by remapped player; the remap comes from the profile, identity here)
constexpr std::array<uint32_t, 7> k_PlayerColours = {0xFF4646, 0x47FF54, 0xE347FF, 0x47F9FF, 0xFFFD47, 0x4777FF, 0xFFA247};

using Symbol = ecs::components::TownBeliefSymbol;
using Centre = ecs::components::TownBeliefSymbols;

/// The rule's random float in a..b: the effect of a town centre is local, so the local stream (Step opens its step
/// scope)
float Rand(float a, float b)
{
	return game_random::psys::FloatRand(a, b);
}

/// The players' symbols: the local human's cell is a copy of the
/// ChooseSymbol cell of the profile's "player symbol" (registry, 0 when missing, as on this install), so it is drawn
/// straight from ChooseSymbol (4 x 4 cells of 64 x 64). Computer players get Lethis/Kazarr/Nemesis .cps greyscale
/// images there (not done: they use ChooseSymbol cell = player number).
const Creator& SymbolCreator(int player)
{
	static const std::array<Creator, 16> creators = [] {
		std::array<Creator, 16> c;
		for (int i = 0; i < 16; ++i)
		{
			c[static_cast<size_t>(i)].kind = Creator::Kind::Sprite;
			c[static_cast<size_t>(i)].texture = "ChooseSymbol";
			c[static_cast<size_t>(i)].spritesPerRow = 4;
			c[static_cast<size_t>(i)].fileOffset = i;
			c[static_cast<size_t>(i)].numFrames = 1;
		}
		return c;
	}();
	constexpr int k_ProfileSymbol = 0;
	return creators[static_cast<size_t>(player == 0 ? k_ProfileSymbol : std::clamp(player, 0, 15))];
}

const Creator& GlowCreator()
{
	static const Creator creator = [] {
		Creator c;
		c.kind = Creator::Kind::Sprite;
		c.texture = "S_SpriteSheet3";
		c.spritesPerRow = 8;
		c.numFrames = 32;
		return c;
	}();
	return creator;
}

bool IsTownCentre(const ecs::components::Abode& abode)
{
	// the town centre types, from the info constants on first use (kept in the psys state)
	auto& centres = Locator::particleSystem::value().GetState().townCentreTypes;
	if (!centres.has_value())
	{
		centres.emplace();
		for (const auto& info : Locator::infoConstants::value().abode)
		{
			if (info.abodeType == AbodeType::TownCentre)
			{
				centres->insert(info.abodeNumber);
			}
		}
	}
	return centres->contains(abode.type);
}

/// A belief symbol shown over a centre: the player, its belief clamped to 0..1 and its rank
struct Shown
{
	int player;
	float belief;
	int rank;
};

/// The players with some belief in the town and their rank: the players with more belief, or as much and a higher
/// number. The belief is the town's (Town::belief.belief, by player number). The candidates are the active players'
/// slots 0..6 (the neutral one has no symbol); the rank counts all 8 slots, the neutral one included
std::vector<Shown> ShownSymbols(const std::array<float, 8>& beliefs)
{
	constexpr int k_SymbolPlayers = 7;
	constexpr int k_RankSlots = 8;
	std::vector<Shown> result;
	for (int player = 0; player < k_SymbolPlayers; ++player)
	{
		const float b = std::clamp(beliefs.at(static_cast<size_t>(player)), 0.0f, 1.0f);
		if (b <= 0.0f)
		{
			continue;
		}
		int rank = 0;
		for (int other = 0; other < k_RankSlots; ++other)
		{
			const float ob = std::clamp(beliefs.at(static_cast<size_t>(other)), 0.0f, 1.0f);
			if (other != player && (ob > b || (ob == b && other > player)))
			{
				++rank;
			}
		}
		result.push_back({player, b, rank});
	}
	return result;
}

float Radius(const Shown& shown)
{
	return shown.rank == 0 ? 0.0f : k_RadiusAt0 + (k_RadiusAt1 - k_RadiusAt0) * shown.belief;
}

/// The town centres (an Abode whose number is a TownCentre's, with a Mesh) and their town's beliefs, in the order the
/// original draws them: its list of centres gets each new one at the head, so the newest first (here by the object
/// creation index); unavailable ones (components::Unavailable) left out
template <typename Fn>
void ForEachCentre(Fn&& fn)
{
	using namespace ecs::components;
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<std::pair<int64_t, entt::entity>> centres;
	registry.Each<const Abode, const Transform, const Mesh>(
	    [&centres](entt::entity entity, const Abode& abode, const Transform&, const Mesh&) {
		    // a centre leaves the list when it is deleted
		    if (IsTownCentre(abode) && ecs::IsAvailable(entity))
		    {
			    centres.emplace_back(ecs::object_index::Of(entity), entity);
		    }
	    },
	    entt::exclude<Unavailable>);
	std::stable_sort(centres.begin(), centres.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
	for (const auto& [index, entity] : centres)
	{
		const auto& abode = registry.Get<const Abode>(entity);
		const auto town = ecs::town_queries::TownByKey(abode.townId);
		if (town == entt::null || !registry.Valid(town))
		{
			continue;
		}
		fn(entity, registry.Get<const Town>(town).belief.belief);
	}
}

/// One step of the centre's effect through UR_TownCentreBelief, with the step's dt
void StepCentre(Centre& centre, const std::vector<Shown>& shown, float dt)
{
	for (const auto& symbolOf : shown)
	{
		const auto [slot, created] = centre.symbols.try_emplace(symbolOf.player);
		auto& symbol = slot->second;
		if (created)
		{
			// the new symbol's two angles, a1 then a2
			symbol.a1 = game_random::psys::FloatRand(glm::two_pi<float>());
			symbol.a2 = game_random::psys::FloatRand(glm::two_pi<float>());
		}
		const float b = symbolOf.belief;
		const float radius = Radius(symbolOf);
		float speed = symbolOf.rank == 0 ? 0.0f : (k_SpeedAt0 + (k_SpeedAt1 - k_SpeedAt0) * b) / std::max(radius, 1e-3f);
		float fight = 0.0f;
		if (symbolOf.rank == 1)
		{
			// the second symbol only. While its wait is > 0 it counts down by dt
			if (symbol.waitLength > 0.0f)
			{
				symbol.waitLength -= dt;
			}
			else
			{
				// the fight's timer += dt; past the fight's length, the next wait and fight are drawn together, wait
				// first, and the timer is back to 0
				symbol.fightTimer += dt;
				if (symbol.fightTimer > symbol.fightLength)
				{
					const float wait = k_BetweenFightsAt0 + (k_BetweenFightsAt1 - k_BetweenFightsAt0) * b;
					symbol.waitLength = Rand(0.5f, 1.5f) * wait;
					const float length = k_FightAt0 + (k_FightAt1 - k_FightAt0) * b;
					symbol.fightTimer = 0.0f;
					symbol.fightLength = Rand(0.5f, 1.5f) * length;
				}
				// f = timer / length, 0 for <= 0 or NaN, at most 1; fight = 1 - (2f - 1)^2
				float f = symbol.fightTimer / symbol.fightLength;
				if (!(f > 0.0f))
				{
					f = 0.0f;
				}
				else if (!(f < 1.0f))
				{
					f = 1.0f;
				}
				const float g = 2.0f * f - 1.0f;
				fight = 1.0f - g * g;
			}
			// speed x ((SpeedUpDuringFight - 1) x fight + 1)
			speed *= (k_SpeedUpDuringFight - 1.0f) * fight + 1.0f;
		}
		symbol.fight = fight;
		// phase += dt x PhaseSpeed, a1 += ((cos(phase) + 1) x 0.25 + 0.5) x speed x dt x 0.846, a2 += speed x dt, each
		// one fmod 2 pi. (approximate) the original keeps each sum in extended precision into the fmod and takes the cos
		// of the unrounded fmod result; here float sums and cos of the stored float
		const auto wrap = [](float x) { return static_cast<float>(std::fmod(static_cast<double>(x), k_FmodTwoPi)); };
		symbol.phase = wrap(dt * k_PhaseSpeed + symbol.phase);
		symbol.a1 = wrap(((std::cos(symbol.phase) + 1.0f) * 0.25f + 0.5f) * speed * dt * 0.846f + symbol.a1);
		symbol.a2 = wrap(speed * dt + symbol.a2);
	}
}
} // namespace

void town_belief::Clear()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> centres;
	registry.Each<const Centre>([&centres](entt::entity entity, const Centre&) { centres.push_back(entity); });
	for (const auto entity : centres)
	{
		registry.RemoveState<Centre>(entity);
	}
}

void town_belief::RemoveCentre(entt::entity townCentre)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(townCentre))
	{
		registry.RemoveState<Centre>(townCentre);
	}
}

void town_belief::Step()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	// the centre's effect steps with the ms of a turn, not the frame's: dt = ms x 0.001
	const float dt = static_cast<float>(game_clock::MsPerTurn()) * 0.001f;
	// the symbol draw's game time step: the frame's whole game ms, 0 paused
	const auto milliseconds = static_cast<float>(game_clock::FrameGameMs());
	// the rule's random draws run in the town centre effect's step, a local one
	const game_random::psys::StepScope step(game_random::psys::NetGameType::Local);
	ForEachCentre([&](entt::entity entity, const std::array<float, 8>& beliefs) {
		const auto shown = ShownSymbols(beliefs);
		auto& registry = Locator::entitiesRegistry::value();
		auto* existing = registry.TryGet<Centre>(entity);
		const bool created = existing == nullptr;
		auto& centre = created ? registry.AssignState<Centre>(entity) : *existing;
		// a new effect: it is processed at once when created and an effect's first process steps twice; then this
		// frame's own step. (approximate) the original makes it when the centre becomes functional or is loaded,
		// openblack on the first frame that sees the centre
		const int steps = created ? 3 : 1;
		for (int i = 0; i < steps; ++i)
		{
			StepCentre(centre, shown, dt);
		}
		// each symbol's draw, once a frame
		for (const auto& symbolOf : shown)
		{
			auto& symbol = centre.symbols[symbolOf.player];
			// the second glow's sprite angle (frame_anim::PlayerSymbolSpin) and the cells of the two glows
			// (frame_anim::PlayerSymbolCell)
			symbol.spin = graphics::frame_anim::PlayerSymbolSpin(symbol.glowSpin, milliseconds);
			symbol.cellA = static_cast<float>(graphics::frame_anim::PlayerSymbolCell(symbol.glowA, milliseconds, 0));
			symbol.cellB = static_cast<float>(graphics::frame_anim::PlayerSymbolCell(symbol.glowB, milliseconds, 1));
		}
	});
}

void town_belief::Collect(const glm::vec3& camera, std::vector<manager::Drawable>& out)
{
	using namespace ecs::components;
	auto& registry = Locator::entitiesRegistry::value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	ForEachCentre([&](entt::entity entity, const std::array<float, 8>& beliefs) {
		const auto* found = registry.TryGet<const Centre>(entity);
		if (found == nullptr)
		{
			return; // not stepped yet
		}
		const auto& centre = *found;
		const auto& transform = registry.Get<const Transform>(entity);
		const auto& mesh = registry.Get<const Mesh>(entity);
		// the totem (TotemStatue) + the height of the icon on the plinth + HeightAt1; without a totem, the top of the
		// town centre's mesh
		glm::vec3 base = transform.position;
		bool totem = false;
		registry.Each<const TotemStatue, const Transform>(
		    [&](const TotemStatue& statue, const Transform& plinth) {
			    if (totem || statue.townCentre != entity)
			    {
				    return;
			    }
			    float height = 0.0f;
			    if (ecs::IsAvailable(statue.top))
			    {
				    const auto* topMesh = registry.TryGet<const Mesh>(statue.top);
				    const auto* topTransform = registry.TryGet<const Transform>(statue.top);
				    if (topMesh != nullptr && topTransform != nullptr && meshes.Contains(topMesh->id))
				    {
					    height = meshes.Handle(topMesh->id)->GetBoundingBox().Size().y * topTransform->scale.y;
				    }
			    }
			    base = glm::vec3(plinth.position.x, statue.baseY + height + k_HeightAt1, plinth.position.z);
			    totem = true;
		    },
		    entt::exclude<Unavailable>);
		if (!totem && meshes.Contains(mesh.id))
		{
			base.y += meshes.Handle(mesh.id)->GetBoundingBox().maxima.y * transform.scale.y + k_HeightAt1;
		}
		const float s = std::clamp(glm::distance(camera, base) * 0.01f, 1.0f, 10.0f);

		// each symbol its own Z object
		manager::Drawable drawable {base, {}, 1.0f, manager::DrawPath::Sorted};
		for (const auto& symbolOf : ShownSymbols(beliefs))
		{
			const auto symbolIt = centre.symbols.find(symbolOf.player);
			if (symbolIt == centre.symbols.end())
			{
				continue; // not stepped yet
			}
			const auto& symbol = symbolIt->second;
			const float radius = Radius(symbolOf);
			const float scale = k_ScaleAt0 + (k_ScaleAt1 - k_ScaleAt0) * symbolOf.belief;
			const glm::vec3 position =
			    base + s * glm::vec3(radius * std::cos(symbol.a2) * std::cos(symbol.a1),
			                         radius * std::sin(symbol.a1) +
			                             (1.0f - symbol.fight) * static_cast<float>(symbolOf.rank) * k_HeightPerLevel,
			                         radius * std::sin(symbol.a2) * std::cos(symbol.a1));

			// two glows then the symbol, all additive billboards
			const float size = 1.5f * scale;
			const uint32_t rgb = k_PlayerColours[static_cast<size_t>(symbolOf.player) % k_PlayerColours.size()];
			const std::array<uint8_t, 3> colour = {static_cast<uint8_t>(rgb >> 16), static_cast<uint8_t>(rgb >> 8),
			                                       static_cast<uint8_t>(rgb)};
			const glm::mat3 still(1.0f);
			// the second glow's spin carried as the atom's Y angle matrix (affine::AngleY), whose roll
			// atan2(M[0][2], M[0][0]) = +spin (billboard::Screen turns it clockwise)
			const glm::mat3 spun = affine::AngleY(symbol.spin);
			drawable.atoms.push_back({&GlowCreator(), position, still, 1.5f * size, 1.0f, 99.0f, symbol.cellA, colour});
			drawable.atoms.push_back({&GlowCreator(), position, spun, 1.5f * size, 1.0f, 99.0f, symbol.cellB, {255, 255, 255}});
			drawable.atoms.push_back({&SymbolCreator(symbolOf.player), position, still, size, 1.0f, 255.0f, 0.0f, colour});
		}
		if (!drawable.atoms.empty())
		{
			out.push_back(std::move(drawable));
		}
	});
}
