/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MissionaryBoat.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <array>
#include <memory>
#include <optional>
#include <utility>

#include <entt/core/hashed_string.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/L3DAnim.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Audio.h"
#include "Common/GameRandom.h"
#include "ECS/Animations.h"
#include "ECS/Components/DynamicShadow.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/DisappearSmoke.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/WorldEffectsInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::missionary_boat
{
namespace
{
using namespace components;

constexpr glm::vec3 k_Dock {1881.0833f, 8.1316004f, 3154.1094f}; // the dry dock
constexpr glm::vec3 k_SeaStart {1456.54f, 0.0f, 3263.06f};       // where the crossing starts
constexpr float k_HalfPi = 1.5707964f;
constexpr float k_QuarterPi = 0.78539819f;
// 1 / sqrt 2 as a table lookup plus one Newton step gives it; negated, the speed's direction
constexpr float k_InvSqrt2 = 0.70710659f;
constexpr float k_Speed = 0.005f; // units per ms
constexpr int32_t k_CrossingMs = 60000;
constexpr int32_t k_HoldMs = 3000;
constexpr int32_t k_EndMargin = 400; // len(Boat1) - 400 turns it to mode 1
constexpr int32_t k_WakeCycle = 6000;
constexpr int32_t k_PuffMs = 200;
constexpr int32_t k_PushedMs = 850;
// the ScriptSfx samples at these clip times, played in 2D
constexpr std::array<int32_t, 3> k_SoundTimes = {100, 1500, 3900};
constexpr std::array<int, 3> k_Sounds = {62, 61, 60}; // ScriptSfx MissionaryBoatCreak / Slide / Splash_01
// the five sailors on the dock
constexpr std::array<float, 5> k_SailorX = {5.2f, 5.3f, 5.5f, 5.0f, 5.0f};
constexpr std::array<int32_t, 5> k_SailorPhase = {5, 500, 1500, 455, 2000};
constexpr std::array<float, 5> k_SailorZ = {1.0f, -0.7f, 0.0f, 0.4f, -0.2f};

// the boat's animations and meshes
constexpr uint32_t k_PushObject = 346;                        // pushing an object
constexpr std::array<uint32_t, 3> k_Pushed = {332, 235, 333}; // overworked 1, crowd won 2, overworked 2
constexpr uint32_t k_Titanic = 406;                           // the Titanic pose
constexpr uint32_t k_Sitting = 378;                           // sitting, swinging legs
constexpr uint32_t k_Scrubbs = 359;                           // scrubbing
constexpr uint32_t k_CowEat = 36;                             // cow eating

/// One person, animal or thing drawn on deck in mode 1: the matrix L . hull,
/// L = RotY(angle) x scale + t in the hull's frame, the clip time (timer + phase) % the clip's length
struct DeckPlace
{
	MeshId mesh;
	uint32_t clip; ///< 0: static
	float angle;
	float scale;
	glm::vec3 offset;
	int32_t phase;
};
const std::array<DeckPlace, 8> k_Deck = {{
    {MeshId::AnimalCow1, k_CowEat, -1.0f, 1.0f, {-1.778f, 10.78f, 1.83f}, 0},
    {MeshId::AnimalCow1, k_CowEat, -0.7f, 1.0f, {-1.778f, 10.78f, 3.83f}, 1255},
    {MeshId::SpellGrainPile, 0, 0.0f, 0.26f, {-5.708f, 10.854f, 3.199f}, 0},
    {MeshId::PersonNorseFemaleA1, k_Titanic, 0.0f, 1.0f, {-0.14f, 13.213f, -19.657f}, 0},
    {MeshId::PersonNorseSailor, k_Titanic, 0.0f, 1.0f, {-0.14f, 13.213f, -18.9f}, 500},
    {MeshId::PersonNorseSailor, k_Sitting, 3.1415927f, 1.0f, {-6.25f, 11.424f, -7.227f}, 0},
    {MeshId::PersonNorseSailor, k_Sitting, 3.1415927f, 1.0f, {-5.25f, 11.424f, -7.227f}, 2345},
    {MeshId::PersonNorseSailor, k_Scrubbs, 3.1415927f, 1.0f, {5.881f, 10.741f, 0.174f}, 0},
}};

struct Boat
{
	int32_t mode {0};
	int32_t animTime {0};     ///< the hull's clip
	int32_t previousTime {0}; ///< animTime before this frame (the sound thresholds)
	int32_t timer {0};
	int32_t puffTimer {0};
	bool hold {true}; ///< the hull waits 3 s before sliding
	std::array<int32_t, 5> wakePhase {};
	glm::mat4 hull {1.0f};  ///< the hull's matrix (LH row matrix as a glm column one)
	bool reflected {false}; ///< PreDraw drew the hull under water this frame
	entt::entity hullEntity {entt::null};
	std::vector<entt::entity> people; ///< the five sailors (mode 0) or the deck (mode 1)
};

/// What this module keeps between calls (Locator::debugHooks)
struct MissionaryBoatDebugState
{
	// OPENBLACK_TEST_JC_SPECIAL's delay: the mode to make once that much game time has gone (-1: nothing pending)
	int32_t pendingMode {-1};
	int32_t pendingFrames {0};
	int32_t fastForwardMs {0};
	float traceClock {0.0f};
};

MissionaryBoatDebugState& MissionaryBoatDebugData()
{
	return openblack::Locator::debugHooks::value().Get<MissionaryBoatDebugState>();
}
/// What this module keeps between calls (Locator::worldEffects)
struct MissionaryBoatState
{
	std::optional<Boat> boat {};
	std::vector<WakeSprite> wake {};
	// a clip that does not load is reported once
	std::array<bool, 2> clipTried {};
};

MissionaryBoatState& MissionaryBoatData()
{
	return openblack::Locator::worldEffects::value().Get<MissionaryBoatState>();
}

/// Data\MISC\Boat1.anm / Boat2.anm, one track matrix each
const L3DAnim* Clip(int32_t which)
{
	const auto index = static_cast<size_t>(which);
	const char* file = which == 0 ? "boat1.anm" : "boat2.anm";
	auto& animations = Locator::resources::value().GetAnimations();
	const auto id = entt::hashed_string((std::string("misc/") + file).c_str()).value();
	auto& tried = MissionaryBoatData().clipTried;
	if (!animations.Contains(id) && !tried.at(index))
	{
		tried.at(index) = true;
		const auto path = Locator::filesystem::value().GetPath<filesystem::Path::Misc>() / file;
		try
		{
			animations.Load(id, resources::L3DAnimLoader::FromDiskTag {}, path);
		}
		catch (const std::exception&)
		{
		}
		if (!animations.Contains(id) || animations.Handle(id)->GetFrames().empty())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Missionary boat: cannot load {}", path.generic_string());
		}
	}
	if (!animations.Contains(id))
	{
		return nullptr;
	}
	const auto& clip = *animations.Handle(id);
	return clip.GetFrames().empty() ? nullptr : &clip;
}

int32_t ClipLength(uint32_t clip)
{
	const auto& animations = Locator::resources::value().GetAnimations();
	const auto id = ClipId(clip);
	return animations.Contains(id) ? std::max(animations.Handle(id)->GetDurationMs(), 1) : 1;
}

float Altitude(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

/// RotateY(angle) of a row matrix as a factor on the right (M RotY = affine::RotateY(M, angle))
glm::mat4 RotY(float angle)
{
	return glm::mat4(affine::AngleY(angle));
}

entt::entity MakeObject(MeshId mesh, uint32_t clip)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, k_Dock, glm::mat3(1.0f), glm::vec3(1.0f));
	const bool animated = clip != 0;
	registry.Assign<Mesh>(entity, resources::HashIdentifier(mesh), static_cast<int8_t>(0),
	                      static_cast<int8_t>(animated ? 0 : 1));
	if (animated)
	{
		// the clip time is set by the boat every frame, it does not run by itself
		auto& animation = registry.Assign<SkeletalAnimation>(entity);
		animation.clip = ClipId(clip);
		animation.clipIndex = static_cast<int32_t>(clip);
		animation.hasClip = true;
		animation.speed = 0.0f;
	}
	return entity;
}

void SetClip(entt::entity entity, uint32_t clip, int32_t time)
{
	auto& animation = Locator::entitiesRegistry::value().Get<SkeletalAnimation>(entity);
	animation.clip = ClipId(clip);
	animation.clipIndex = static_cast<int32_t>(clip);
	animation.time = static_cast<float>(time);
}

void Place(entt::entity entity, const glm::mat4& matrix, float scale = 1.0f)
{
	auto& transform = Locator::entitiesRegistry::value().Get<Transform>(entity);
	transform.position = glm::vec3(matrix[3]);
	transform.rotation = glm::mat3(matrix);
	transform.scale = glm::vec3(scale);
}

/// Destroys the boat and everything it made
void Free()
{
	auto& state = MissionaryBoatData();
	if (!state.boat.has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(state.boat->hullEntity))
	{
		registry.Destroy(state.boat->hullEntity);
	}
	for (const auto entity : state.boat->people)
	{
		if (registry.Valid(entity))
		{
			registry.Destroy(entity);
		}
	}
	state.boat.reset();
	state.wake.clear();
	registry.SetDirty();
}

void PlaySample(int sample)
{
	// default play options, bank ScriptSfx, not 3D, no owner, mode 2: one of the 16 sound channels
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::ScriptSfx), sample};
	options.mode = 2;
	audio::PlaySoundEffect(options);
}

/// PreDraw: the clip and the hull's matrix. False when the boat was freed (or turned into a new one, which gets no
/// PreDraw this frame).
bool PreDraw(int32_t dt)
{
	auto& boat = *MissionaryBoatData().boat;
	boat.reflected = false;
	if (boat.mode == 0)
	{
		boat.timer += dt;
		boat.previousTime = boat.animTime;
		if (!boat.hold || boat.timer > k_HoldMs)
		{
			boat.animTime += dt;
		}
		const auto* clip = Clip(0);
		if (clip == nullptr || boat.animTime > clip->GetDurationMs() - k_EndMargin)
		{
			Create(1);
			return false;
		}
		// M = Translate(dock), the track . M, RotateY(pi / 2), diag(-1, 1, 1) in front
		std::vector<glm::mat4> track;
		clip->SampleLocal(boat.animTime, track);
		boat.hull = glm::translate(k_Dock) * track.at(0) * RotY(k_HalfPi) * glm::scale(glm::vec3(-1.0f, 1.0f, 1.0f));
		// y += altitude under the hull - altitude of the dock
		boat.hull[3].y += Altitude(boat.hull[3].x, boat.hull[3].z) - Altitude(k_Dock.x, k_Dock.z);
		Place(boat.hullEntity, boat.hull);
		boat.reflected = true; // drawn under water; then it gets the land light back
		return true;
	}
	// mode 1: Boat2's time, wrapped when the clip loops, else held on its last millisecond
	const auto* clip = Clip(1);
	if (clip != nullptr)
	{
		const int32_t length = std::max(clip->GetDurationMs(), 1);
		boat.animTime = clip->IsLooping() ? (boat.animTime + dt) % length : std::min(boat.animTime + dt, length - 1);
	}
	boat.timer += dt;
	if (boat.timer > k_CrossingMs)
	{
		Free();
		return false;
	}
	const float travelled = static_cast<float>(boat.timer) * k_Speed;
	const glm::vec3 position = k_SeaStart + glm::vec3(-k_InvSqrt2, 0.0f, -k_InvSqrt2) * travelled;
	glm::mat4 track(1.0f);
	if (clip != nullptr)
	{
		std::vector<glm::mat4> bones;
		clip->SampleLocal(boat.animTime, bones);
		track = bones.at(0);
	}
	boat.hull =
	    glm::translate(position) * RotY(k_QuarterPi) * track * RotY(k_HalfPi) * glm::scale(glm::vec3(-1.0f, 1.0f, 1.0f));
	Place(boat.hullEntity, boat.hull);
	boat.reflected = true;
	return true;
}

/// PostDraw: sounds, dust, the sailors or the deck, and the wake
void PostDraw(int32_t dt)
{
	auto& state = MissionaryBoatData();
	auto& boat = *state.boat;
	state.wake.clear();
	const glm::vec3 hullPosition(boat.hull[3]);
	if (boat.mode == 0)
	{
		for (size_t i = 0; i < k_SoundTimes.size(); ++i)
		{
			if (boat.previousTime < k_SoundTimes.at(i) && boat.animTime > k_SoundTimes.at(i))
			{
				PlaySample(k_Sounds.at(i));
			}
		}
		boat.puffTimer += dt;
		if (boat.puffTimer > k_PuffMs)
		{
			if (boat.animTime > 3900)
			{
				if (boat.animTime < 6500)
				{
					// two white sprays of size 7 at hull + (Random(-2, 2) - 10, 7, Random(-20, 20))
					for (int k = 0; k < 2; ++k)
					{
						const float r1 = game_random::crt::Random(-20.0f, 20.0f);
						const float r2 = game_random::crt::Random(-2.0f, 2.0f);
						disappear_smoke::Create(hullPosition + glm::vec3(r2 - 10.0f, 7.0f, r1), 0, 7.0f, 0xFEFFFFFFu);
					}
				}
			}
			else if (boat.animTime > 1130)
			{
				// two sand-coloured dusts of size 5 at hull + (Random(-2, 2), 0, Random(-20, 20))
				for (int k = 0; k < 2; ++k)
				{
					const float r1 = game_random::crt::Random(-20.0f, 20.0f);
					const float r2 = game_random::crt::Random(-2.0f, 2.0f);
					disappear_smoke::Create(hullPosition + glm::vec3(r2, 0.0f, r1), 0, 5.0f, 0xFFB88C38u);
				}
			}
			boat.puffTimer = 0;
		}
		// the one sailor object drawn five times at the dock, turned -pi/2
		for (size_t i = 0; i < boat.people.size(); ++i)
		{
			const auto entity = boat.people[i];
			auto& animation = Locator::entitiesRegistry::value().Get<SkeletalAnimation>(entity);
			uint32_t clip = static_cast<uint32_t>(animation.clipIndex);
			if (boat.animTime > k_PushedMs)
			{
				if (boat.hold)
				{
					boat.timer = 0;
				}
				clip = k_Pushed.at(i % 3);
				boat.hold = false;
			}
			glm::vec3 at(k_Dock.x + k_SailorX.at(i), 0.0f,
			             (static_cast<float>(i) - 2.5f) * 3.0f + k_SailorZ.at(i) + k_Dock.z + 10.0f);
			at.y = Altitude(at.x, at.z);
			Place(entity, glm::translate(at) * RotY(-k_HalfPi));
			SetClip(entity, clip, (k_SailorPhase.at(i) + boat.timer) % ClipLength(clip));
		}
		return;
	}
	// mode 1: the wake, five flat sprites 6 s apart behind the hull
	for (auto& phase : boat.wakePhase)
	{
		phase = (phase + dt) % k_WakeCycle;
		const float t = static_cast<float>(phase) * 0.000166667f;
		glm::vec3 at = glm::vec3(boat.hull * glm::vec4(0.0f, 0.0f, t * 100.0f - 15.0f, 1.0f));
		at.y = 0.2f;
		// the fade starts at a value nothing ever writes: 0
		const float f = t;
		const auto alpha = static_cast<uint32_t>(static_cast<int32_t>((1.0f - f) * 255.0f)) & 0xFFu;
		if (f > 0.2)
		{
			state.wake.push_back({at, t * 30.0f + 10.0f, 0.5f, 3.9269910f, 0x31, (alpha << 24) | 0x00FFFFFFu});
		}
	}
	// the deck: each thing's matrix L . hull, its clip at timer + phase (the hull's colour: see the wiki)
	for (size_t i = 0; i < boat.people.size() && i < k_Deck.size(); ++i)
	{
		const auto& place = k_Deck.at(i);
		const glm::mat4 local = glm::translate(place.offset) * RotY(place.angle);
		Place(boat.people[i], boat.hull * local, place.scale);
		if (place.clip != 0)
		{
			SetClip(boat.people[i], place.clip, (boat.timer + place.phase) % ClipLength(place.clip));
		}
	}
}
} // namespace

void Create(int32_t mode)
{
	Free();
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	Boat boat;
	boat.mode = mode;
	// the hull: a static ark mesh at the dock, angle 0, scale 1
	boat.hullEntity = MakeObject(MeshId::ObjectArk, 0);
	boat.hull = glm::translate(k_Dock);
	Place(boat.hullEntity, boat.hull);
	auto& registry = Locator::entitiesRegistry::value();
	if (mode == 0)
	{
		// its dynamic shadow, lit by the fixed sun and falling on objects too
		registry.Assign<DynamicShadow>(boat.hullEntity, DynamicShadow {.onObjects = true, .useSun = true});
		// the sailor pushing the boat, drawn five times
		for (int i = 0; i < 5; ++i)
		{
			boat.people.push_back(MakeObject(MeshId::PersonNorseSailor, k_PushObject));
		}
	}
	else
	{
		// the eating cow twice, the grain pile, the sailor object five times; the wake
		for (const auto& place : k_Deck)
		{
			boat.people.push_back(MakeObject(place.mesh, place.clip));
		}
		for (size_t i = 0; i < boat.wakePhase.size(); ++i)
		{
			boat.wakePhase.at(i) = static_cast<int32_t>(i) * k_WakeCycle / 5;
		}
	}
	MissionaryBoatData().boat = boat;
	registry.SetDirty();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Missionary boat: mode {}", mode);
}

void Step(int32_t dt);

void Update(float gameMilliseconds)
{
	auto& debug = MissionaryBoatDebugData();
	auto& state = MissionaryBoatData();
	if (debug.pendingMode >= 0 && --debug.pendingFrames <= 0)
	{
		Create(std::exchange(debug.pendingMode, -1));
		// test only: that much game time at once, in steps of 33 ms
		for (; debug.fastForwardMs > 0 && state.boat.has_value(); debug.fastForwardMs -= 33)
		{
			Step(33);
		}
	}
	if (state.boat.has_value() && std::getenv("OPENBLACK_BOAT_TRACE") != nullptr)
	{
		debug.traceClock += gameMilliseconds;
		if (debug.traceClock >= 500.0f)
		{
			debug.traceClock = 0.0f;
			SPDLOG_LOGGER_INFO(
			    spdlog::get("game"), "Boat: mode {} clip {} timer {} hold {} hull ({:.2f}, {:.2f}, {:.2f}) wake {} puffs {}",
			    state.boat->mode, state.boat->animTime, state.boat->timer, state.boat->hold, state.boat->hull[3].x,
			    state.boat->hull[3].y, state.boat->hull[3].z, state.wake.size(), disappear_smoke::Get().size());
		}
	}
	// the frame's game time: whole ms already (game_clock::FrameGameMs from Game)
	Step(static_cast<int32_t>(std::max(gameMilliseconds, 0.0f)));
}

void Step(int32_t dt)
{
	auto& state = MissionaryBoatData();
	// the landscape's draw: PreDraw
	if (state.boat.has_value())
	{
		PreDraw(dt);
	}
	// the smoke puffs, then PostDraw of whatever boat there is now
	disappear_smoke::Update(static_cast<float>(dt) * 0.001f);
	if (state.boat.has_value())
	{
		PostDraw(dt);
	}
	else
	{
		state.wake.clear();
	}
}

entt::entity GetHull()
{
	auto& state = MissionaryBoatData();
	return state.boat.has_value() ? state.boat->hullEntity : entt::null;
}

entt::entity GetReflectedHull()
{
	auto& state = MissionaryBoatData();
	return state.boat.has_value() && state.boat->reflected ? state.boat->hullEntity : entt::null;
}

const std::vector<WakeSprite>& GetWake()
{
	return MissionaryBoatData().wake;
}

void RunDebugHook()
{
	auto& state = MissionaryBoatDebugData();
	const char* test = std::getenv("OPENBLACK_TEST_JC_SPECIAL");
	if (test == nullptr)
	{
		return;
	}
	int special = 0;
	int mode = 0;
	int frames = 0;
	int fastForward = 0;
	if (std::sscanf(test, "%d,%d,%d,%d", &special, &mode, &frames, &fastForward) < 1 || special != 6)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"),
		                   "JC special test: OPENBLACK_TEST_JC_SPECIAL=\"6[,mode[,frames[,fast forward ms]]]\", got \"{}\"",
		                   test);
		return;
	}
	state.pendingMode = mode;
	state.pendingFrames = frames;
	state.fastForwardMs = fastForward;
}

} // namespace openblack::ecs::missionary_boat
