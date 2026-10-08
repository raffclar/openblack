/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureCaveEffects.h"

#include <algorithm>

#include <entt/core/hashed_string.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "3D/FrameAnim.h"
#include "3D/TempleInteriorInterface.h"
#include "Common/GameRandom.h"
#include "ECS/Components/MistDome.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace frame_anim = openblack::graphics::frame_anim;

namespace
{
// The smoke texture's and the fire's frames are 8 by 8 cells of their textures
constexpr float k_Cell = 1.0f / 8.0f;

// The textures, loaded with every raw texture of the game
constexpr entt::id_type k_Smoke = entt::hashed_string("raw/smoke").value();
constexpr entt::id_type k_SmokeAlpha = entt::hashed_string("raw/smokea").value();
constexpr entt::id_type k_Fire = entt::hashed_string("raw/S_Fire").value();
constexpr entt::id_type k_FireAlpha = entt::hashed_string("raw/S_Firea").value();

// The creature's room's flames: four fire sprites over the fire, orange and added by alpha
constexpr size_t k_Flames = 4;
constexpr glm::vec4 k_FlameColour {1.0f, 128.0f / 255.0f, 64.0f / 255.0f, 128.0f / 255.0f};

// Its smoke over the fire, grey, two units above it
constexpr size_t k_FireSmokes = 2;
constexpr glm::vec3 k_FireSmokeColour {64.0f / 255.0f};
constexpr float k_FireSmokeSize = 2.0f;

// The spray at the waterfall's foot, sixteen smokes thrown up from places the game picks again every frame
constexpr size_t k_Sprays = 16;
constexpr float k_SpraySize = 4.0f;

// The mist there, four domes of mist.l3d
constexpr size_t k_Mists = 4;

// The smokes' rates: the particles' spin a second, their drift, the pull of the wind on them, how they fall when
// thrown up, and the steps of age a second
constexpr float k_Spin = 0.765f;
constexpr float k_DriftSpeed = 2.55f;
constexpr float k_Pull = 1.5f;
constexpr float k_RisingFall = 2.5f;
constexpr float k_AgePerSecond = 255.0f;
/// The longest step it takes at once, in seconds
constexpr float k_LongestStep = 100.0f;
/// How big the particles grow, from half their smoke's size, by age
constexpr float k_GrowthPerAge = 0.0022222223f;
constexpr float k_SmallestSize = 1e-4f;

/// The cell of a frame of an 8 by 8 texture
glm::vec2 Cell(uint32_t frame)
{
	return {static_cast<float>(frame % 8) * k_Cell, static_cast<float>(frame / 8) * k_Cell};
}

graphics::TextureHandle Texture(entt::id_type id)
{
	return Locator::resources::value().GetTextures().Handle(id)->GetNativeHandle();
}

entt::entity CreateSprite(entt::id_type texture, entt::id_type alpha, bool additive, glm::vec3 position)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<TempleInteriorPart>(entity, TempleRoom::CreatureCave);
	registry.Assign<Sprite>(entity, Texture(texture), glm::vec2(0.0f), glm::vec2(k_Cell), glm::vec4(1.0f), additive, true,
	                        Texture(alpha));
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	return entity;
}
} // namespace

bool CreatureCaveEffects::CanBeMade()
{
	if (!Locator::resources::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return false;
	}
	const auto& textures = Locator::resources::value().GetTextures();
	return std::ranges::all_of(std::array {k_Smoke, k_SmokeAlpha, k_Fire, k_FireAlpha},
	                           [&textures](entt::id_type id) { return textures.Contains(id); });
}

float CreatureCaveEffects::Draw(float min, float max)
{
	return std::uniform_real_distribution<float>(min, max)(_engine);
}

CreatureCaveEffects::Smoke CreatureCaveEffects::CreateSmoke()
{
	// A new smoke: its particles start on their way up from the world's origin, unseen until they first start over
	Smoke smoke;
	for (size_t i = 0; i < smoke.particles.size(); ++i)
	{
		auto& particle = smoke.particles.at(i);
		particle.position = {0.0f, static_cast<float>(i) * 0.5f, 0.0f};
		particle.age = static_cast<int32_t>(i) * 90;
		particle.angle = Draw(0.0f, glm::pi<float>());
		particle.spinsBack = (static_cast<int32_t>(Draw(1.0f, 100.0f)) & 1) != 0;
		particle.hidden = true;
		particle.sprite = CreateSprite(k_Smoke, k_SmokeAlpha, false, particle.position);
	}
	return smoke;
}

CreatureCaveEffects::CreatureCaveEffects(glm::vec3 fire)
    : _random([this](float min, float max) { return Draw(min, max); })
{
	// As the creature's room sets up its effects
	auto& registry = Locator::entitiesRegistry::value();
	for (size_t i = 0; i < k_Flames; ++i)
	{
		const glm::vec3 place = fire + glm::vec3(Draw(-1.0f, 1.0f), Draw(1.0f, 2.0f), Draw(-1.0f, 1.0f));
		const auto flame = CreateSprite(k_Fire, k_FireAlpha, true, place);
		const float size = std::max((static_cast<float>(i) * 0.5f) + Draw(0.8f, 1.2f) + 2.0f, k_SmallestSize);
		registry.Get<Transform>(flame).scale = glm::vec3(size, size, 1.0f);
		registry.Get<Sprite>(flame).tint = k_FlameColour;
		_flames.push_back(flame);
	}

	for (size_t i = 0; i < k_Sprays; ++i)
	{
		auto spray = CreateSmoke();
		// From blue to white across the sixteen, as their colours' bytes wrap
		const auto step = static_cast<int32_t>(i * 0xff) / 16;
		const auto colour = ((static_cast<uint32_t>((step << 12) >> 8) - 0x1100U) & 0xff00U) |
		                    ((static_cast<uint32_t>((step * 0x2f0000) >> 8) - 0x300000U) & 0xff0000U) | 0xffU;
		spray.colour = glm::vec3((colour >> 16) & 0xff, (colour >> 8) & 0xff, colour & 0xff) / 255.0f;
		spray.drift = {Draw(0.0f, 1.0f) - 0.5f, Draw(0.0f, 1.0f), Draw(0.0f, 1.0f) - 0.5f};
		spray.rising = true;
		spray.size = k_SpraySize;
		_spray.push_back(spray);
	}

	for (size_t i = 0; i < k_FireSmokes; ++i)
	{
		auto smoke = CreateSmoke();
		smoke.place = fire + glm::vec3(Draw(-1.0f, 1.0f), Draw(-1.0f, 1.0f) + 2.0f, Draw(-1.0f, 1.0f));
		smoke.colour = k_FireSmokeColour;
		smoke.size = k_FireSmokeSize;
		_fireSmoke.push_back(smoke);
	}

	for (size_t i = 0; i < k_Mists; ++i)
	{
		const glm::vec3 place {Draw(0.0f, 1.0f) + 160.0f - 0.5f, -45.0f - Draw(0.0f, 8.0f), Draw(0.0f, 1.0f) - 30.0f - 0.5f};
		const auto mist = registry.Create();
		registry.Assign<TempleInteriorPart>(mist, TempleRoom::CreatureCave);
		registry.Assign<Transform>(mist, place, glm::mat3(1.0f), glm::vec3(1.0f));
		// The game's mist starts at a random frame of its texture, drawn from the C library's stream as every mist's
		MistDome dome;
		dome.clock.counter = frame_anim::MistStartCounter(game_random::crt::Random(0.0f, 16.0f));
		registry.Assign<MistDome>(mist, dome);
		_mists.push_back(mist);
	}
}

CreatureCaveEffects::~CreatureCaveEffects()
{
	// The registry may have gone before the temple, as the game closes
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto destroy = [&registry](entt::entity entity) {
		if (registry.Valid(entity))
		{
			registry.Destroy(entity);
		}
	};
	std::ranges::for_each(_flames, destroy);
	for (const auto* smokes : {&_spray, &_fireSmoke})
	{
		for (const auto& smoke : *smokes)
		{
			for (const auto& particle : smoke.particles)
			{
				destroy(particle.sprite);
			}
		}
	}
	std::ranges::for_each(_mists, destroy);
}

uint32_t CreatureCaveEffects::FlameFrame(uint32_t tickCount, uint32_t flame)
{
	// The flames' 32 frames, every 32 milliseconds, each a quarter of the way on from the one before it
	return 0x1fU - (((tickCount >> 5U) + flame * 8U) & 0x1fU);
}

uint8_t CreatureCaveEffects::SmokeAlpha(int32_t age, bool rising)
{
	constexpr int32_t k_Alpha = 0x4f;
	constexpr int32_t k_Fading = 0xe2;
	if (age < k_Fading)
	{
		// Smoke thrown up fades in over its first hundred steps
		return static_cast<uint8_t>(rising && age <= 99 ? age * k_Alpha / 100 : k_Alpha);
	}
	return static_cast<uint8_t>(((0xe1 - age) * k_Alpha / 0x2a3) + k_Alpha);
}

void CreatureCaveEffects::StepSmoke(Smoke& smoke, uint32_t milliseconds, const Random& random)
{
	// The cave's smokes are out of the wind: only smoke thrown up pulls down.
	glm::vec3 pull {0.0f};
	if (smoke.rising)
	{
		pull.y -= k_RisingFall;
	}
	const float seconds = std::min(static_cast<float>(milliseconds) * 0.001f, k_LongestStep);
	const float spin = seconds * k_Spin;
	// The steps of age, the fraction kept for the next frame as the island's smokes keep it
	const auto ageing = frame_anim::SmokeAgeStep(smoke.ageRemainder, static_cast<float>(milliseconds));

	const auto move = [&smoke, pull](Smoke::Particle& particle, float time) {
		const auto velocity = particle.velocity + (pull * time * k_Pull);
		particle.position += (particle.velocity + velocity) * time * 0.5f;
		particle.velocity = velocity;
		particle.position += smoke.drift * time * k_DriftSpeed;
	};
	for (auto& particle : smoke.particles)
	{
		particle.age += ageing;
		particle.angle += particle.spinsBack ? spin : -spin;
		if (particle.age <= k_SmokeLife)
		{
			move(particle, seconds);
			continue;
		}
		// It starts over from the smoke's place, as far on as it has gone past its end
		particle.position = smoke.place;
		particle.age %= k_SmokeLife;
		particle.hidden = false;
		particle.velocity =
		    smoke.rising ? glm::vec3(random(-4.0f, 4.0f), random(3.0f, 4.0f), random(-4.0f, 4.0f)) : glm::vec3(0.0f);
		move(particle, static_cast<float>(particle.age) / k_AgePerSecond);
	}
}

void CreatureCaveEffects::ShowSmoke(const Smoke& smoke) const
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto& particle : smoke.particles)
	{
		auto& sprite = registry.Get<Sprite>(particle.sprite);
		auto& transform = registry.Get<Transform>(particle.sprite);
		// Its frame of the first 16 of the smoke texture, its size and its fade by its age
		sprite.uvMin = Cell(static_cast<uint32_t>(frame_anim::MistCell(particle.age)));
		const float size = std::max(((static_cast<float>(particle.age) * k_GrowthPerAge) + 0.5f) * smoke.size, k_SmallestSize);
		const float alpha = particle.hidden ? 0.0f : static_cast<float>(SmokeAlpha(particle.age, smoke.rising)) / 255.0f;
		sprite.tint = glm::vec4(smoke.colour, alpha);
		transform.position = particle.position;
		transform.rotation = glm::mat3(glm::rotate(particle.angle, glm::vec3(0.0f, 0.0f, 1.0f)));
		transform.scale = glm::vec3(size, size, 1.0f);
	}
}

void CreatureCaveEffects::Update(uint32_t milliseconds, uint32_t tickCount)
{
	auto& registry = Locator::entitiesRegistry::value();

	// Each frame: the spray from new places about the waterfall's foot, and the fire's smoke
	for (auto& spray : _spray)
	{
		spray.place = {Draw(0.0f, 5.0f) + 160.0f - 2.5f, -30.0f - Draw(0.0f, 23.0f), Draw(0.0f, 1.0f) - 16.0f - 0.5f};
		StepSmoke(spray, milliseconds, _random);
		ShowSmoke(spray);
	}
	for (auto& smoke : _fireSmoke)
	{
		StepSmoke(smoke, milliseconds, _random);
		ShowSmoke(smoke);
	}

	// Each mist dome steps on through 16 frames of the smoke texture
	for (const auto mist : _mists)
	{
		frame_anim::MistAdvance(registry.Get<MistDome>(mist).clock, static_cast<float>(milliseconds));
	}

	// The flames' frames
	for (uint32_t i = 0; i < _flames.size(); ++i)
	{
		registry.Get<Sprite>(_flames[i]).uvMin = Cell(FlameFrame(tickCount, i));
	}
}
