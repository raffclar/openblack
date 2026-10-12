/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureCaveSystem.h"

#include <cmath>

#include <algorithm>
#include <span>
#include <vector>

#include <glm/gtx/euler_angles.hpp>

#include "3D/CreatureBody.h"
#include "3D/CreatureCaveTrophies.h"
#include "3D/L3DMesh.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Creature/CaveCreature.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureWatching.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/PosedModel.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"

namespace openblack::ecs::systems
{

using namespace components;

namespace
{
/// Whether the game has a text for a creature having learnt a miracle, by the miracle's place in its table; the creature
/// room's scroll and plinths leave out those it has none for
/// The number of the game's help text for a creature having learnt a miracle, 0 for none
uint32_t LearntText(size_t miracle)
{
	if (!Locator::infoConstants::has_value())
	{
		return 0;
	}
	const auto& info = Locator::infoConstants::value();
	return miracle < info.magicEffect.size()
	           ? static_cast<uint32_t>(magic::GetMagicEffectInfo(info, static_cast<MagicType>(miracle)).creatureLearntEnum)
	           : 0;
}

/// Another creature's own name, as its scroll shows it; nothing for one without a name or that is gone
std::u16string NameOf(const Registry& registry, entt::entity creature)
{
	const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	return body != nullptr ? body->name : std::u16string();
}

/// The places the temple's walk mesh marks in the creature room: the seat by the fire, the fire, and a place further back
std::optional<cave_creature::Places> CavePlaces()
{
	const entt::id_type movement = entt::hashed_string("temple/interior/movement_l3d").value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(movement))
	{
		return std::nullopt;
	}
	const auto& marks = meshes.Handle(movement)->GetExtraMetrics();
	if (marks.size() < 3)
	{
		return std::nullopt;
	}
	return cave_creature::Places {
	    .seat = glm::vec3(marks[0][3]), .fire = glm::vec3(marks[1][3]), .back = glm::vec3(marks[2][3])};
}
} // namespace

bool CreatureCaveSystem::InTemple() const
{
	return Locator::temple::has_value();
}

void CreatureCaveSystem::Update(std::chrono::duration<float, std::milli> frameTime)
{
	if (InTemple())
	{
		// The temple's creature room is the cave: F5 takes the player there, and Escape back out
		const auto& temple = Locator::temple::value();
		_screen.open = temple.Active() && temple.GetCurrentRoom() == TempleRoom::CreatureCave;
		KeepCaveCreature();
		UpdateCaveCreature(frameTime.count());
		return;
	}
	DropCaveCreature();
	// Without a temple to go into, F5 shows the cave on its own and hides it again
	if (Locator::gameActionSystem::has_value())
	{
		using input::BindableActionMap;
		const auto& actions = Locator::gameActionSystem::value();
		if (actions.Get(BindableActionMap::ZOOM_TO_CREATURE_ROOM) &&
		    actions.GetChanged(BindableActionMap::ZOOM_TO_CREATURE_ROOM))
		{
			if (_screen.open)
			{
				Close();
			}
			else
			{
				Open();
			}
		}
	}
}

void CreatureCaveSystem::Open()
{
	if (InTemple())
	{
		auto& temple = Locator::temple::value();
		if (temple.Active())
		{
			temple.GoToRoom(TempleRoom::CreatureCave);
		}
		else
		{
			temple.Activate(TempleRoom::CreatureCave);
		}
		return;
	}
	// Going into the cave lets go of the creature the camera followed
	if (Locator::creatureModeSystem::has_value())
	{
		Locator::creatureModeSystem::value().Leave();
	}
	_screen.open = true;
}

void CreatureCaveSystem::Close()
{
	if (InTemple())
	{
		if (Locator::temple::value().Active())
		{
			Locator::temple::value().RequestLeave();
		}
		return;
	}
	_screen.open = false;
}

bool CreatureCaveSystem::Escape()
{
	// In the temple, its own Escape leads the way out
	if (InTemple() || !_screen.open)
	{
		return false;
	}
	Close();
	return true;
}

std::optional<entt::entity> CreatureCaveSystem::GetCreature() const
{
	// The cave is the player's own creature's, whichever creature the camera follows
	return Locator::creatureModeSystem::has_value() ? Locator::creatureModeSystem::value().PlayersCreature() : std::nullopt;
}

std::optional<creature_cave::Snapshot> CreatureCaveSystem::Snapshot() const
{
	const auto creature = GetCreature();
	if (!creature.has_value() || !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(*creature);
	if (body == nullptr)
	{
		return std::nullopt;
	}
	creature_cave::Snapshot snapshot;
	snapshot.name = body->name;
	snapshot.strength = body->strength;
	snapshot.fatness = body->fatness;
	snapshot.alignment = body->alignment;
	if (const auto* needs = registry.TryGet<const CreatureNeeds>(*creature))
	{
		snapshot.needs = needs->needs;
	}
	if (const auto* record = registry.TryGet<const CreatureFightRecord>(*creature))
	{
		snapshot.fightBalance = record->tendency;
	}
	const auto* mind = registry.TryGet<const CreatureMindState>(*creature);
	if (mind == nullptr)
	{
		return snapshot;
	}
	snapshot.attitudeToPlayer = mind->attitudeToPlayer;
	// Whether it thinks it knows what its god wants most
	// TODO(raffclar): the game forgets what it found each time it writes the scroll's text; this is looked at every frame,
	// so it only looks
	auto perceived = mind->perceivedDesires;
	snapshot.knowsGodsDesire = creature_perceived_desires::TakeDominant(perceived, [mind](size_t desire) {
		                           return mind->desires.has_value() && desire < mind->desires->desires.size() &&
		                                  mind->desires->desires.at(desire).activated;
	                           }).has_value();
	snapshot.turnsLookedAt = mind->turnsLookedAt;
	const auto* tables = Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().GetTables() : nullptr;
	if (mind->learnt.has_value())
	{
		const auto& learnt = *mind->learnt;
		for (const auto& attitude : learnt.creatures)
		{
			snapshot.creaturesKnown.push_back({.name = NameOf(registry, static_cast<entt::entity>(attitude.creature)),
			                                   .howNice = attitude.howNice,
			                                   .attention = attitude.attention});
		}
		if (mind->desires.has_value())
		{
			const auto& desires = mind->desires->desires;
			snapshot.anger = desires.at(static_cast<size_t>(creature_desires::Desire::Anger)).value;
			snapshot.compassion = desires.at(static_cast<size_t>(creature_desires::Desire::Compassion)).value;
			for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
			{
				const auto& state = mind->desires->desires.at(d);
				if (state.activated)
				{
					const auto desire = static_cast<creature_desires::Desire>(d);
					snapshot.likes.push_back(
					    {.desire = desire,
					     .level = creature_learning::LikesLevel(desire, state, learnt.initialThresholds.at(d))});
				}
			}
		}
		for (size_t a = 0; a < learnt.opinions.size(); ++a)
		{
			const auto name = tables != nullptr && a < tables->actions.size() ? tables->actions[a].name : std::to_string(a);
			snapshot.opinions.push_back({.action = name, .opinion = learnt.opinions[a]});
		}
		if (tables != nullptr)
		{
			const auto& knowledge = learnt.knowledge;
			for (size_t i = 0; i < tables->skills.size(); ++i)
			{
				const bool known = i < knowledge.skillsKnown.size() && knowledge.skillsKnown[i];
				snapshot.skills.push_back({.name = tables->skills[i].name, .known = known});
			}
			const auto multiplier = creature_mind_tables::MiracleMultiplier(creature::InfoRow(body->species));
			for (size_t i = 0; i < tables->miracles.size(); ++i)
			{
				const auto& rule = tables->miracles[i];
				// How far it has learnt a miracle is the times it has seen it over the times it must, whether it
				// has learnt it or not
				const auto needed = creature_watching::TimesNeeded(rule.timesToSee, multiplier);
				const auto seen = i < knowledge.miraclesSeen.size() ? knowledge.miraclesSeen[i].count : 0u;
				snapshot.miracles.push_back({
				    .name = rule.name,
				    .knownAbout = i < knowledge.miraclesKnown.size() && knowledge.miraclesKnown[i],
				    .learntText = LearntText(i),
				    .learnt = CreatureCaveTrophies::PercentLearnt(seen, needed),
				});
			}
		}
	}
	return snapshot;
}

bool CreatureCaveSystem::ApplyTattoo(uint8_t site, uint8_t design, glm::u8vec3 colour)
{
	const auto creature = GetCreature();
	if (!creature.has_value() || !Locator::creatureSkinSystem::has_value())
	{
		return false;
	}
	const auto* tattoos = Locator::entitiesRegistry::value().TryGet<const CreatureTattoos>(*creature);
	if (tattoos == nullptr)
	{
		return false;
	}
	const auto edit = creature_cave::Apply(tattoos->slots, site, design, colour);
	if (!edit.has_value())
	{
		return false;
	}
	Locator::creatureSkinSystem::value().SetTattoo(*creature, edit->slot, edit->tattoo);
	return true;
}

bool CreatureCaveSystem::RemoveTattoo(uint8_t site)
{
	const auto creature = GetCreature();
	if (!creature.has_value() || !Locator::creatureSkinSystem::has_value())
	{
		return false;
	}
	const auto* tattoos = Locator::entitiesRegistry::value().TryGet<const CreatureTattoos>(*creature);
	if (tattoos == nullptr)
	{
		return false;
	}
	const auto edit = creature_cave::Remove(tattoos->slots, site);
	if (!edit.has_value())
	{
		return false;
	}
	Locator::creatureSkinSystem::value().SetTattoo(*creature, edit->slot, edit->tattoo);
	return true;
}

void CreatureCaveSystem::KeepCaveCreature()
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::temple::value().Active())
	{
		DropCaveCreature();
		_caveMade = false;
		return;
	}
	if (_caveCreature.has_value())
	{
		// The copy goes when the creature does
		const auto* copy = registry.TryGet<const CreatureCopy>(*_caveCreature);
		if (copy == nullptr || !registry.Valid(copy->creature))
		{
			DropCaveCreature();
		}
		return;
	}
	// The room is made once a visit, as it is first drawn: with the creature by the fire if the player has one then and
	// the scripts haven't said not to show it
	if (_caveMade || !Locator::temple::value().IsRoomDrawn(TempleRoom::CreatureCave))
	{
		return;
	}
	_caveMade = true;
	if (const auto creature = GetCreature(); creature.has_value() && _creatureShown)
	{
		MakeCaveCreature(*creature);
	}
}

void CreatureCaveSystem::MakeCaveCreature(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto places = CavePlaces();
	const auto* body = registry.TryGet<const Creature>(creature);
	if (!places.has_value() || body == nullptr || !registry.AllOf<Mesh, CreatureMorph>(creature))
	{
		return;
	}
	const auto size = cave_creature::SizeOf(body->size);
	const auto coin = std::uniform_int_distribution<uint32_t>(0, 1)(_random) != 0;
	const auto rest = cave_creature::RestFor(size, coin);
	const auto position = cave_creature::StandsAt(*places, size, rest);
	// It is put down facing the fire
	const auto heading = cave_creature::HeadingTowards(position, places->fire);

	const auto copy = registry.Create();
	registry.Assign<Transform>(copy, position, glm::mat3(glm::eulerAngleY(heading)),
	                           glm::vec3(archetypes::CreatureArchetype::DrawnScale(body->species, size)));
	registry.Assign<CreatureCopy>(copy, creature, size);
	// Its fidgets in the tattoo editor start 20 to 40 seconds away
	cave_creature::State state {.rest = rest, .heading = heading};
	cave_creature::StartIdle(state, [this](uint32_t range) {
		return range == 0 ? 0u : std::uniform_int_distribution<uint32_t>(0, range - 1)(_random);
	});
	registry.Assign<CaveCreature>(copy, state, places->fire);
	registry.Assign<CreatureAnimation>(copy);
	registry.Assign<CreatureEyes>(copy);
	_caveCreature = copy;
}

void CreatureCaveSystem::DropCaveCreature()
{
	if (_caveCreature.has_value() && Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		if (registry.Valid(*_caveCreature))
		{
			registry.Destroy(*_caveCreature);
		}
	}
	_caveCreature.reset();
}

void CreatureCaveSystem::UpdateCaveCreature(float milliseconds)
{
	if (!_caveCreature.has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto* copy = registry.TryGet<CreatureCopy>(*_caveCreature);
	auto* cave = registry.TryGet<CaveCreature>(*_caveCreature);
	auto* animation = registry.TryGet<CreatureAnimation>(*_caveCreature);
	auto* transform = registry.TryGet<Transform>(*_caveCreature);
	if (copy == nullptr || cave == nullptr || animation == nullptr || transform == nullptr)
	{
		return;
	}
	const auto frameMs = std::min(milliseconds, cave_creature::k_MaxFrameMs);
	const cave_creature::Random random = [this](uint32_t range) {
		return range == 0 ? 0u : std::uniform_int_distribution<uint32_t>(0, range - 1)(_random);
	};

	// The camera is zoomed onto it while the tattoo editor is open on its creature, looking where the editor turns it
	cave_creature::Frame frame {.fire = cave->light, .position = transform->position, .milliseconds = frameMs};
	if (Locator::tattooEditorSystem::has_value())
	{
		const auto& editor = Locator::tattooEditorSystem::value();
		frame.zoomed = editor.IsOpen() && editor.GetCreature() == std::optional(copy->creature);
		frame.eye = editor.GetView().eye;
		frame.focus = editor.GetView().focus;
	}
	if (Locator::gameActionSystem::has_value())
	{
		using input::BindableActionMap;
		const auto& actions = Locator::gameActionSystem::value();
		frame.buttonHeld = actions.GetAny(BindableActionMap::MOVE, BindableActionMap::ACTION);
		frame.leftHeld = actions.Get(BindableActionMap::MOVE);
		if (Locator::camera::has_value() && Locator::windowing::has_value())
		{
			// The point under the pointer a little way into the screen
			const auto size = glm::vec2(Locator::windowing::value().GetSize());
			const auto& camera = Locator::camera::value();
			if (size.x > 0.0f && size.y > 0.0f && camera.GetNearClip() > 0.0f)
			{
				const auto [eye, near] = camera.OnNearPlane(glm::vec2(actions.GetMousePosition()) / size);
				frame.pointer = eye + ((near - eye) * (cave_creature::k_PointerDepth / camera.GetNearClip()));
			}
		}
	}

	cave_creature::Body body {.body = animation->body, .gesture = animation->gesture, .lookAt = animation->lookAt};
	auto& state = cave->state;
	if (const auto turnTo = cave_creature::Step(state, body, frame, random); turnTo.has_value())
	{
		StartCaveTurn(*copy, state, *turnTo);
	}
	cave_creature::AdvanceTurn(state, frameMs * creature_layers::PlaybackRate(copy->size));
	animation->body = body.body;
	animation->gesture = body.gesture;
	animation->lookAt = body.lookAt;
	// While it turns its spin's own root turns the body; it faces the new way once the turn is over
	animation->slots.clear();
	if (state.turn.has_value())
	{
		const auto& turn = *state.turn;
		animation->slots.push_back({.animation = turn.spin,
		                            .timeMs = std::clamp(turn.timeMs, 0.0f, std::max(turn.durationMs - 1.0f, 0.0f)),
		                            .weight = 1.0f,
		                            .mirrored = false,
		                            .blendedWith = turn.blendedWith,
		                            .blendWeight = turn.blendWeight});
	}
	transform->rotation = glm::mat3(glm::eulerAngleY(state.heading));
	if (Locator::creatureAnimationSystem::has_value())
	{
		Locator::creatureAnimationSystem::value().PoseCopy(*_caveCreature, frameMs);
	}
}

void CreatureCaveSystem::StartCaveTurn(const CreatureCopy& copy, cave_creature::State& state, float heading)
{
	// It turns on the spot by two of its 0, 90 and 180 degree spins to the side it turns to, blended for the angle
	namespace locomotion = creature_locomotion;
	const auto angle = locomotion::WrapAngle(heading - state.heading);
	const auto first = locomotion::SideOf(angle) == locomotion::Side::Right ? locomotion::animations::k_RightSpin
	                                                                        : locomotion::animations::k_LeftSpin;
	const auto pair = locomotion::PairFor(first, std::abs(angle));
	std::optional<float> from;
	std::optional<float> to;
	if (Locator::creatureAnimationSystem::has_value())
	{
		auto& animations = Locator::creatureAnimationSystem::value();
		from = animations.AnimationDuration(copy.creature, pair.from);
		to = animations.AnimationDuration(copy.creature, pair.to);
	}
	if (!from.has_value() || !to.has_value())
	{
		// TODO(raffclar): a species without its spins; the game plays no turn then, and how it comes round is not known.
		//                 It faces the new way at once.
		state.heading = heading;
		return;
	}
	// The blend lasts between the two spins' lengths, to the nearest millisecond
	state.turn = cave_creature::State::Turn {.from = state.heading,
	                                         .to = heading,
	                                         .timeMs = 0.0f,
	                                         .durationMs = std::nearbyint(*from + ((*to - *from) * pair.weight)),
	                                         .spin = pair.from,
	                                         .blendedWith = pair.to,
	                                         .blendWeight = pair.weight};
}

std::optional<creature_tattoo_editor::Orbit> CreatureCaveSystem::GetCaveCreatureView() const
{
	if (!_caveCreature.has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* copy = registry.TryGet<const CreatureCopy>(*_caveCreature);
	const auto* transform = registry.TryGet<const Transform>(*_caveCreature);
	if (copy == nullptr || transform == nullptr)
	{
		return std::nullopt;
	}
	return creature_tattoo_editor::CaveView(transform->position, copy->size);
}

bool CreatureCaveSystem::CursorOverCaveCreature(const screen_pick::View& view) const
{
	if (!_caveCreature.has_value())
	{
		return false;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* copy = registry.TryGet<const CreatureCopy>(*_caveCreature);
	const auto* animation = registry.TryGet<const CreatureAnimation>(*_caveCreature);
	const auto* transform = registry.TryGet<const Transform>(*_caveCreature);
	if (copy == nullptr || animation == nullptr || transform == nullptr || !registry.Valid(copy->creature))
	{
		return false;
	}
	const auto* meshComponent = registry.TryGet<const Mesh>(copy->creature);
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (meshComponent == nullptr || !meshes.Contains(meshComponent->id))
	{
		return false;
	}
	const auto mesh = meshes.Handle(meshComponent->id);
	const auto model = creature::PlacementMatrix(transform->position, transform->rotation, transform->scale);
	// As the game finds what is under the cursor: by the body's bounding sphere, then its triangles as they are drawn
	const auto box = mesh->GetBoundingBox();
	const float scale = glm::length(glm::vec3(model[0]));
	if (!screen_pick::CursorOverSphere(view, glm::vec3(model * glm::vec4(box.Center(), 1.0f)),
	                                   glm::length(box.Size() * 0.5f) * scale, glm::vec3(model[3])))
	{
		return false;
	}
	const auto& rest = mesh->GetBoneMatrices();
	const auto bones = animation->boneMatrices.size() == rest.size() ? std::span<const glm::mat4>(animation->boneMatrices)
	                                                                 : std::span<const glm::mat4>(rest);
	std::vector<glm::vec3> placed;
	std::vector<screen_pick::ClipCorner> corners;
	for (size_t s = 0; s < mesh->GetSubMeshes().size(); ++s)
	{
		const auto& subMesh = *mesh->GetSubMeshes()[s];
		if (!posed_model::IsDrawn(subMesh))
		{
			continue;
		}
		posed_model::Place(registry, copy->creature, *mesh, s, model, bones, placed);
		const auto& geometry = subMesh.GetBodyGeometry();
		corners.clear();
		for (size_t c = 0; c < placed.size(); ++c)
		{
			corners.push_back(
			    screen_pick::ToClip(view, placed[c], c < geometry.uvs.size() ? geometry.uvs[c] : glm::vec2(0.0f)));
		}
		const std::span<const uint16_t> indices = geometry.indices;
		for (const auto& primitive : subMesh.GetPrimitives())
		{
			if (primitive.indicesOffset + primitive.indicesCount > indices.size())
			{
				continue;
			}
			const screen_pick::Primitive triangles {.corners = corners,
			                                        .indices = indices.subspan(primitive.indicesOffset, primitive.indicesCount),
			                                        .twoSided = primitive.twoSided};
			if (screen_pick::FirstHit(view, triangles).has_value())
			{
				return true;
			}
		}
	}
	return false;
}

} // namespace openblack::ecs::systems
