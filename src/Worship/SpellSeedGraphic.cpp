/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellSeedGraphic.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <glm/mat3x3.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "Camera/Camera.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ObjectColour.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Registry.h"
#include "ECS/Systems/WorshipStateInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/MagicTables.h"
#include "Particles/PSysManager.h"
#include "Particles/ParticleTypes.h"
#include "Particles/Rules/SurfRevol.h"
#include "Particles/SpellLink.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Resources/SharedAssets.h"

using namespace openblack;
using namespace openblack::worship;
using namespace openblack::ecs::components;

namespace
{
/// The power-up band mesh, loaded here on first use (Game.cpp loads only O_Bibble_up of that folder)
constexpr auto k_BandMesh = resources::shared_assets::k_PowerUpBandMesh;

constexpr float k_TwoPi = 6.2831854820251465f; // the fmod divisor of the angles
constexpr float k_SpinRate = 2.0f;             // rad/s of the mesh's y angle
constexpr float k_BandSpinRate = 10.3f;        // rad/s of the band angle
constexpr float k_BandSpin2Rate = 1.0f;        // rad/s of the second band angle
constexpr float k_BandScale = 0.2f;            // times the band scale and the graphic's scale
/// The bands' alpha before the caller's (set once, never changed)
constexpr uint32_t k_BandAlpha = 0x3C;
/// Each band level is drawn twice with the same matrix and colour (the second draw of the last level may also record the
/// picking distance and box). Additive, so every band adds its light twice.
constexpr size_t k_DrawsPerBand = 2;

struct SpellSeedGraphicState
{
	float phase {0.0f};
};

/// This module's state (Locator::worshipState)
SpellSeedGraphicState& SeedGraphic()
{
	if (!Locator::worshipState::has_value())
	{
		std::fputs("worship::seed_graphic: no worship state in the locator (Locator::worshipState)\n", stderr);
		std::abort();
	}
	return Locator::worshipState::value().Get<SpellSeedGraphicState>();
}

bool LoadBandMesh()
{
	return resources::shared_assets::LoadPowerUpBand("Worship") != resources::shared_assets::LoadResult::Failed;
}

entt::entity NewBand(const glm::vec3& position)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto band = registry.Create();
	registry.Assign<Transform>(band, position, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Mesh>(band, k_BandMesh.value(), static_cast<int8_t>(0), static_cast<int8_t>(0));
	registry.Assign<Alpha>(band, 0.0f);
	registry.Assign<ObjectColour>(band);
	return band;
}

/// The band's colour is the owner's player colour, or the local player's when the owner is neutral, with alpha
/// (k_BandAlpha x the caller's alpha) >> 8; the specular is 20 in r, g and b. (inferred) openblack's local player is
/// PLAYER_ONE.
void SetBandColour(entt::entity band, PlayerNames owner, uint8_t alpha)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (band == entt::null || !registry.Valid(band))
	{
		return;
	}
	const auto player = owner == PlayerNames::NEUTRAL ? PlayerNames::PLAYER_ONE : owner;
	const uint32_t rgb = psys::surf_revol::PlayerColour(static_cast<int>(player)); // identity remap
	constexpr uint32_t k_BandSpecular = 0x141414u;                                 // 20 in each channel
	registry.AssignOrReplace<ObjectColour>(
	    band, ObjectColour {{static_cast<uint8_t>(rgb >> 16), static_cast<uint8_t>(rgb >> 8), static_cast<uint8_t>(rgb)},
	                        k_BandSpecular});
	const uint32_t a = (k_BandAlpha * alpha) >> 8; // the byte above bit 8
	registry.AssignOrReplace<Alpha>(band, static_cast<float>(a & 0xFF) / 255.0f);
}

/// One band object drawn pu + 1 times, each twice (k_DrawsPerBand; openblack: one entity per drawing, extraBands the
/// 2 (pu + 1) - 1 after the first)
void UpdateBands(entt::entity graphicEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& graphic = registry.Get<SpellSeedGraphic>(graphicEntity);
	if (graphic.powerUp != -1 && graphic.band == entt::null && LoadBandMesh())
	{
		graphic.band = NewBand(graphic.point);
	}
	// No band drawn while the power-up is -1 (the band object stays, see SetPowerUpType)
	const size_t extra =
	    graphic.band == entt::null || graphic.powerUp < 0 ? 0 : (static_cast<size_t>(graphic.powerUp) + 1) * k_DrawsPerBand - 1;
	while (graphic.extraBands.size() > extra)
	{
		registry.Destroy(graphic.extraBands.back());
		graphic.extraBands.pop_back();
	}
	while (graphic.extraBands.size() < extra)
	{
		graphic.extraBands.push_back(NewBand(graphic.point));
	}
	if (graphic.band != entt::null)
	{
		const bool shown = graphic.powerUp != -1;
		if (shown && !registry.AllOf<Mesh>(graphic.band))
		{
			registry.Assign<Mesh>(graphic.band, k_BandMesh.value(), static_cast<int8_t>(0), static_cast<int8_t>(0));
		}
		else if (!shown && registry.AllOf<Mesh>(graphic.band))
		{
			registry.Remove<Mesh>(graphic.band);
		}
	}
	registry.SetDirty();
}

/// FLYING_FLOCK (10) takes AnimalBat1 when the player's alignment (0 with no player) is below the flock's
/// alignmentSwitch, else AnimalSpellDove. FOOD (3) and the creature phials (12..27) get the environment map and
/// BEAM_EXPLOSION (29) its own material properties: neither is ported (openblack has no per-object envmap).
std::optional<MeshId> ReplacedMesh(SpellSeedType seed, PlayerNames player)
{
	if (seed != SpellSeedType::FlockFlying)
	{
		return std::nullopt;
	}
	const auto* flying = magic::GetMagicInfoAs<GMagicFlockFlyingInfo>(Locator::infoConstants::value(), MagicType::FlockFlying);
	if (flying == nullptr)
	{
		return std::nullopt;
	}
	const float alignment = player == PlayerNames::NEUTRAL ? 0.0f : ecs::effects::alignment::Get(player);
	return alignment < flying->alignmentSwitch ? MeshId::AnimalBat1 : MeshId::AnimalSpellDove;
}

/// The point, the mesh at + unknown0x150 x scale, the effect at + unknown0x154 x scale
void SetPositions(SpellSeedGraphic& graphic, const glm::vec3& point)
{
	const auto& info = magic::GetSpellSeedInfo(Locator::infoConstants::value(), graphic.seedType);
	graphic.point = point;
	graphic.meshPosition = point + glm::vec3(0.0f, info.unknown0x150 * graphic.scale, 0.0f);
	graphic.effectPosition = point + glm::vec3(0.0f, info.unknown0x154 * graphic.scale, 0.0f);
}

/// Steps the effect with a zeroed process info (power 1, enabled) by the elapsed time
void StepEffect(uint32_t id, float milliseconds)
{
	if (id == 0 || psys::manager::Find(id) == nullptr)
	{
		return;
	}
	psys::ProcessInfo info {
	    .power = 1.0f,
	    .enabled = true,
	};
	psys::manager::ProcessForSpell(id, info, milliseconds * 0.001f);
}

/// The band's matrix as rows (world = sum local_i row_i): identity, rows 1 and 2 swapped with the old row 1 negated,
/// then turned in (x, z) by base + the band angle, in (x, y) by 0.3, in (x, z) by k and in (x, y) by 0.2; base, k = 0,
/// -1 for the first band, 0.5, 1 for the others
glm::mat3 BandRotation(float bandAngle, size_t index)
{
	std::array<glm::vec3, 3> rows = {glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)};
	const auto turnXZ = [&rows](float angle) {
		const float c = std::cos(angle);
		const float s = std::sin(angle);
		for (auto& row : rows)
		{
			const float x = row.x;
			row.x = c * x - s * row.z;
			row.z = c * row.z + s * x;
		}
	};
	const auto turnXY = [&rows](float angle) {
		const float c = std::cos(angle);
		const float s = std::sin(angle);
		for (auto& row : rows)
		{
			const float x = row.x;
			row.x = c * x + s * row.y;
			row.y = c * row.y - s * x;
		}
	};
	const float base = index == 0 ? 0.0f : 0.5f;
	const float k = index == 0 ? -1.0f : 1.0f;
	turnXZ(base + bandAngle);
	turnXY(0.3f);
	turnXZ(k);
	turnXY(0.2f);
	return {rows[0], rows[1], rows[2]};
}
} // namespace

entt::entity seed_graphic::Create(const glm::vec3& worldPosition, SpellSeedType seed, PlayerNames player, float scale,
                                  int powerUp)
{
	const auto index = static_cast<int>(seed);
	if (index < 0 || index >= static_cast<int>(magic::k_SpellSeedCount))
	{
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& info = magic::GetSpellSeedInfo(Locator::infoConstants::value(), seed);
	// Not a game object (no creation index); the angles start at 0, both scales at 1
	const auto entity = registry.Create();
	auto& graphic = registry.Assign<SpellSeedGraphic>(entity);
	graphic.seedType = seed;
	graphic.player = player;
	graphic.scale = scale;
	graphic.powerUp = powerUp;
	SetPositions(graphic, worldPosition);
	// DrawSpellGraphic draws the mesh at GSpellSeedInfo.scale x scale, and only when useMesh is set: STORM, FIRE,
	// LIGHTNING_BOLT, WATER and TELEPORT have 0, only their holder effect shows
	const auto mesh = ReplacedMesh(seed, player).value_or(info.mesh);
	registry.Assign<Transform>(entity, graphic.meshPosition, glm::mat3(1.0f), glm::vec3(info.scale * scale));
	if (info.useMesh != 0)
	{
		registry.Assign<Mesh>(entity, resources::HashIdentifier(mesh), static_cast<int8_t>(0), static_cast<int8_t>(0));
	}
	// The holder effect at point + unknown0x154 x scale. The graphic owns it and steps it itself (DrawUpdateAtPos /
	// UpdateOnly every drawn frame, ProcessTurn every turn when auto-updated), drawn as last stepped (time multiplier 1)
	if (info.holderParticle != ParticleType::None)
	{
		const auto file = psys::ParticleTypeFile(info.holderParticle);
		if (!file.empty())
		{
			auto& component = registry.Get<SpellSeedGraphic>(entity);
			component.psys =
			    psys::manager::StartForSpell(std::string(file), component.effectPosition, glm::vec3(0.0f), scale, nullptr);
			if (component.psys != 0)
			{
				psys::manager::SetPerFrame(component.psys);
				// On an orb or an icon the whole effect is one Z-sorted object (the creature room and world room
				// draw is not ported)
				psys::manager::SetDrawPath(component.psys, psys::manager::DrawPath::Queued);
			}
		}
	}
	if (powerUp != -1)
	{
		UpdateBands(entity);
	}
	return entity;
}

void seed_graphic::Delete(entt::entity graphic)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (graphic == entt::null || !registry.Valid(graphic))
	{
		return;
	}
	if (auto* component = registry.TryGet<SpellSeedGraphic>(graphic); component != nullptr)
	{
		if (component->psys != 0)
		{
			psys::manager::Delete(component->psys);
		}
		if (component->band != entt::null && registry.Valid(component->band))
		{
			registry.Destroy(component->band);
		}
		for (const auto band : component->extraBands)
		{
			if (registry.Valid(band))
			{
				registry.Destroy(band);
			}
		}
	}
	registry.Destroy(graphic);
	registry.SetDirty();
}

void seed_graphic::SetPowerUpType(entt::entity graphic, int powerUp)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (graphic == entt::null || !registry.Valid(graphic))
	{
		return;
	}
	registry.Get<SpellSeedGraphic>(graphic).powerUp = powerUp;
	UpdateBands(graphic);
}

void seed_graphic::SetAutoUpdate(entt::entity graphic, bool autoUpdate)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (graphic != entt::null && registry.Valid(graphic))
	{
		registry.Get<SpellSeedGraphic>(graphic).autoUpdate = autoUpdate;
	}
}

void seed_graphic::SetBandScale(entt::entity graphic, float bandScale)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (graphic != entt::null && registry.Valid(graphic))
	{
		registry.Get<SpellSeedGraphic>(graphic).bandScale = bandScale;
	}
}

void seed_graphic::DrawUpdateAtPos(entt::entity graphic, const glm::vec3& point, float scale, float milliseconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (graphic == entt::null || !registry.Valid(graphic) || !registry.AllOf<SpellSeedGraphic>(graphic))
	{
		return;
	}
	auto& component = registry.Get<SpellSeedGraphic>(graphic);
	// The new scale, then the positions from the point
	component.scale = scale;
	SetPositions(component, point);
	// The effect moves to the effect point, magnitude = scale, and steps by milliseconds
	if (auto* effect = component.psys != 0 ? psys::manager::Find(component.psys) : nullptr; effect != nullptr)
	{
		effect->SetOrigin(component.effectPosition);
		effect->SetMagnitude(scale);
		StepEffect(component.psys, milliseconds);
	}
}

void seed_graphic::UpdateOnly(entt::entity graphic, float milliseconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (graphic == entt::null || !registry.Valid(graphic) || !registry.AllOf<SpellSeedGraphic>(graphic))
	{
		return;
	}
	StepEffect(registry.Get<const SpellSeedGraphic>(graphic).psys, milliseconds);
}

void seed_graphic::DrawSpellGraphic(entt::entity graphicEntity, uint8_t alpha, float milliseconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (graphicEntity == entt::null || !registry.Valid(graphicEntity) ||
	    !registry.AllOf<SpellSeedGraphic, Transform>(graphicEntity))
	{
		return;
	}
	auto& graphic = registry.Get<SpellSeedGraphic>(graphicEntity);
	const auto& info = magic::GetSpellSeedInfo(Locator::infoConstants::value(), graphic.seedType);
	const float seconds = milliseconds * 0.001f;

	// Only with useMesh. Size = GSpellSeedInfo.scale x scale; the y angle += 2 x dt, fmod 2 pi (negative + 2 pi)
	// (with useMesh 0 the whole mesh part is skipped, the angle too)
	if (info.useMesh != 0)
	{
		graphic.spin = std::fmod(graphic.spin + k_SpinRate * seconds, k_TwoPi);
		if (graphic.spin < 0.0f)
		{
			graphic.spin += k_TwoPi;
		}
		// A creature spell phial (the seed's magic is a creature spell): its 8 x 4 sheet at -15 frames a second
		// (frame_anim::SpellIconFrame) through the object's UV offset. The magic is magicTypes[0], the one power-up
		// level -1 maps to
		const auto magicType = info.magicTypes[0];
		const bool phial =
		    static_cast<size_t>(magicType) < magic::k_MagicTypeCount &&
		    magic::GetMagicInfoAs<GMagicCreatureSpellInfo>(Locator::infoConstants::value(), magicType) != nullptr;
		// the light the mesh takes (components::SpellSeedGraphic::landCellLight, RenderingSystem LandLightOf)
		graphic.landCellLight = !phial;
		if (phial)
		{
			const auto uv = graphics::frame_anim::SpellIconFrame(graphic.uvPhase, seconds);
			auto* scroll = registry.TryGet<UvScroll>(graphicEntity);
			if (scroll == nullptr)
			{
				scroll = &registry.Assign<UvScroll>(graphicEntity);
			}
			scroll->u = uv.x;
			scroll->v = uv.y;
		}
	}
	// A player seed (its base magic is not a creature spell): the mesh takes the land's light of the cell under it, with
	// no haze after it (landCellLight, land_light::ObjectMode::Cell as for the spell icon), and sits at the point turned
	// by the y angle: rows X = (cos, 0, sin), Z = (-sin, 0, cos). No bob and no pulse: those (and the 0.7 / 0.8 / 1.5
	// squashes) are only for the creature spell phials (12..27); of that branch only the UV frames (above) are ported.
	auto& transform = registry.Get<Transform>(graphicEntity);
	transform.position = graphic.meshPosition;
	transform.rotation = affine::AngleY(graphic.spin);
	transform.scale = glm::vec3(info.scale * graphic.scale);
	// The alpha byte does not survive: it goes into the diffuse colour's top byte and turns global alpha on
	// (alpha != 0xFF), but the land light then overwrites the whole colour with table[luminosity] (or table[255] off the
	// map), whose alpha is 0xFF (every palette.raw texel's alpha is 0xFF, LandLightTable). So the mesh is drawn with a
	// global alpha of 0xFF: it looks opaque. Only the bands and the holder effect take the caller's alpha
	if (alpha != 0xFF)
	{
		registry.AssignOrReplace<Alpha>(graphicEntity, 1.0f);
	}
	else if (registry.AllOf<Alpha>(graphicEntity))
	{
		registry.Remove<Alpha>(graphicEntity);
	}
	// The holder effect gets the caller's alpha and is drawn: every particle's alpha becomes (alpha x it) >> 8 unless
	// it is 0xFF, so in a one-shot orb (0x95) the seed's additive effect (FIRE's SF_FireBallOnHolder...) adds 149/256
	// of its light, on an icon (0xFF) all of it
	if (auto* effect = graphic.psys != 0 ? psys::manager::Find(graphic.psys) : nullptr; effect != nullptr)
	{
		effect->SetGlobalAlpha(static_cast<float>(alpha));
	}

	// The bands, when the power-up is not -1 and the band object exists: the band angle += 10.3 dt, the second += dt
	// (fmod 2 pi), one drawing per level 0..pu at the point with size 0.2 x bandScale x scale, turned to the camera
	// (billboard::BandToEye), each drawn twice (k_DrawsPerBand) in the owner's colour (SetBandColour)
	if (graphic.powerUp != -1 && graphic.band != entt::null)
	{
		// the angles only move inside this branch
		graphic.bandSpin = std::fmod(graphic.bandSpin + k_BandSpinRate * seconds, k_TwoPi);
		graphic.bandSpin2 = std::fmod(graphic.bandSpin2 + k_BandSpin2Rate * seconds, k_TwoPi);
		const float size = k_BandScale * graphic.bandScale * graphic.scale;
		// The camera of this frame; the band's translation is the point
		const auto toEye = Locator::camera::has_value()
		                       ? graphics::billboard::BandToEye(graphic.point, Locator::camera::value().GetOrigin())
		                       : glm::mat3(1.0f);
		for (size_t i = 0; i <= graphic.extraBands.size(); ++i)
		{
			const auto band = i == 0 ? graphic.band : graphic.extraBands[i - 1];
			if (registry.Valid(band))
			{
				auto& bandTransform = registry.Get<Transform>(band);
				bandTransform.position = graphic.point;
				bandTransform.rotation = toEye * BandRotation(graphic.bandSpin, i / k_DrawsPerBand);
				bandTransform.scale = glm::vec3(size);
				SetBandColour(band, graphic.player, alpha);
			}
		}
	}
	registry.SetDirty();
}

void seed_graphic::UpdateIconGraphics(float milliseconds)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> graphics;
	registry.Each<const SpellIcon>([&graphics](const SpellIcon& icon) {
		if (icon.graphic != entt::null)
		{
			graphics.push_back(icon.graphic);
		}
	});
	// Worship-site and town-centre icons: UpdateOnly, then DrawSpellGraphic with the icon's alpha; both icons are drawn
	// with a white diffuse, so 0xFF. (inferred): every frame, the original only when the icon was on screen
	for (const auto graphic : graphics)
	{
		UpdateOnly(graphic, milliseconds);
		DrawSpellGraphic(graphic, 0xFF, milliseconds);
	}
}

void seed_graphic::ProcessTurn()
{
	const auto turnMilliseconds = static_cast<float>(magic::k_TurnMs); // the turn length
	// The auto-updated graphics step their holder effect with the zeroed info; every 30 turns the FLYING_FLOCK mesh is
	// redone for the player's alignment
	auto& registry = Locator::entitiesRegistry::value();
	// The game's turn
	const uint32_t turn = game_clock::Turn();
	const bool refreshMesh = (turn % 0x1E) == 0;
	registry.Each<SpellSeedGraphic>([&registry, turnMilliseconds, refreshMesh](entt::entity entity, SpellSeedGraphic& graphic) {
		if (graphic.autoUpdate)
		{
			StepEffect(graphic.psys, turnMilliseconds);
		}
		if (refreshMesh && registry.AllOf<Mesh>(entity))
		{
			if (const auto mesh = ReplacedMesh(graphic.seedType, graphic.player); mesh.has_value())
			{
				registry.Get<Mesh>(entity).id = resources::HashIdentifier(*mesh);
			}
		}
	});
}

void seed_graphic::UpdatePhase(float milliseconds)
{
	auto& phase = SeedGraphic().phase;
	phase += milliseconds * 0.001f / 3.33f;
	while (phase > 1.0f)
	{
		phase -= 1.0f;
	}
}

float seed_graphic::Phase()
{
	return SeedGraphic().phase;
}
