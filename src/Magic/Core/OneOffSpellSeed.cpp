/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "OneOffSpellSeed.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "Audio/Audio.h"
#include "Camera/Camera.h"
#include "ECS/Archetypes/OneOffSpellSeedArchetype.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Graphics/ArgbColour.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Players.h"
#include "Resources/ResourcesInterface.h"
#include "Spell.h"
#include "SpellSeed.h"
#include "Worship/PlayerSpellIcons.h"
#include "Worship/SpellSeedGraphic.h"
#include "Worship/WorshipSpellIcon.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;

entt::entity one_off::Create(const glm::vec3& worldPosition, SpellSeedType seedType, int powerUp, float scale)
{
	const auto orb = ecs::archetypes::OneOffSpellSeedArchetype::Create(worldPosition, seedType, powerUp, scale);
	if (orb != entt::null)
	{
		// into its cell at once; its info (mobile object info 25) is type 20 MOBILE_OBJECT, so the head of the mobile
		// list
		ecs::map_cells::InsertMapObject(orb);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Magic: one-shot orb {} (seed {}, pu {}) at ({:.1f}, {:.1f}, {:.1f})",
		                   static_cast<uint32_t>(orb), static_cast<int>(seedType), powerUp, worldPosition.x, worldPosition.y,
		                   worldPosition.z);
	}
	return orb;
}

entt::entity one_off::CreateSpellIntoHand(PlayerNames player, SpellSeedType seedType, int powerUp, float multiplier)
{
	if (!Locator::handSystem::has_value() || static_cast<int>(seedType) < 0 || static_cast<int>(seedType) >= 30)
	{
		return entt::null;
	}
	auto& hand = Locator::handSystem::value();
	// the hand must be ready for an object (inferred: nothing held)
	if (hand.GetHeldObject().has_value())
	{
		return entt::null;
	}
	// at the interface's hand position (altitude 0): the player's best icon for the seed, and with one a seed of the
	// icon, its creator the icon (Worship/WorshipSpellIcon.cpp), else a free seed of that type. (inferred: openblack's
	// right hand stands for the interface's; no hand gives (0, 0))
	using Side = ecs::systems::HandSystemInterface::Side;
	const auto hands = hand.GetPlayerHandPositions();
	const glm::vec3 hand3d = hands[static_cast<size_t>(Side::Left)].value_or(glm::vec3(0.0f)); // the one hand
	const glm::vec3 handPosition = magic::ToWorld(glm::vec3(hand3d.x, 0.0f, hand3d.z));        // altitude 0: on the land
	const auto icon = worship::player::FindBestSpellIconForSpellSeed(player, seedType);
	const auto entity = icon != entt::null ? worship::icon::CreateSeed(icon, handPosition, player, powerUp, multiplier)
	                                       : seed::Create(handPosition, seedType, player, powerUp, multiplier);
	auto& registry = Locator::entitiesRegistry::value();
	auto& component = registry.Get<SpellSeed>(entity);
	const auto& info = GetSpellSeedInfo(Locator::infoConstants::value(), seedType);
	players::SetMagicTypeEverBeenEnabled(player, MagicTypeForPowerUpLevel(info, powerUp));
	component.fromOneShot = true;
	seed::AddToChantStore(component, seed::GetChantNeeded(component, powerUp));
	// the hand holds it, then the seed's InterfaceSetInMagicHand
	hand.PlaceObjectInMagicHand(entity);
	if (seed::InterfaceSetInMagicHand(entity) != 1)
	{
		return entt::null;
	}
	seed::SetInactive(registry.Get<SpellSeed>(entity), false);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Magic: seed {} ({}, pu {}) in the hand with {:.0f} chants, ready {}, icon {}",
	                   static_cast<uint32_t>(entity), info.debugString.data(), powerUp,
	                   registry.Get<SpellSeed>(entity).chantStore, registry.Get<SpellSeed>(entity).ready,
	                   icon == entt::null ? -1 : static_cast<int>(icon));
	return entity;
}

int one_off::InterfaceTap(entt::entity orb, PlayerNames player)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto component = registry.Get<const OneOffSpellSeed>(orb); // a copy: the seed's creation adds entities
	if (CreateSpellIntoHand(player, component.seedType, component.powerUp, component.scale) == entt::null)
	{
		return 0;
	}
	// TODO: start immersion 14
	// the bubble pop (G_SpellBubblePop_04, bank InGame), owned by the orb, 3D, not tracked, at the hand's position.
	// (approximate) the left hand's interaction point stands for the interface's hand position.
	{
		audio::PlayOptions options;
		options.sample = {audio::Bank(audio::SfxBank::InGame), 0x6D};
		options.owner = audio::Owner::Thing(orb);
		options.is3D = true;
		options.track = false;
		if (Locator::handSystem::has_value())
		{
			const auto left = static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left);
			const auto hand = Locator::handSystem::value().GetPlayerHandPositions()[left];
			const auto* transform =
			    registry.TryGet<const ecs::components::Transform>(Locator::handSystem::value().GetPlayerHands()[left]);
			options.position = hand.value_or(transform != nullptr ? transform->position : glm::vec3(0.0f));
		}
		audio::PlaySoundEffect(options);
	}
	worship::seed_graphic::Delete(component.graphic); // the seed graphic inside goes with it
	ecs::map_cells::RemoveMapObject(orb);             // out of its cell
	registry.Destroy(orb);
	registry.SetDirty();
	return 3;
}

void one_off::InterfaceSetInMagicHand(entt::entity orb, PlayerNames player)
{
	const auto& component = Locator::entitiesRegistry::value().Get<OneOffSpellSeed>(orb);
	const auto& info = GetSpellSeedInfo(Locator::infoConstants::value(), component.seedType);
	players::SetMagicTypeEverBeenEnabled(player, MagicTypeForPowerUpLevel(info, component.powerUp));
}

void one_off::UpdateFrames(float milliseconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	bool any = false;
	registry.Each<OneOffSpellSeed, UvScroll>([&](entt::entity /*orb*/, OneOffSpellSeed& orb, UvScroll& scroll) {
		// 18 frames a second over the 4 x 4 sheet (frame_anim::OneOffFrame)
		const auto uv = graphics::frame_anim::OneOffFrame(orb.phase, milliseconds);
		scroll.u = uv.x;
		scroll.v = uv.y;
		any = true;
	});
	// When drawn (always on), the mesh is turned about the centre c of its box (all the submeshes) so that its +Y points
	// at the camera (graphics::billboard::LookAtCentre). The visible submesh is the half sphere above c, so the bubble
	// looks round from every side. The object's position does not move (only its 3D matrix does) and the physics sphere
	// is the same after the turn.
	const auto camera = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : glm::vec3(0.0f);
	registry.Each<OneOffSpellSeed, const Transform, const Mesh>(
	    [&registry, &camera, hasCamera = Locator::camera::has_value()](entt::entity entity, OneOffSpellSeed& orb,
	                                                                   const Transform& transform, const Mesh& mesh) {
		    if (!hasCamera)
		    {
			    return;
		    }
		    const auto l3d = Locator::resources::value().GetMeshes().Handle(mesh.id);
		    const glm::vec3 centre = l3d ? l3d->GetBoundingBox().Center() : glm::vec3(0.0f);
		    // the 3D object's scale (the orb is scaled uniformly)
		    // where the orb is drawn this frame (in the hand or flying it is not its logic position)
		    const auto lookAt =
		        graphics::billboard::LookAtCentre(ecs::DrawnPosition(registry, entity), centre, transform.scale.x, camera);
		    orb.facing = lookAt.axes;
		    orb.facingOffset = lookAt.offset;
	    });
	// The rest of the draw: the orb is added for drawing with its matrix moved to the box centre +
	// normalize(camera - centre) x its radius (the larger half extent x/z x scale), then put back: only the alpha sort
	// key moves, so the orb sorts in front of the seed inside and the seed is drawn first, seen through the bubble. Then,
	// if the orb was on screen, the seed graphic's position (the drawn matrix applied to the box centre the orb turns
	// about (inferred); scale = the 3D object's scale x 0.6), its update at that point and its draw with the orb's
	// diffuse alpha 0x95: the seed spins in the centre of the bubble and goes with the orb when it is carried or thrown.
	// (inferred): every frame, not only when on screen.
	registry.Each<OneOffSpellSeed, const Transform, const Mesh>(
	    [&registry, &camera, milliseconds, hasCamera = Locator::camera::has_value()](
	        entt::entity entity, OneOffSpellSeed& orb, const Transform& transform, const Mesh& mesh) {
		    const auto l3d = Locator::resources::value().GetMeshes().Handle(mesh.id);
		    // the seed graphic's position: the box centre through the matrix the orb is drawn with this frame, so
		    // the seed stays inside the bubble in the hand and in flight
		    const glm::vec3 boxCentre = l3d ? l3d->GetBoundingBox().Center() : glm::vec3(0.0f);
		    glm::vec3 centre = glm::vec3(ecs::DrawnModel(registry, entity, false) * glm::vec4(boxCentre, 1.0f));
		    const float radius = ecs::object::GetRadius(entity);
		    orb.sortPoint = centre;
		    if (hasCamera && glm::dot(camera - centre, camera - centre) > 0.0f)
		    {
			    orb.sortPoint = centre + glm::normalize(camera - centre) * radius;
		    }
		    worship::seed_graphic::DrawUpdateAtPos(orb.graphic, centre, transform.scale.x * 0.6f, milliseconds);
		    // the orb's diffuse alpha: the orb is tinted with alpha 0x96, multiplied into the land colour, whose alpha is
		    // 0xFF: (0xFF x 0x96) >> 8 = 0x95
		    worship::seed_graphic::DrawSpellGraphic(
		        orb.graphic,
		        static_cast<uint8_t>(argb_colour::Alpha(argb_colour::MultiplyArgbShift8(0xFF000000u, 0x96FFFFFFu))),
		        milliseconds);
	    });
	if (any)
	{
		registry.SetDirty();
	}
}
