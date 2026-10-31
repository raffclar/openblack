/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "LiquidParticleSystem.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>

#include <entt/core/hashed_string.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/PlayersSymbols.h"
#include "Common/GameRandom.h"
#include "ECS/GamePlayers.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"
#include "Particles/ParticleDrawFrame.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using liquid_particles::Kind;
using liquid_particles::Particle;

namespace
{
/// The particles are drawn from this sheet, eight frames a row
constexpr entt::hashed_string k_Sheet = entt::hashed_string("raw/blobs");
constexpr entt::hashed_string k_SheetAlpha = entt::hashed_string("raw/blobsa");

/// The dust and the smoke: the blobs sheet, blended over what is behind, writing no depth
particles::Creator MakePuffCreator()
{
	particles::Creator puff;
	puff.kind = particles::Creator::Kind::Sprite;
	puff.className = "LiquidPuff";
	puff.texture = "blobs";
	puff.spritesPerRow = 8;
	puff.numFrames = 64;
	puff.additive = false;
	return puff;
}

/// The motes: the same sheet, added to what is behind them
particles::Creator MakeMoteCreator()
{
	auto mote = MakePuffCreator();
	mote.className = "LiquidMote";
	mote.additive = true;
	return mote;
}

/// The drops of liquid: the same sheet, kept where the alpha passes the test, writing depth
particles::Creator MakeLiquidCreator()
{
	auto liquid = MakePuffCreator();
	liquid.className = "LiquidDrop";
	liquid.alphaTest = true;
	return liquid;
}

LiquidParticleSystem::Sources GameSources()
{
	return {
	    .random = []() { return Locator::gameRandom::has_value() ? Locator::gameRandom::value().CrtRand() : 0; },
	    .altitude =
	        [](int32_t x, int32_t z) {
		        return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetAltitude(x, z) : 0.0f;
	        },
	    .drawDue =
	        [](frame_pace::Pace& pace) {
		        return Locator::time::has_value() && frame_pace::Due(pace, Locator::time::value().GetFrameRealTime());
	        },
	};
}
} // namespace

LiquidParticleSystem::LiquidParticleSystem()
    : LiquidParticleSystem(GameSources())
{
}

LiquidParticleSystem::LiquidParticleSystem(Sources sources)
    : _sources(std::move(sources))
    , _liquidCreator(MakeLiquidCreator())
    , _puffCreator(MakePuffCreator())
    , _moteCreator(MakeMoteCreator())
{
	_pool.reserve(liquid_particles::k_MostParticles);
}

void LiquidParticleSystem::Add(glm::vec3 position, glm::vec3 velocity, float size, uint32_t argb, Kind kind)
{
	AddTo(_pool, position, velocity, size, argb, kind);
}

void LiquidParticleSystem::AddForPlayer(PlayerNames player, glm::vec3 position, glm::vec3 velocity, float size, uint32_t argb,
                                        Kind kind)
{
	AddTo(_playerPools[player].particles, position, velocity, size, argb, kind);
}

void LiquidParticleSystem::SetPlayerPoint(PlayerNames player, glm::vec3 point)
{
	_playerPools[player].point = point;
}

std::span<const Particle> LiquidParticleSystem::PlayerParticles(PlayerNames player) const
{
	const auto found = _playerPools.find(player);
	return found != _playerPools.end() ? std::span<const Particle>(found->second.particles) : std::span<const Particle> {};
}

void LiquidParticleSystem::AddTo(std::vector<Particle>& pool, glm::vec3 position, glm::vec3 velocity, float size, uint32_t argb,
                                 Kind kind)
{
	// Only a particle there is room for is made, and only then does it draw its frame
	if (pool.size() >= liquid_particles::k_MostParticles)
	{
		return;
	}
	pool.push_back({
	    .position = position,
	    .velocity = velocity,
	    .size = size,
	    .variant = liquid_particles::StartVariant(kind, _sources.random),
	    .argb = argb,
	    .kind = kind,
	});
}

void LiquidParticleSystem::UpdateFrame(float seconds, bool drawsDrops)
{
	// The shared pool moves on first, then the pools of the other players in the order of their players
	liquid_particles::StepPool(_pool, seconds, _sources.altitude, _sources.random);
	for (auto& [player, pool] : _playerPools)
	{
		liquid_particles::StepPool(pool.particles, seconds, _sources.altitude, _sources.random);
	}
	// The game draws the pools after it moves them on. Each draw of a drop of liquid takes one random number for its
	// frame.
	const bool empty =
	    _pool.empty() && std::ranges::all_of(_playerPools, [](const auto& entry) { return entry.second.particles.empty(); });
	if (!drawsDrops || empty || !_sources.drawDue(_drawPace))
	{
		return;
	}
	const auto redraw = [this](std::vector<Particle>& pool) {
		for (auto& particle : pool)
		{
			if (particle.kind == Kind::Liquid)
			{
				particle.variant = liquid_particles::LiquidFrame(_sources.random());
			}
		}
	};
	redraw(_pool);
	for (auto& [player, pool] : _playerPools)
	{
		redraw(pool.particles);
	}
}

void LiquidParticleSystem::CollectDrawFrame(particles::draw::Frame& frame) const
{
	if (!_pool.empty())
	{
		CollectPool(frame, _pool, _pool.front().position);
	}
	for (const auto& [player, pool] : _playerPools)
	{
		CollectPool(frame, pool.particles, pool.point);
	}
}

void LiquidParticleSystem::CollectPool(particles::draw::Frame& frame, const std::vector<Particle>& pool, glm::vec3 point) const
{
	if (pool.empty())
	{
		return;
	}
	const particles::draw::Sources sources {
	    .textures = [](std::string_view /*texture*/) -> std::optional<std::pair<entt::id_type, entt::id_type>> {
		    return std::pair {k_Sheet.value(), k_SheetAlpha.value()};
	    },
	    .playerColour = {},
	    .random = {},
	    .symbolsInARow = static_cast<int>(players_symbols::SquaresInARowFor(ecs::game_players::Count())),
	};
	particles::Effect::DrawWalk walk;
	walk.steps.reserve(pool.size());
	walk.atoms.reserve(pool.size());
	for (const auto& particle : pool)
	{
		const auto* creator = particle.kind == Kind::Liquid                 ? &_liquidCreator
		                      : liquid_particles::IsAdditive(particle.kind) ? &_moteCreator
		                                                                    : &_puffCreator;
		walk.steps.push_back({.chain = false, .index = static_cast<uint32_t>(walk.atoms.size())});
		walk.atoms.push_back({
		    .creator = creator,
		    .position = particle.position,
		    .rotation = glm::mat3(1.0f),
		    .scale = liquid_particles::DrawnSize(particle),
		    .stretch = 1.0f,
		    .alpha = static_cast<float>(particle.argb >> 24u),
		    .frame = static_cast<float>(liquid_particles::Frame(particle)),
		    .rgb = {static_cast<uint8_t>((particle.argb >> 16u) & 0xFFu), static_cast<uint8_t>((particle.argb >> 8u) & 0xFFu),
		            static_cast<uint8_t>(particle.argb & 0xFFu)},
		});
	}
	particles::draw::AddEffect(frame, walk, particles::draw::DrawPath::Sorted, point, particles::draw::k_NeutralPlayer,
	                           sources);
}

void LiquidParticleSystem::Clear()
{
	_pool.clear();
	_playerPools.clear();
}

void LiquidParticleSystem::SaveState(reflection::BinaryWriter& out) const
{
	// The drops, the dust and the motes in the order of the pool, and the pace of the frames that draw them. These are
	// presentation values: the frames move and draw the drops, and no turn reads them.
	out.BeginPresentation();
	out.WriteSize(_pool.size());
	for (const auto& particle : _pool)
	{
		reflection::Save(out, particle.position);
		reflection::Save(out, particle.velocity);
		reflection::Save(out, particle.size);
		reflection::Save(out, particle.age);
		reflection::Save(out, particle.variant);
		reflection::Save(out, particle.argb);
		reflection::Save(out, particle.kind);
		reflection::Save(out, particle.onLand);
	}
	reflection::Save(out, _drawPace);
	out.EndPresentation();
}

bool LiquidParticleSystem::LoadState(reflection::BinaryReader& in)
{
	const auto count = in.ReadSize();
	if (!count.has_value() || *count > liquid_particles::k_MostParticles)
	{
		return false;
	}
	_pool.assign(*count, {});
	for (auto& particle : _pool)
	{
		reflection::Load(in, particle.position);
		reflection::Load(in, particle.velocity);
		reflection::Load(in, particle.size);
		reflection::Load(in, particle.age);
		reflection::Load(in, particle.variant);
		reflection::Load(in, particle.argb);
		reflection::Load(in, particle.kind);
		reflection::Load(in, particle.onLand);
	}
	reflection::Load(in, _drawPace);
	return !in.Failed() && _pool.size() <= liquid_particles::k_MostParticles;
}
