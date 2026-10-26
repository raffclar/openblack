/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TattooMenuSystem.h"

#include <cstdint>

#include <algorithm>
#include <chrono>
#include <span>
#include <system_error>
#include <vector>

#include <PhysiqueFile.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/mat3x3.hpp>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/ScreenPick.h"
#include "Camera/Camera.h"
#include "Creature/CaveCreature.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureRig.h"
#include "Creature/TattooMenu.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/TattooMenuCreature.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureCarryOverSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureHairSystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "ECS/TattooSites.h"
#include "Graphics/InterfaceCreatures.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

namespace
{
std::optional<std::filesystem::path> ProfilePhysique()
{
	return Locator::creatureCarryOverSystem::has_value() ? Locator::creatureCarryOverSystem::value().ProfilePhysique()
	                                                     : std::nullopt;
}
} // namespace

bool TattooMenuSystem::IsButtonShown(bool restartPending) const
{
	const auto physique = ProfilePhysique();
	std::error_code error;
	const bool kept = physique.has_value() && std::filesystem::is_regular_file(*physique, error);
	const bool shown = !Locator::creatureCaveSystem::has_value() || Locator::creatureCaveSystem::value().IsCreatureShown();
	return tattoo_menu::ButtonShown(kept, shown, restartPending);
}

void TattooMenuSystem::Open()
{
	Close();
	auto& registry = Locator::entitiesRegistry::value();
	const auto physique = ProfilePhysique();
	const auto shown =
	    tattoo_menu::FromPhysique(physique.has_value() ? creaturemind::ReadPhysiqueFile(*physique) : std::nullopt,
	                              [](CreatureType species) { return archetypes::CreatureArchetype::SpeciesStrength(species); });

	// The body: a creature's shape, skin and tattoos, which nothing of the world looks for, as it is nowhere in it
	const auto body = registry.Create();
	registry.Assign<Creature>(body, Creature {.owner = PlayerNames::NEUTRAL,
	                                          .leashable = false,
	                                          .species = shown.species,
	                                          .mind = 0,
	                                          .size = tattoo_menu::k_Size});
	registry.Assign<Mesh>(body, creature::GetIdFromType(shown.species, creature::CreatureBody::Appearance::Base));
	registry.Assign<CreatureMorph>(body, CreatureMorph {.shownFatness = 0.5f, .drawn = shown.morph, .revision = 0});
	registry.Assign<CreatureTattoos>(body);
	auto& marks = registry.Assign<CreatureMarks>(body);
	marks.marks.wounds = shown.wounds;
	marks.marks.blood = shown.blood;
	registry.Assign<CreatureSkin>(body);

	// The copy that is posed: at the world's origin, facing as its mesh does
	const auto copy = registry.Create();
	registry.Assign<Transform>(copy, glm::vec3(0.0f), glm::mat3(1.0f),
	                           glm::vec3(archetypes::CreatureArchetype::DrawnScale(shown.species, tattoo_menu::k_Size)));
	registry.Assign<CreatureCopy>(copy, body, tattoo_menu::k_Size);
	registry.Assign<CreatureAnimation>(copy);
	registry.Assign<CreatureEyes>(copy);
	// It has hair of its own. The hair starts again each time that the editor opens.
	registry.Assign<CreatureHair>(copy);
	cave_creature::State state {.rest = cave_creature::Rest::Sit, .heading = GameHeading()};
	// It is shown from the start, so it does not turn round as the cave's creature does when the camera comes to it
	state.zoomed = true;
	cave_creature::StartIdle(state, [this](uint32_t range) {
		return range == 0 ? 0u : std::uniform_int_distribution<uint32_t>(0, range - 1)(_random);
	});
	registry.Assign<TattooMenuCreature>(copy, TattooMenuCreature {.state = state, .tattooSites = std::nullopt});
	_body = body;
	_copy = copy;

	// The tattoos start as the player's creature has them, or with none
	std::optional<entt::entity> player;
	if (Locator::playerSystem::has_value())
	{
		const auto& players = Locator::playerSystem::value();
		player = players.GetPrimaryCreature(players.GetLocalPlayer());
	}
	if (Locator::tattooEditorSystem::has_value())
	{
		Locator::tattooEditorSystem::value().OpenFromMenu(player, body, _view);
	}
}

void TattooMenuSystem::Close()
{
	if (Locator::tattooEditorSystem::has_value())
	{
		auto& editor = Locator::tattooEditorSystem::value();
		if (editor.IsFromMenu())
		{
			_view = editor.GetView();
			editor.Close();
		}
	}
	if (Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		for (const auto entity : {_copy, _body})
		{
			if (entity.has_value() && registry.Valid(*entity))
			{
				registry.Destroy(*entity);
			}
		}
	}
	_copy.reset();
	_body.reset();
}

glm::mat4 TattooMenuSystem::View() const
{
	const auto& orbit = Locator::tattooEditorSystem::has_value() && Locator::tattooEditorSystem::value().IsFromMenu()
	                        ? Locator::tattooEditorSystem::value().GetView()
	                        : _view;
	return glm::lookAtLH(orbit.eye, orbit.focus, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 TattooMenuSystem::Projection() const
{
	// The world's camera's projection, as on a 4:3 screen whatever the window's shape
	return Locator::camera::has_value()
	           ? graphics::interface_creatures::SpaceProjection(Locator::camera::value().GetProjectionMatrix())
	           : glm::mat4(1.0f);
}

void TattooMenuSystem::Update(float milliseconds, glm::ivec2 pointer, bool buttonHeld, bool leftHeld, const Viewport& viewport)
{
	_viewport = viewport;
	if (!_copy.has_value())
	{
		return;
	}
	// The editor closed: the creature goes with it
	if (!Locator::tattooEditorSystem::has_value() || !Locator::tattooEditorSystem::value().IsFromMenu())
	{
		Close();
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto* menu = registry.TryGet<TattooMenuCreature>(*_copy);
	auto* animation = registry.TryGet<CreatureAnimation>(*_copy);
	if (menu == nullptr || animation == nullptr)
	{
		return;
	}
	const auto frameMs = std::min(milliseconds, cave_creature::k_MaxFrameMs);
	const cave_creature::Random random = [this](uint32_t range) {
		return range == 0 ? 0u : std::uniform_int_distribution<uint32_t>(0, range - 1)(_random);
	};
	cave_creature::Frame frame {.zoomed = true, .buttonHeld = buttonHeld, .leftHeld = leftHeld, .milliseconds = frameMs};
	const auto pixel = glm::vec2(pointer) - viewport.origin;
	if (const auto point = tattoo_menu::PointerPoint(View(), Projection(), viewport.size, pixel); point.has_value())
	{
		frame.pointer = *point;
	}

	// It smiles when a tattoo goes on and is sad when one comes off
	if (const auto face = Locator::tattooEditorSystem::value().TakeFace(); face.has_value())
	{
		animation->face = creature_layers::PullFace(animation->face, creature_face::AnimationOf(*face),
		                                            creature_tattoo_editor::k_FaceMilliseconds, creature_face::Cue::Tattooed);
	}
	cave_creature::Body body {.body = animation->body, .gesture = animation->gesture, .lookAt = animation->lookAt};
	cave_creature::InEditor(menu->state, body, frame, random);
	animation->body = body.body;
	animation->gesture = body.gesture;
	animation->lookAt = body.lookAt;
	if (Locator::creatureAnimationSystem::has_value())
	{
		Locator::creatureAnimationSystem::value().PoseCopy(Locator::entitiesRegistry::value(), *_copy, frameMs);
	}
	// Its hair swings from the new pose by the whole time of the frame, as the game is paused under the menu
	if (Locator::creatureHairSystem::has_value())
	{
		Locator::creatureHairSystem::value().StepCopy(Locator::entitiesRegistry::value(), *_copy,
		                                              std::chrono::duration<float, std::milli>(milliseconds));
	}
	HoverSites(pointer);
}

void TattooMenuSystem::HoverSites(glm::ivec2 pointer)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& editor = Locator::tattooEditorSystem::value();
	auto* menu = registry.TryGet<TattooMenuCreature>(*_copy);
	const auto* animation = registry.TryGet<const CreatureAnimation>(*_copy);
	const auto* transform = registry.TryGet<const Transform>(*_copy);
	const auto* meshComponent = _body.has_value() ? registry.TryGet<const Mesh>(*_body) : nullptr;
	const auto* body = _body.has_value() ? registry.TryGet<const Creature>(*_body) : nullptr;
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (menu == nullptr || animation == nullptr || transform == nullptr || meshComponent == nullptr || body == nullptr ||
	    !meshes.Contains(meshComponent->id) || _viewport.size.x <= 0.0f || _viewport.size.y <= 0.0f ||
	    !Locator::camera::has_value())
	{
		editor.Hover({}, pointer);
		return;
	}
	const auto mesh = meshes.Handle(meshComponent->id);
	if (!menu->tattooSites.has_value())
	{
		menu->tattooSites = tattoo_sites::Find(*mesh, body->species);
	}
	const auto view = View();
	const auto projection = Projection();
	const screen_pick::View pick {
	    .worldToClip = projection * view,
	    .resolution = _viewport.size,
	    .near = Locator::camera::value().GetNearClip(),
	    .xScale = projection[0][0],
	    .camera = glm::vec3(glm::inverse(view)[3]),
	};
	const auto model = creature::PlacementMatrix(transform->position, transform->rotation, transform->scale);
	const auto& rest = mesh->GetBoneMatrices();
	const auto bones = animation->boneMatrices.size() == rest.size() ? std::span<const glm::mat4>(animation->boneMatrices)
	                                                                 : std::span<const glm::mat4>(rest);
	const auto sites =
	    tattoo_sites::OnScreen(registry, *_body, *mesh, model, bones, *menu->tattooSites, pick, glm::ivec2(_viewport.origin));
	editor.Hover(sites, pointer);
}

std::optional<TattooMenuSystemInterface::Shown> TattooMenuSystem::GetShown() const
{
	if (!_copy.has_value())
	{
		return std::nullopt;
	}
	return Shown {.copy = *_copy, .view = View(), .projection = Projection(), .viewport = _viewport};
}
