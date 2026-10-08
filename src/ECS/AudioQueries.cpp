/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AudioQueries.h"

#include <cstdio>
#include <cstdlib>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/Clouds.h"
#include "3D/L3DAnim.h"
#include "3D/MapCoords.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/GameQueries.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Common/GUtilsDistance.h"
#include "Debug/DebugEnv.h"
#include "ECS/Animations.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Influence/Influence.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Weather/Atmos.h"
#include "Enums.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Resources/ResourcesInterface.h"
#include "Video/VideoPlayer.h"
#include "Worship/Citadel.h"
#include "Worship/WorshipSite.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// What the audio test hooks keep between calls, in the debug hooks' store (Locator::debugHooks)
struct AudioQueriesDebugHooksState
{
	uint32_t lanternHookTurn {0}; // OPENBLACK_AUDIO_TEST_LANTERN: hook turns so far
	uint32_t citadelHookTurn {0}; // OPENBLACK_AUDIO_TEST_CITADEL: hook turns so far, the world's and the temple's
};

AudioQueriesDebugHooksState& AudioQueriesDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("ecs::audio_queries: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<AudioQueriesDebugHooksState>();
}

std::optional<audio::AnimatedThing> AnimatedThing(entt::entity entity)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::IsAvailable(entity))
	{
		return std::nullopt;
	}
	// the thing's 3D sound position (its position), else nothing
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	audio::AnimatedThing thing;
	thing.position = transform->position;
	if (const auto* action = registry.TryGet<const LivingAction>(entity); action != nullptr)
	{
		thing.turnsSinceStateChange = action->turnsSinceStateChange;
	}
	if (const auto* villager = registry.TryGet<const Villager>(entity); villager != nullptr)
	{
		audio::AnimatedThing::Villager v {
		    .alive = villager->life > 0.0f, // alive: life above 0
		    .child = villager->lifeStage == Villager::LifeStage::Child,
		    .female = villager->sex == Villager::Sex::FEMALE,
		};
		if (villager->abode != entt::null && ecs::IsAvailable(villager->abode))
		{
			v.abode = villager->abode; // the villager's abode
		}
		thing.villager = v;
	}
	return thing;
}

std::optional<std::string> AnimationClipName(int32_t index)
{
	if (!Locator::resources::has_value() || index < 0)
	{
		return std::nullopt;
	}
	auto& animations = Locator::resources::value().GetAnimations();
	const auto id = ecs::ClipId(static_cast<uint32_t>(index));
	if (!animations.Contains(id))
	{
		return std::nullopt;
	}
	// the header's name is 32 chars padded with zeros
	return std::string(animations.Handle(id)->GetName().c_str());
}

std::vector<audio::StreetLantern> StreetLanterns()
{
	std::vector<audio::StreetLantern> lanterns;
	if (!Locator::entitiesRegistry::has_value())
	{
		return lanterns;
	}
	Locator::entitiesRegistry::value().Each<const StreetLantern, const Transform>(
	    [&lanterns](entt::entity entity, const StreetLantern& /*unused*/, const Transform& transform) {
		    // the lantern's height
		    lanterns.push_back({entity, transform.position, ecs::object::GetHeight(entity)});
	    });
	return lanterns;
}

audio::CameraWeatherInfo WeatherSmooth(glm::vec3 point)
{
	// the weather at the point, recalculated (as the camera does each update)
	const auto smooth = weather::atmos::GetWeatherSmooth(point, true);
	audio::CameraWeatherInfo info {
	    .temperature = smooth.temperature,
	    .rain = smooth.rain,
	    .snow = smooth.snow,
	    .overcast = smooth.overcast,
	    .windX = smooth.windX,
	    .windZ = smooth.windZ,
	};
	return info;
}

float CameraAlignment()
{
	// x: once a turn the players' update passes clamp((the alignment of the most influential player at the
	// interface's camera position + 1) / 2, 0, 1), ecs::effects::alignment::GetInterfaceAlignment(). The sky takes the
	// same x (it stores 2 (1 - x)), so the sky's openblack overrides (OPENBLACK_TEST_SKY_ALIGNMENT, the debug slider)
	// reach the audio too: Clouds::InfluentialPlayerAlignment is 2x - 1, or the override's -1..1.
	float x = ecs::effects::alignment::GetInterfaceAlignment();
	if (const float sky = Clouds::InfluentialPlayerAlignment(); sky != x * 2.0f - 1.0f)
	{
		x = (sky + 1.0f) * 0.5f; // (openblack) an override is on
	}
	return ecs::audio_queries::AudioAlignmentValue(x);
}

/// The camera's MapCoords (the point the tribe's music measures towns from): the render camera's position, x and z
/// x 6553.6 truncated, GameQueries::camera's point
std::optional<map_coords::MapCoords> CameraMapCoords()
{
	if (!Locator::camera::has_value())
	{
		return std::nullopt;
	}
	return map_coords::FromWorld(Locator::camera::value().GetOrigin());
}

/// A town as the tribe's music reads it: its Tribe (given to CREATE_TOWN, TownArchetype) and its distance in metres to
/// the camera; nullopt when the entity is no longer an available town
std::optional<audio::MusicTown> MusicTownOf(entt::entity town, const map_coords::MapCoords& camera)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (town == entt::null || !ecs::IsAvailable(town) || registry.TryGet<const Town>(town) == nullptr)
	{
		return std::nullopt;
	}
	audio::MusicTown music;
	music.id = static_cast<uint32_t>(entt::to_integral(town));
	const auto* tribe = registry.TryGet<const Tribe>(town);
	music.tribe = static_cast<int>(tribe != nullptr ? *tribe : Tribe::NONE);
	music.distance = gutils::GetDistanceInMetres(camera, ecs::object::MapCoordsOf(town));
	return music;
}

std::optional<audio::MusicTown> NearestMusicTown(float maxDistance)
{
	const auto camera = CameraMapCoords();
	if (!camera || !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	// the nearest town within townTriggerOffDistance, every player and the neutral one, strictly nearer than the best,
	// only a town with a town centre (among its abodes, or a planned one; map_cells::TownHasCentre)
	return MusicTownOf(ecs::map_cells::GetNearestTownWithCentre(*camera, maxDistance), *camera);
}

std::optional<audio::MusicTown> KeptMusicTown(uint32_t id)
{
	const auto camera = CameraMapCoords();
	if (!camera || !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	// the kept town again: still available, and its distance to the camera
	return MusicTownOf(static_cast<entt::entity>(id), *camera);
}

std::optional<audio::ThingId> NearestTownAt(glm::vec3 point, float maxDistance)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	// the nearest town to the point (as MapCoords): strictly nearer than the best, every player and the neutral one
	const auto town = ecs::map_cells::GetNearestTown(map_coords::FromWorld(point), maxDistance);
	if (town == entt::null)
	{
		return std::nullopt;
	}
	return static_cast<audio::ThingId>(entt::to_integral(town));
}

std::vector<audio::DesireTown> DesireTowns()
{
	std::vector<audio::DesireTown> towns;
	if (!Locator::entitiesRegistry::has_value())
	{
		return towns;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	// every player's town list (map_cells::ForEachTown, the neutral last)
	ecs::map_cells::ForEachTown([&registry, &towns](entt::entity entity) {
		const auto* t = registry.TryGet<const Town>(entity);
		const auto* transform = registry.TryGet<const Transform>(entity);
		if (t == nullptr || transform == nullptr)
		{
			return true;
		}
		audio::DesireTown town {
		    .id = static_cast<audio::ThingId>(entt::to_integral(entity)),
		    .position = transform->position, // the town's position
		};
		// the storage pit, if available, and its position
		if (const auto pit = ecs::town_queries::GetStoragePit(entity); pit != entt::null)
		{
			if (const auto* pitTransform = registry.TryGet<const Transform>(pit); pitTransform != nullptr)
			{
				town.storagePit = pitTransform->position;
			}
		}
		town.population = t->stats.adults + t->stats.children; // adults + children
		// the town's desires sorted by value (value, type), and each one's raw desire
		const auto& sorted = ecs::town_desire::GetSortedRawDesires(entity);
		for (size_t k = 0; k < sorted.size(); ++k)
		{
			town.desires.at(k).value = sorted.at(k).value;
			town.desires.at(k).type = sorted.at(k).index;
			town.desires.at(k).raw = ecs::town_desire::GetRawDesire(entity, static_cast<TownDesireInfo>(sorted.at(k).index));
		}
		towns.push_back(town);
		return true;
	});
	return towns;
}

std::optional<std::array<float, 3>> TownResourceNeeds(audio::ThingId id)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto town = static_cast<entt::entity>(id);
	if (!ecs::IsAvailable(town) || !Locator::entitiesRegistry::value().AnyOf<Town>(town))
	{
		return std::nullopt;
	}
	// Raw + Boost + BoostA of the desires 0 (food), 1 (wood) and 10 (rain), added in that order and stored as floats
	using ecs::town_desire::Field;
	const auto need = [town](TownDesireInfo d) {
		const float raw = ecs::town_desire::GetField(town, d, Field::Raw);
		const float rawBoost = raw + ecs::town_desire::GetField(town, d, Field::Boost);
		return rawBoost + ecs::town_desire::GetField(town, d, Field::BoostA);
	};
	return std::array<float, 3> {need(TownDesireInfo::ForFood), need(TownDesireInfo::ForWood), need(TownDesireInfo::ForRain)};
}

std::optional<audio::WorshipDesire> WorshipSites()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	// the local player's citadel (openblack's is PLAYER_ONE).
	// OPENBLACK_TEST_WORSHIP_PLAYER=<n> (test hook, not original): that player's instead
	auto player = PlayerNames::PLAYER_ONE;
	static const debug_env::Variable k_TestWorshipPlayer("OPENBLACK_TEST_WORSHIP_PLAYER");
	if (const char* test = k_TestWorshipPlayer.Get(); test != nullptr)
	{
		const int n = std::atoi(test);
		if (n >= 0 && n < static_cast<int>(PlayerNames::_COUNT))
		{
			player = static_cast<PlayerNames>(n);
		}
	}
	const auto citadel = worship::citadel::Of(player);
	const auto* citadelTransform = citadel != entt::null ? registry.TryGet<const Transform>(citadel) : nullptr;
	if (citadelTransform == nullptr)
	{
		return std::nullopt;
	}
	audio::WorshipDesire desire {
	    .citadelPosition = citadelTransform->position,          // the citadel's position
	    .need = worship::citadel::StrainSoundFraction(citadel), // the strain's fraction, capped by the caller
	};
	// the citadel's sites in slot order: their position, whether they have worshippers and their desire for food
	const auto sites = worship::citadel::WorshipSitesOf(player);
	for (size_t i = 0; i < sites.size(); ++i)
	{
		const auto site = sites.at(i);
		const auto* transform = site != entt::null ? registry.TryGet<const Transform>(site) : nullptr;
		if (transform == nullptr)
		{
			continue;
		}
		audio::WorshipDesire::Site entry {
		    .id = static_cast<audio::ThingId>(entt::to_integral(site)),
		    .position = transform->position,
		    .worshippers = worship::site::DancerCount(site) > 0,
		    .foodDesire = worship::site::CalculateDesireForFood(site),
		};
		desire.sites.at(i) = entry;
	}
	return desire;
}

/// The heart beat's values and the heart, for the local player (openblack's PLAYER_ONE)
audio::HeartBeatInput HeartBeat()
{
	audio::HeartBeatInput input;
	if (!Locator::entitiesRegistry::has_value())
	{
		return input;
	}
	constexpr auto k_Player = PlayerNames::PLAYER_ONE;
	// the player's towns: the sum of their raw protection desires
	for (const auto town : ecs::map_cells::TownsOf(k_Player))
	{
		input.protectionDesire = ecs::town_desire::GetRawDesire(town, TownDesireInfo::ForProtection) + input.protectionDesire;
	}
	// the proportion of the world's population who believe in the player, and this turn's influence power ratio
	input.believers = magic::players::BelieverFraction(k_Player);
	input.beliefShare = influence::InfluencePowerRatio(k_Player);
	// the other players' creatures near the local player's towns. Pending: creature (openblack's creatures do not
	// belong to the players yet): none, so they add nothing
	// the citadel with a built, living heart: the heart's sample plays at the citadel's position
	const auto citadel = worship::citadel::Of(k_Player);
	if (worship::citadel::HasLivingHeart(citadel))
	{
		if (const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(citadel); transform != nullptr)
		{
			input.citadelHeart = transform->position;
		}
	}
	return input;
}

/// The chant music's game side: the citadel within 150 of the camera's MapCoords, its nearest site with dancers within
/// 100 and that site's dance ((inferred) every openblack site has its dance, made with it)
std::optional<audio::ChantSite> ChantSite()
{
	const auto camera = CameraMapCoords();
	if (!camera || !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto citadel = ecs::map_cells::GetNearestCitadel(*camera, 150.0f);
	if (citadel == entt::null)
	{
		return std::nullopt;
	}
	const auto site = worship::citadel::FindNearestWorshipSite(citadel, *camera, 100.0f);
	if (site == entt::null)
	{
		return std::nullopt;
	}
	const auto* component = Locator::entitiesRegistry::value().TryGet<const WorshipSite>(site);
	if (component == nullptr)
	{
		return std::nullopt;
	}
	audio::ChantSite chant {
	    .tribe = static_cast<int>(component->tribe),
	    .dancers = static_cast<uint32_t>(worship::site::DancerCount(site)), // the dance's dancer count
	};
	// the dance centre's MapCoords (zero without the mesh's point)
	map_coords::MapCoords centre {};
	if (const auto point = worship::site::GetSpecialPos(site, worship::site::Point::DanceCentre); point)
	{
		centre = map_coords::FromWorld(*point);
	}
	chant.position = map_coords::ToWorld(centre); // the ground's altitude + the altitude; x, z x 10 / 65536
	// the land's altitude at the centre and at the camera's MapCoords
	chant.ground = map_coords::ToWorld(map_coords::MapCoords {centre.x, centre.z, 0.0f}).y;
	chant.cameraGround = map_coords::ToWorld(map_coords::MapCoords {camera->x, camera->z, 0.0f}).y;
	return chant;
}

void RunViewHook(uint32_t turn)
{
	static const debug_env::Variable k_AudioTestView("OPENBLACK_AUDIO_TEST_VIEW");
	const char* view = k_AudioTestView.Get();
	if (view == nullptr || !Locator::entitiesRegistry::has_value() || !Locator::camera::has_value())
	{
		return;
	}
	unsigned at = 0;
	int wanted = 0;
	float distance = 4.0f;
	if (std::sscanf(view, "%u,%d,%f", &at, &wanted, &distance) < 2 || turn != at)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	int clip = -1;
	static const debug_env::Variable k_AudioTestAnim("OPENBLACK_AUDIO_TEST_ANIM");
	if (const char* anim = k_AudioTestAnim.Get(); anim != nullptr)
	{
		clip = std::atoi(anim);
	}
	int index = 0;
	registry.Each<const Villager, const Transform>([&](entt::entity entity, const Villager&, const Transform& t) {
		if (clip >= 0)
		{
			auto& animation = registry.AssignOrReplace<SkeletalAnimation>(entity);
			animation.clip = ecs::ClipId(static_cast<uint32_t>(clip));
			animation.clipIndex = clip;
			animation.locked = true;
			animation.hasClip = true;
			animation.time = 0.0f;
			animation.speed = 1.0f;
		}
		if (index++ != wanted)
		{
			return;
		}
		const glm::vec3 focus = t.position + glm::vec3(0.0f, 0.8f, 0.0f);
		Locator::camera::value().GetModel().SetFlight(focus + glm::vec3(0.0f, distance * 0.35f, distance), focus);
		SPDLOG_LOGGER_INFO(spdlog::get("audio"),
		                   "Audio test: turn {}, camera on villager {} at ({:.1f}, {:.1f}, {:.1f}), clip {}", turn, wanted,
		                   t.position.x, t.position.y, t.position.z, clip);
	});
}

void RunLanternHook()
{
	auto& hookTurn = AudioQueriesDebugHooksData().lanternHookTurn;
	++hookTurn;
	static const debug_env::Variable k_AudioTestLantern("OPENBLACK_AUDIO_TEST_LANTERN");
	const char* hook = k_AudioTestLantern.Get();
	if (hook == nullptr || !Locator::camera::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	unsigned at = 0;
	float distance = 3.0f;
	if (std::sscanf(hook, "%u,%f", &at, &distance) < 1 || hookTurn != at)
	{
		return;
	}
	const auto lanterns = StreetLanterns();
	if (lanterns.empty())
	{
		return;
	}
	const auto& lantern = lanterns.front();
	const glm::vec3 top = lantern.position + glm::vec3(0.0f, lantern.height, 0.0f);
	Locator::camera::value().GetModel().SetFlight(top + glm::vec3(0.0f, distance * 0.35f, distance), top);
	SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Audio test: camera on lantern {} top ({:.1f}, {:.1f}, {:.1f})",
	                   static_cast<uint32_t>(lantern.thing), top.x, top.y, top.z);
}
} // namespace

void ecs::audio_queries::RunCitadelTestHook()
{
	auto& hookTurn = AudioQueriesDebugHooksData().citadelHookTurn;
	++hookTurn;
	static const debug_env::Variable k_AudioTestCitadel("OPENBLACK_AUDIO_TEST_CITADEL");
	const char* hook = k_AudioTestCitadel.Get();
	if (hook == nullptr || !Locator::temple::has_value())
	{
		return;
	}
	unsigned in = 0;
	unsigned out = 0;
	if (std::sscanf(hook, "%u,%u", &in, &out) < 1)
	{
		return;
	}
	auto& temple = Locator::temple::value();
	if (hookTurn == in && !temple.Active())
	{
		temple.Activate();
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Audio test: inside the citadel");
	}
	else if (out != 0 && hookTurn == out && temple.Active())
	{
		temple.Deactivate();
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Audio test: out of the citadel");
	}
}

float ecs::audio_queries::AudioAlignmentValue(float x)
{
	// in float steps: below 0 or NaN -> 0; above 1 -> 1; s = (1 - x) + (1 - x) (the value the sky keeps); the result
	// is 2 - s - 1
	if (!(x >= 0.0f))
	{
		x = 0.0f;
	}
	else if (x > 1.0f)
	{
		x = 1.0f;
	}
	const float s = (1.0f - x) + (1.0f - x);
	const float twoMinusS = 2.0f - s;
	return twoMinusS - 1.0f;
}

void ecs::audio_queries::Fill(audio::GameQueries& queries)
{
	// the land's surface under a point: ecs::sea_cells is the single source
	queries.surfaceType = [](glm::vec3 point) { return ecs::sea_cells::GetSurfaceType(point); };
	queries.weatherSmooth = &WeatherSmooth;
	// a full screen film is playing (the video player, atomic)
	queries.videoPlaying = &video::IsPlaying;
	// the audio's alignment value (for the atmosphere banks and the alignment music)
	queries.cameraAlignment = &CameraAlignment;
	// the local player's alignment (the citadel music): openblack's is PLAYER_ONE
	queries.localPlayerAlignment = []() { return ecs::effects::alignment::Get(PlayerNames::PLAYER_ONE); };
	queries.animatedThing = &AnimatedThing;
	queries.animationClipName = &AnimationClipName;
	queries.streetLanterns = &StreetLanterns;
	// the tribe's music towns through ecs::map_cells: the nearest one with a centre, its tribe and its distance to the
	// camera's MapCoords
	queries.nearestTown = &NearestMusicTown;
	queries.town = &KeptMusicTown;
	// the resource drop sound: the nearest town (map_cells::GetNearestTown) and its three needs (ecs::town_desire)
	queries.nearestTownAt = &NearestTownAt;
	queries.townResourceNeeds = &TownResourceNeeds;
	// the town desire sounds: every town's sorted raw desires (ecs::town_desire); the worship site desire sounds: the
	// citadel's sites (worship::citadel / worship::site)
	queries.desireTowns = &DesireTowns;
	queries.worshipSites = &WorshipSites;
	// the heart beat: the protection desires, the believers, the influence power and the heart
	queries.heartBeat = &HeartBeat;
	// the chant music: the worship site near the camera (GameMusic plays its chant)
	queries.chantSite = &ChantSite;
}

void ecs::audio_queries::RunTestHooks(uint32_t turn)
{
	RunViewHook(turn);
	RunLanternHook();
	RunCitadelTestHook();
}
