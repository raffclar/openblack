/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TemplePeople.h"

#include <array>

#include <entt/core/hashed_string.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "Animals/AnimalAnimation.h"
#include "Common/GameRandom.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// The floors the worshippers walk, laid over the temple's rooms
constexpr auto k_WalkMeshId = entt::hashed_string("temple/interior/movement_l3d");
/// The player whose temple the player is in
constexpr auto k_Player = PlayerNames::PLAYER_ONE;

/// The game's own random numbers, from this machine's seed
class GameDice final: public temple_people::Dice
{
public:
	explicit GameDice(GameRandomInterface& random)
	    : _random(random)
	{
	}
	uint32_t Below(int32_t n) override { return _random.LocalRand(n); }
	float UpTo(float x) override { return _random.LocalFloatRand(x); }

private:
	GameRandomInterface& _random;
};

std::optional<temple_people::WalkMesh> LoadWalkMesh()
{
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(k_WalkMeshId.value()))
	{
		return std::nullopt;
	}
	const auto mesh = meshes.Handle(k_WalkMeshId.value());
	if (mesh->GetSubMeshes().empty())
	{
		return std::nullopt;
	}
	// The mesh's first submesh holds the floors, a triangle for every three of its indices
	const auto& geometry = mesh->GetSubMeshes().front()->GetBodyGeometry();
	std::vector<std::array<uint16_t, 3>> triangles;
	triangles.reserve(geometry.indices.size() / 3);
	for (size_t i = 0; i + 2 < geometry.indices.size(); i += 3)
	{
		triangles.push_back({geometry.indices[i], geometry.indices[i + 1], geometry.indices[i + 2]});
	}
	if (triangles.empty())
	{
		return std::nullopt;
	}
	return temple_people::BuildWalkMesh(geometry.positions, triangles);
}

/// The worshippers of each of the player's worship sites, by the site's place around the temple
std::vector<std::vector<entt::entity>> PlayersWorshippers(const ecs::Registry& registry)
{
	std::vector<std::vector<entt::entity>> bySite;
	registry.Each<const Temple, const CitadelWorship>([&](const Temple& temple, const CitadelWorship& worship) {
		if (temple.owner != k_Player || !bySite.empty())
		{
			return;
		}
		for (const auto site : worship.sites)
		{
			if (const auto* worshipSite = site != entt::null ? registry.TryGet<const WorshipSite>(site) : nullptr)
			{
				bySite.push_back(worshipSite->worshippers);
			}
		}
	});
	return bySite;
}
} // namespace

std::unique_ptr<TemplePeople> TemplePeople::Gather()
{
	if (!Locator::resources::has_value() || !Locator::gameRandom::has_value() || !Locator::livingActionSystem::has_value())
	{
		return nullptr;
	}
	auto walkMesh = LoadWalkMesh();
	if (!walkMesh.has_value())
	{
		return nullptr;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& livingActions = Locator::livingActionSystem::value();
	// A villager comes in unless it is gone or dying
	const auto available = [&registry, &livingActions](entt::entity villager) {
		if (!registry.Valid(villager) || !registry.AllOf<Villager, LivingAction, Mesh, Transform>(villager))
		{
			return false;
		}
		return livingActions.VillagerGetFinalState(registry.Get<const LivingAction>(villager)) != VillagerStates::Dying;
	};
	const auto worshippers = PlayersWorshippers(registry);
	const auto visitors = temple_people::ChooseVisitors(worshippers, available);

	auto people = std::make_unique<TemplePeople>();
	people->_mesh = std::move(*walkMesh);
	GameDice dice(Locator::gameRandom::value());
	for (const auto villager : visitors)
	{
		// Each comes in as the villager looks: its model, its size and its sex
		people->_visitors.push_back({
		    .villager = villager,
		    .mesh = registry.Get<const Mesh>(villager).id,
		    .scale = registry.Get<const Transform>(villager).scale.x,
		    .man = registry.Get<const Villager>(villager).sex == Villager::Sex::MALE,
		    .walker = temple_people::Arrive(people->_mesh, dice),
		});
	}
	return people;
}

void TemplePeople::Update(float milliseconds, const glm::mat4& temple, const glm::mat4& viewProjection, float nearClip)
{
	_drawn.clear();
	if (_visitors.empty())
	{
		return;
	}
	GameDice dice(Locator::gameRandom::value());
	const float seconds = milliseconds * 0.001f;
	for (auto& visitor : _visitors)
	{
		temple_people::Step(visitor.walker, _mesh, seconds, dice);
	}

	const auto& animations = Locator::resources::value().GetAnimations();
	auto& meshes = Locator::resources::value().GetMeshes();
	const auto played = static_cast<int32_t>(milliseconds);
	for (auto& visitor : _visitors)
	{
		auto& walker = visitor.walker;
		const auto feet = temple * glm::vec4(walker.position, 1.0f);
		if (!temple_people::Drawn(viewProjection * feet, nearClip) || !meshes.Contains(visitor.mesh))
		{
			continue;
		}
		const auto model = glm::translate(glm::mat4(1.0f), glm::vec3(feet)) * glm::mat4(glm::mat3(temple)) *
		                   glm::eulerAngleY(-walker.facing) * glm::scale(glm::mat4(1.0f), glm::vec3(visitor.scale));
		TemplePerson person {.mesh = visitor.mesh, .model = model, .bones = {}};
		// While seen, its clip plays on and sets the speed it walks at
		const auto clipId = resources::HashIdentifier(static_cast<uint32_t>(temple_people::ClipOf(walker.gait, visitor.man)));
		if (animations.Contains(clipId))
		{
			const auto clip = animations.Handle(clipId);
			const animals::ClipTiming timing {
			    .playTime = clip->GetPlayTime(), .frameCount = clip->GetFrames().size(), .looping = clip->IsLooping()};
			walker.speed = temple_people::SpeedOf(clip->GetStride(), timing.playTime, visitor.scale);
			walker.clipPlace = animals::AdvanceClip(timing, walker.clipPlace, played);

			const auto body = meshes.Handle(visitor.mesh);
			const auto& frames = clip->GetFrames();
			const auto& parents = body->GetBoneParents();
			const auto span = animals::SpanAt(timing, walker.clipPlace);
			if (body->IsBoned() && !frames.empty() && frames[span.from].bones.size() >= parents.size() &&
			    frames[span.to].bones.size() >= parents.size())
			{
				person.bones.resize(parents.size());
				animals::PoseBetween(frames[span.from].bones, frames[span.to].bones, span.t, parents, person.bones);
			}
		}
		_drawn.push_back(std::move(person));
	}
}
