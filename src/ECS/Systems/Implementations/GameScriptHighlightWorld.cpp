/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GameScriptHighlightWorld.h"

#include <algorithm>

#include <LHVM.h>
#include <entt/core/hashed_string.hpp>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "CHLApi.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Graphics/Texture2D.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace rules = openblack::ecs::script_highlights;

namespace
{
/// The sprite sheet the glow is on, and its alpha
constexpr entt::hashed_string k_Sheet = entt::hashed_string("raw/S_SpriteSheet3");
constexpr entt::hashed_string k_SheetAlpha = entt::hashed_string("raw/S_SpriteSheet3a");
/// The game's own bank of sounds
constexpr int32_t k_InGameBank = 1;
} // namespace

ecs::Registry& GameScriptHighlightWorld::Entities()
{
	return Locator::entitiesRegistry::value();
}

std::optional<rules::KindInfo> GameScriptHighlightWorld::InfoOf(uint32_t kind) const
{
	if (!Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& rows = Locator::infoConstants::value().scriptHighlight;
	if (kind >= rows.size())
	{
		return std::nullopt;
	}
	const auto& row = rows.at(kind);
	return rules::KindInfo {
	    .normal = row.normal,
	    .active = row.active,
	    .glints = row.particleTypeGlints,
	    .activeEffect = row.particleTypeActive,
	};
}

std::optional<rules::ModelBox> GameScriptHighlightWorld::BoxOf(MeshId mesh) const
{
	if (!Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto id = resources::HashIdentifier(mesh);
	if (!meshes.Contains(id))
	{
		return std::nullopt;
	}
	const auto box = meshes.Handle(id)->GetBoundingBox();
	return rules::ModelBox {.centre = box.Center(), .halfSize = box.Size() * 0.5f};
}

float GameScriptHighlightWorld::LandHeight(glm::vec2 point) const
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
}

std::vector<rules::ThingBelow> GameScriptHighlightWorld::ThingsBelow(entt::entity highlight, glm::vec3 point) const
{
	std::vector<rules::ThingBelow> things;
	if (!Locator::entitiesMap::has_value())
	{
		return things;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto cell = ecs::MapInterface::GetGridCell(point);
	for (const auto thing : Locator::entitiesMap::value().GetAllInCell(glm::ivec2(cell)))
	{
		if (thing == highlight || !registry.Valid(thing))
		{
			continue;
		}
		const auto* transform = registry.TryGet<const Transform>(thing);
		if (transform == nullptr)
		{
			continue;
		}
		// How far its model reaches across and how tall it stands, at its size
		float radius = 0.0f;
		float height = 0.0f;
		if (const auto* mesh = registry.TryGet<const Mesh>(thing); mesh != nullptr && Locator::resources::has_value())
		{
			const auto& meshes = Locator::resources::value().GetMeshes();
			if (meshes.Contains(mesh->id))
			{
				const auto size = meshes.Handle(mesh->id)->GetBoundingBox().Size();
				radius = std::max(size.x, size.z) * 0.5f * transform->scale.x;
				height = size.y * transform->scale.y;
			}
		}
		const auto& at = transform->position;
		things.push_back({
		    .inCell = rules::InCell({at.x, at.z}),
		    .radius = radius,
		    .top = at.y - LandHeight({at.x, at.z}) + height,
		    // TODO(script-natives): the game also skips a thing whose position changed since its last turn; openblack
		    // keeps no last position, so only a thing in flight counts as moving
		    .livingOrMoving = registry.AnyOf<Villager, Creature, Animal, InPhysics>(thing),
		});
	}
	return things;
}

uint32_t GameScriptHighlightWorld::Turn() const
{
	return Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
}

float GameScriptHighlightWorld::MillisecondsPerTurn() const
{
	return static_cast<float>(TimeSystemInterface::k_TurnDuration.count());
}

bool GameScriptHighlightWorld::ScrollsDrawn() const
{
	return !Locator::chlapi::has_value() || Locator::chlapi::value().IsHighlightDrawOn();
}

uint32_t GameScriptHighlightWorld::StartEffect(ParticleType type, glm::vec3 at)
{
	if (!Locator::particleSystem::has_value())
	{
		return ParticleSystemInterface::k_NoEffect;
	}
	return Locator::particleSystem::value().Start(type, at, 1.0f);
}

void GameScriptHighlightWorld::TargetEffect(uint32_t effect, entt::entity target)
{
	if (Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().AddTarget(effect, target);
	}
}

void GameScriptHighlightWorld::StepEffect(uint32_t effect, float seconds)
{
	if (Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().ProcessByFrame(effect, seconds);
	}
}

void GameScriptHighlightWorld::MoveEffect(uint32_t effect, glm::vec3 to)
{
	if (Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().SetOrigin(effect, to);
	}
}

void GameScriptHighlightWorld::DeleteEffect(uint32_t effect)
{
	if (Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().Delete(effect);
	}
}

void GameScriptHighlightWorld::PlaySound(uint32_t sample)
{
	if (Locator::chlapi::has_value())
	{
		Locator::chlapi::value().PlayBankSoundEffect(k_InGameBank, static_cast<int32_t>(sample), std::nullopt);
	}
}

std::optional<Sprite> GameScriptHighlightWorld::GlowLook() const
{
	if (!Locator::resources::has_value())
	{
		return std::nullopt;
	}
	auto& textures = Locator::resources::value().GetTextures();
	if (!textures.Contains(k_Sheet.value()) || !textures.Contains(k_SheetAlpha.value()))
	{
		return std::nullopt;
	}
	// One picture of the sheet's 8 by 8, white, added to what is behind it by its alpha
	constexpr float k_Cell = 1.0f / static_cast<float>(rules::k_GlowSheetPictures);
	const auto column = static_cast<float>(rules::k_GlowPicture % rules::k_GlowSheetPictures);
	const auto row = static_cast<float>(rules::k_GlowPicture / rules::k_GlowSheetPictures);
	return Sprite {
	    .texture = textures.Handle(k_Sheet.value())->GetNativeHandle(),
	    .uvMin = glm::vec2(column, row) * k_Cell,
	    .uvExtent = glm::vec2(k_Cell),
	    .tint = glm::vec4(1.0f),
	    .additive = true,
	    .facesCamera = true,
	    .alpha = textures.Handle(k_SheetAlpha.value())->GetNativeHandle(),
	};
}

void GameScriptHighlightWorld::HelpEvent(uint32_t /*event*/)
{
	// TODO(script-natives): the help system's profile of what the player has done and been told isn't in openblack yet;
	// a tap is one of its events (see docs scripts/highlights.md)
}

void GameScriptHighlightWorld::StartHelpScript(std::string_view name)
{
	if (Locator::vm::has_value() && Locator::dialogueControlSystem::has_value())
	{
		chlapi::CHLApi::StartHelpScript(name);
	}
}

void GameScriptHighlightWorld::ShowTip(entt::entity /*sign*/, uint32_t /*text*/, uint32_t /*category*/)
{
	// TODO(script-natives): the help system's bubble, which shows a sign's tip, isn't in openblack yet
}

void GameScriptHighlightWorld::HideTip()
{
	// TODO(script-natives): the help system's bubble isn't in openblack yet
}

void GameScriptHighlightWorld::ReplayChallenge(uint32_t /*challenge*/)
{
	// TODO(script-natives): the temple's log of challenges (the scripts' snapshots) isn't in openblack yet; a started
	// scroll tapped again plays its challenge's last message from it
}
