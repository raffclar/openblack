/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Camera/CreatureInteractCameraModel.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/FightCameraModel.h"
#include "Camera/WorldCamera.h"
#include "Creature/FightMarks.h"
#include "CreatureFightSystem.h"
#include "CreatureFightSystemDetail.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureArena.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureHome.h"
#include "ECS/CreaturePlacement.h"
#include "ECS/CreatureScars.h"
#include "ECS/Map.h"
#include "ECS/PosedModel.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/FightTranscriptNote.h"
#include "ECS/Systems/GuidanceSystemInterface.h"
#include "ECS/ThingCircles.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/ThingModel.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"
#include "Particles/ParticleDrawFrame.h"
#include "Sound/SoundRequestInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace fight = openblack::creature_fight;
namespace marks = openblack::creature_fight::marks;
namespace posed_model = openblack::ecs::posed_model;
using namespace openblack::ecs::systems::fight_detail;

namespace
{
/// Where a sparkle sits now, if what it sits on is still there
std::optional<glm::vec3> MarkPosition(const ecs::Registry& registry, entt::entity self, entt::entity opponent,
                                      const marks::Mark& mark)
{
	if (mark.anchor == marks::Anchor::Ground)
	{
		const auto* locomotion = registry.TryGet<const CreatureLocomotion>(self);
		const auto& transform = registry.Get<const Transform>(self);
		return marks::ToWorld(
		    marks::GroundFrame(transform.position, locomotion != nullptr ? locomotion->heading : GameHeading()), mark.local);
	}
	const auto body = mark.anchor == marks::Anchor::Opponent ? opponent : self;
	const auto drawn = DrawnMeshOf(registry, body);
	if (!drawn.has_value())
	{
		return std::nullopt;
	}
	return posed_model::PointOnSkin(registry, body, *drawn->first, drawn->second,
	                                {.subMesh = mark.subMesh, .firstIndex = mark.firstIndex, .s = mark.s, .t = mark.t});
}

/// A fight's arena is on from when it appears until the duel is over
bool ArenaOn(const ecs::Registry& registry, entt::entity creature)
{
	const auto* fighting = registry.Valid(creature) ? registry.TryGet<const CreatureFighting>(creature) : nullptr;
	return fighting != nullptr && fighting->arenaEntity != entt::null && fighting->stage <= CreatureFighting::Stage::Duel;
}

fight_view::Fighter ViewedFighter(const ecs::Registry& registry, entt::entity creature)
{
	const auto* locomotion = registry.TryGet<const CreatureLocomotion>(creature);
	// The fight's camera looks at the fighters where the frame draws them
	return {.position =
	            DrawnTransform(registry.Get<const Transform>(creature), registry.TryGet<const CreatureAnimation>(creature))
	                .position,
	        .radius = locomotion != nullptr ? locomotion->radius : 5.0f,
	        .height = HeightOf(ShownSize(registry.Get<const Creature>(creature)))};
}
} // namespace

void CreatureFightSystem::DrawMarks(float realMs, const glm::vec3& eye)
{
	std::vector<particles::draw::FightMark> drawn;
	auto& registry = Locator::entitiesRegistry::value();
	const bool step = _marksPace.Advance(realMs);
	// Only the local player's creature shows its sparkles, for as long as it has an opponent
	const auto self = PrimaryCreatureOf(registry, OwnPlayer());
	auto* fighting = self.has_value() ? registry.TryGet<CreatureFighting>(*self) : nullptr;
	if (fighting != nullptr && registry.Valid(fighting->opponent))
	{
		auto& list = fighting->marks;
		if (step)
		{
			list.Animate();
		}
		const auto halfSize = ShownSize(registry.Get<const Creature>(*self));
		const auto shown = list.Marks();
		// While a press on a body is held, the hand goes to a sparkle on the opponent, where it sits before it is drawn
		// nearer the camera
		const auto handMark = marks::HandMark(shown, _markHeld);
		for (size_t place = 0; place < shown.size(); ++place)
		{
			const auto& mark = shown[place];
			if (const auto at = MarkPosition(registry, *self, fighting->opponent, mark))
			{
				if (handMark == place)
				{
					_handSparkle = *at;
				}
				drawn.push_back({.position = marks::DrawnAt(*at, eye),
				                 .halfSize = halfSize,
				                 .uv = marks::FrameUv(mark.frame),
				                 .colour = marks::Rgba(marks::ColourOf(place))});
			}
		}
	}
	if (Locator::effectRequestSystem::has_value())
	{
		Locator::effectRequestSystem::value().SetFightMarks(std::move(drawn));
	}
}

void CreatureFightSystem::UpdateView(float seconds)
{
	// Only this update uses the double click and the look kept before the camera moved
	const auto flightPlace = std::exchange(_flightPlace, std::nullopt);
	const auto look = std::exchange(_look, std::nullopt);
	if (!Locator::camera::has_value())
	{
		return;
	}
	// The fight view is a part of the player's camera of the world, which continues behind the temple
	auto& camera = world_camera::Get();
	const auto& registry = Locator::entitiesRegistry::value();
	if (_view.has_value())
	{
		auto* model = dynamic_cast<FightCameraModel*>(&camera.GetModel());
		if (model == nullptr)
		{
			// While the hand holds a creature, the fight view waits under the camera of the hand. It does not count its
			// time, and it continues when the hand lets go.
			const auto* held = dynamic_cast<const CreatureInteractCameraModel*>(&camera.GetModel());
			if (held == nullptr || dynamic_cast<const FightCameraModel*>(held->GetPlayerModel()) == nullptr)
			{
				_view.reset();
			}
			return;
		}
		_lookSeconds = 0.0f;
		if (_view->lingerSeconds.has_value())
		{
			*_view->lingerSeconds -= seconds;
			if (*_view->lingerSeconds < 0.0f)
			{
				EndView("fight_over");
			}
			return;
		}
		if (ArenaOn(registry, _view->first) && ArenaOn(registry, _view->second))
		{
			model->SetFighters(ViewedFighter(registry, _view->first), ViewedFighter(registry, _view->second));
			// Zoomed far enough out, the player leaves the fight, if they may
			if (model->ZoomedOut() && _fightExit)
			{
				EndView("zoomed_out");
				return;
			}
			// The player who drags the land far enough from the arena leaves the fight
			if (DraggedAway(*model))
			{
				EndView("dragged_away");
			}
			return;
		}
		// Once the fight is over the view lingers a little, if it may end by itself
		if (_fightExit)
		{
			_view->lingerSeconds = fight_view::k_EndSoonSeconds;
		}
		return;
	}
	if (!_cameraWatches)
	{
		return;
	}
	// Only the player's own camera watches for fights: Creature Mode and the hand held to a creature keep their view,
	// and the look starts again when the player's camera comes back
	if (dynamic_cast<const DefaultWorldCameraModel*>(&camera.GetModel()) == nullptr)
	{
		_lookSeconds = 0.0f;
		return;
	}
	// A double click that sends the camera into the arena of a fight shows that fight at once. Only the first arena that
	// holds the place counts.
	if (flightPlace.has_value())
	{
		std::optional<std::pair<entt::entity, entt::entity>> into;
		fight::Arena intoArena;
		registry.Each<const CreatureFighting>([&](entt::entity entity, const CreatureFighting& fighting) {
			if (!into.has_value() && fighting.madeArena && fight::InArena(fighting.arena, flightPlace->place))
			{
				into = std::pair(entity, fighting.opponent);
				intoArena = fighting.arena;
			}
		});
		if (into.has_value())
		{
			if (ArenaOn(registry, into->first) && ArenaOn(registry, into->second))
			{
				TryStartView(into->first, into->second, intoArena, "double_click",
				             std::pair(flightPlace->eye, flightPlace->focus));
			}
			return;
		}
	}
	// The camera looks at an arena with a fight on from within it. When it does this for a while, and the middle of the
	// screen shows land, the fight view starts. The look uses the camera as it was before it moved in this frame.
	if (!look.has_value() || !look->centreShowsLand)
	{
		return;
	}
	const auto eye = look->eye;
	const auto looking = look->looking;
	std::optional<std::pair<entt::entity, entt::entity>> found;
	fight::Arena foundArena;
	registry.Each<const CreatureFighting>([&](entt::entity entity, const CreatureFighting& fighting) {
		if (found.has_value() || !fighting.madeArena || !ArenaOn(registry, entity) || !ArenaOn(registry, fighting.opponent))
		{
			return;
		}
		if (fight_view::WithinArena({eye.x, eye.z}, {looking.x, looking.z}, fighting.arena.centre, fighting.arena.radius))
		{
			found = std::pair(entity, fighting.opponent);
			foundArena = fighting.arena;
		}
	});
	if (!found.has_value())
	{
		return;
	}
	_lookSeconds += seconds;
	if (_lookSeconds > fight_view::k_LookSeconds)
	{
		TryStartView(found->first, found->second, foundArena, "look");
	}
}

void CreatureFightSystem::TryStartView(entt::entity first, entt::entity second, const fight::Arena& arena, std::string_view how,
                                       std::optional<std::pair<glm::vec3, glm::vec3>> from)
{
	auto& camera = world_camera::Get();
	const auto& registry = Locator::entitiesRegistry::value();
	const auto own = PrimaryCreatureOf(registry, OwnPlayer());
	const bool involved = own == first || own == second;
	const bool scripted = registry.AnyOf<ScriptControlled>(first) || registry.AnyOf<ScriptControlled>(second);
	// The camera is too far when it is far from the arena now, and so is the land in the middle of the screen. A double
	// click measures from where the camera was before its flight.
	const auto [eye, looking] = from.has_value() ? *from : ViewNow();
	// The player's creature's fight under a script's control is always watched; any other only from near enough
	const bool tooFar =
	    !(involved && scripted) && fight_view::TooFar({eye.x, eye.z}, {looking.x, looking.z}, arena.centre, arena.radius, 1.0f);
	fight_transcript::Note(first, tooFar ? "view_refused" : "view_on", [&] {
		return fight_transcript::Fields {{"how", how},
		                                 {"why", tooFar ? "too_far" : "shown"},
		                                 {"players_creature", involved},
		                                 {"script_controlled", scripted},
		                                 {"eye_x", eye.x},
		                                 {"eye_y", eye.y},
		                                 {"eye_z", eye.z},
		                                 {"focus_x", looking.x},
		                                 {"focus_y", looking.y},
		                                 {"focus_z", looking.z}};
	});
	if (tooFar)
	{
		// For a fight of the player's creature, the advisors can say one of their lines about the fight instead
		if (involved && Locator::guidanceSystem::has_value())
		{
			Locator::guidanceSystem::value().RemarkIfNow(guidance::Kind::CreatureFight);
		}
		return;
	}
	const auto ground = GroundAt(arena.centre);
	const auto normal = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetNormalAt(arena.centre)
	                                                        : glm::vec3(0.0f, 1.0f, 0.0f);
	const auto start = fight_view::Start(arena.centre, arena.radius, ground, GroundAt, normal);
	auto model = std::make_unique<FightCameraModel>(nullptr, start);
	auto* view = model.get();
	// The fight view is a part of the player's camera, so the count of the camera's time continues
	const auto modelSeconds = camera.GetModelSeconds();
	view->SetPlayerModel(camera.SetModel(std::move(model)));
	camera.SetModelSeconds(modelSeconds);
	_view = Watched {.first = first, .second = second, .lingerSeconds = std::nullopt};
	_lookSeconds = 0.0f;
}

bool CreatureFightSystem::DraggedAway(const FightCameraModel& model) const
{
	if (!_view.has_value() || !model.LandGripped())
	{
		return false;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* fighting = registry.TryGet<const CreatureFighting>(_view->first);
	if (fighting == nullptr)
	{
		return false;
	}
	// The camera must look at the land in the middle of the screen
	const auto& camera = world_camera::Get();
	const auto looking = camera.RaycastScreenCoordToLand({0.5f, 0.5f}, true, Camera::Interpolation::Drawn);
	if (!looking.has_value())
	{
		return false;
	}
	const auto eye = camera.GetOrigin(Camera::Interpolation::Target);
	return fight_view::DraggedAway(model.GetSecondsSinceFlight(), model.LandGripped(), {eye.x, eye.z},
	                               {looking->position.x, looking->position.z}, fighting->arena.centre, fighting->arena.radius);
}

void CreatureFightSystem::EndView(std::string_view why)
{
	if (_view.has_value())
	{
		fight_transcript::Note(_view->first, "view_off", [why] { return fight_transcript::Fields {{"why", why}}; });
	}
	_view.reset();
	if (!Locator::camera::has_value())
	{
		return;
	}
	auto& camera = world_camera::Get();
	if (auto* model = dynamic_cast<FightCameraModel*>(&camera.GetModel()))
	{
		if (auto player = model->TakePlayerModel())
		{
			const auto modelSeconds = camera.GetModelSeconds();
			camera.SetModel(std::move(player));
			camera.SetModelSeconds(modelSeconds);
		}
		return;
	}
	// A view that waits under the camera of the hand stops there. The hand then gives back the player's own camera
	if (auto* held = dynamic_cast<CreatureInteractCameraModel*>(&camera.GetModel()))
	{
		if (auto* waiting = dynamic_cast<FightCameraModel*>(held->GetPlayerModel()))
		{
			held->SetPlayerModel(waiting->TakePlayerModel());
		}
	}
}

void CreatureFightSystem::ComeOutOfTemple(bool placePicked)
{
	if (!_view.has_value() || !Locator::camera::has_value())
	{
		return;
	}
	auto* model = dynamic_cast<FightCameraModel*>(&Locator::camera::value().GetModel());
	if (model == nullptr)
	{
		return;
	}
	// The view stays only when no place was picked on the map, and the fight continues in its arena
	const auto& registry = Locator::entitiesRegistry::value();
	if (!placePicked && !_view->lingerSeconds.has_value() && ArenaOn(registry, _view->first) &&
	    ArenaOn(registry, _view->second))
	{
		model->StartAgain();
		return;
	}
	EndView(placePicked ? "temple_map_pick" : "temple_fight_over");
}

void CreatureFightSystem::NoteLook()
{
	if (!Locator::camera::has_value())
	{
		_look.reset();
		return;
	}
	const auto& camera = world_camera::Get();
	_look = Look {
	    .eye = camera.GetOrigin(Camera::Interpolation::Target),
	    .looking = camera.GetFocus(Camera::Interpolation::Target),
	    .centreShowsLand = ScreenCentreShowsLand(),
	};
}

bool CreatureFightSystem::ScreenCentreShowsLand()
{
	// The line starts from the view on the screen. In the temple, this is the view of the temple, not the view of the
	// camera of the world behind it.
	const auto& shown = Locator::camera::value();
	return shown.RaycastScreenCoordToLand({0.5f, 0.5f}, true, Camera::Interpolation::Drawn).has_value();
}

std::pair<glm::vec3, glm::vec3> CreatureFightSystem::ViewNow()
{
	if (!Locator::camera::has_value())
	{
		return {glm::vec3(0.0f), _screenCentre.value_or(glm::vec3(0.0f))};
	}
	const auto& camera = world_camera::Get();
	if (const auto hit = camera.RaycastScreenCoordToLand({0.5f, 0.5f}, true, Camera::Interpolation::Drawn); hit.has_value())
	{
		_screenCentre = hit->position;
	}
	return {camera.GetOrigin(Camera::Interpolation::Drawn),
	        _screenCentre.value_or(camera.GetFocus(Camera::Interpolation::Drawn))};
}

void CreatureFightSystem::FlewTo(const glm::vec3& place)
{
	// The test of distance uses the camera as it is before the flight moves it
	const auto [eye, focus] = ViewNow();
	_flightPlace = Flight {.place = glm::vec2(place.x, place.z), .eye = eye, .focus = focus};
}

bool CreatureFightSystem::IsCameraOnFightInArena() const
{
	const auto& registry = Locator::entitiesRegistry::value();
	return _view.has_value() && !_view->lingerSeconds.has_value() && registry.Valid(_view->first) &&
	       registry.Valid(_view->second);
}

std::optional<creature_fight_hud::Values> CreatureFightSystem::GetPanel() const
{
	// Shown while the camera watches a fight, the creature that made the arena first
	if (!IsCameraOnFightInArena())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto shown = std::optional(_view->first);
	const auto opponent = _view->second;
	creature_fight_hud::Values values;
	for (size_t i = 0; i < values.sides.size(); ++i)
	{
		const auto entity = i == 0 ? *shown : opponent;
		auto& side = values.sides.at(i);
		side.name = registry.Get<const Creature>(entity).name;
		if (const auto* fighting = registry.TryGet<const CreatureFighting>(entity))
		{
			side.health = fighting->fighter.health;
			side.stamina = fighting->fighter.stamina;
		}
		else
		{
			side.health = 0.0f;
			side.stamina = 0.0f;
		}
	}
	return values;
}

bool CreatureFightSystem::FightMusicHeard(const glm::vec3& camera) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto creature = PrimaryCreatureOf(registry, OwnPlayer());
	if (!creature.has_value())
	{
		return false;
	}
	std::vector<glm::vec3> arenas;
	registry.Each<const CreatureArena, const Transform>(
	    [&arenas, own = *creature](const CreatureArena& arena, const Transform& transform) {
		    if (arena.fightOn && (arena.first == own || arena.second == own))
		    {
			    arenas.push_back(transform.position);
		    }
	    });
	return fight::FightMusicHeard(fight::DirectsFightForMusic(DirectsFight(), _bodyPressHeld), arenas, camera);
}
