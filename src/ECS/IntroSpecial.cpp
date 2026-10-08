/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "IntroSpecial.h"

#include <cmath>

#include <exception>
#include <string>

#include <glm/vec4.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "3D/SkeletalPose.h"
#include "Camera/ScriptCamera.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/SuperVillager.h"
#include "ECS/Systems/WorldEffectsInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/OverlayFrame.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::intro_special
{
using namespace components;
using graphics::billboard::Sprite;

// ---- the pure parts ------------------------------------------------------------------------------------------------

float PutDownYaw()
{
	const float step = std::bit_cast<float>(0x41700000u) * std::bit_cast<float>(0x3C8EFA35u); // 15 x pi/180
	return std::bit_cast<float>(0xBF490FDBu) - step;                                          // -pi/4 - step
}

int32_t AdvanceTime(int32_t time, uint32_t milliseconds, int32_t duration, bool looping, bool& wrapped)
{
	const auto n = static_cast<int32_t>(static_cast<uint32_t>(time) + milliseconds);
	int32_t result = n;
	if (looping)
	{
		// the signed remainder. (pending) a clip of 0 ms divides by zero in the original
		result = duration > 0 ? n % duration : 0;
	}
	else if (duration - 1 < n)
	{
		result = duration - 1;
	}
	wrapped = result < time; // the time went back
	return result;
}

namespace light
{
namespace
{
constexpr float k_TwoPi = std::bit_cast<float>(0x40C90FDBu);        ///< Random(0, 2 pi)
constexpr float k_SizeStep = std::bit_cast<float>(0x3D4CCCCDu);     ///< 0.05
constexpr float k_SizeBase = std::bit_cast<float>(0x40400000u);     ///< 3
constexpr float k_MinSize = std::bit_cast<float>(0x38D1B717u);      ///< 1e-4, SetSize's floor
constexpr float k_Jitter = std::bit_cast<float>(0x40000000u);       ///< Random(-2, 2)
constexpr float k_TailBack = std::bit_cast<float>(0xC0C00000u);     ///< -6: sprite 19 behind the start
constexpr float k_StreakSize = std::bit_cast<float>(0x43AF0000u);   ///< 350
constexpr float k_StreakHeight = std::bit_cast<float>(0x3D4CCCCDu); ///< 0.05: half height 17.5
constexpr float k_StreakAngle = std::bit_cast<float>(0xBEC90FDBu);  ///< -pi / 8
constexpr float k_GlowSize = std::bit_cast<float>(0x42A00000u);     ///< 80, sprite 18

/// The sprite's set-size rule: the origin times new / old, then the size. The light's origins
/// stay 0, so only the size changes
void SetSize(Sprite& sprite, float size)
{
	const float ratio = size / sprite.size;
	sprite.origin.x = ratio * sprite.origin.x;
	sprite.origin.y = ratio * sprite.origin.y;
	sprite.size = size;
}

/// float(15 (19 - i)) x 0.05 (+ jitter) + 3, at least 1e-4
float Floor(float size)
{
	return size < k_MinSize ? k_MinSize : size;
}
} // namespace

Light Create(const glm::vec3& target, const glm::vec3& direction, const RandomFn& random)
{
	Light light;
	// (x^2 + y^2) + z^2, its inverse square root, each component times it
	const float lengthSquared = (direction.x * direction.x + direction.y * direction.y) + direction.z * direction.z;
	const float inverse = affine::InverseSquareRoot(lengthSquared);
	light.direction = {inverse * direction.x, inverse * direction.y, inverse * direction.z};
	// start = target + direction x -4000; head and start both
	light.start = {target.x + light.direction.x * k_Far, target.y + light.direction.y * k_Far,
	               target.z + light.direction.z * k_Far};
	light.head = light.start;
	// 20 sprites, then per sprite
	for (int i = 0; i < k_Sprites; ++i)
	{
		auto& sprite = light.sprites.at(static_cast<size_t>(i));
		sprite = Sprite {}; // zeroed (with its own material)
		// start + direction x (float(-i) x 6)
		const float k = static_cast<float>(-i) * k_Spacing;
		sprite.position = {light.direction.x * k + light.start.x, light.direction.y * k + light.start.y,
		                   light.direction.z * k + light.start.z};
		const auto steps = static_cast<float>((19 - i) * 15);
		float size = steps * k_SizeStep + k_SizeBase;
		if (i > 5)
		{
			sprite.cell = 50;
			size = size + size;
		}
		else
		{
			sprite.cell = 48;
		}
		SetSize(sprite, Floor(size));
		sprite.angle = random(0.0f, k_TwoPi);
		// alpha (19 - i) 255 / 20 (truncated), white
		sprite.argb = (static_cast<uint32_t>((19 - i) * 255 / 20) << 24) | 0xFFFFFFu;
		if (i == 18) // the glow at the head, 80 wide, alpha 0x28
		{
			sprite.argb = 0x28FFFFFFu;
			SetSize(sprite, k_GlowSize);
			sprite.position = light.start;
		}
	}
	// sprite 19, a thin streak 350 x 17.5 (half sizes) 6 behind the start, turned -pi / 8
	auto& streak = light.sprites.at(19);
	streak.position = {light.direction.x * k_TailBack + light.start.x, light.direction.y * k_TailBack + light.start.y,
	                   light.direction.z * k_TailBack + light.start.z};
	streak.angle = k_StreakAngle;
	streak.argb = 0x0EFFFFFFu;
	streak.cell = 49;
	SetSize(streak, k_StreakSize);
	streak.height = k_StreakHeight; // SetHeight: the origin's y times 17.5 / old half height (0)
	light.state = State::Falling;   // state, elapsed and arrived = 0
	light.elapsed = 0;
	light.arrived = false;
	return light;
}

glm::vec3 HeadPosition(Light& light)
{
	float d = static_cast<float>(light.elapsed) * k_Speed;
	if (!(d <= k_Travel))
	{
		light.arrived = true;
		d = k_Travel;
	}
	return {d * light.direction.x + light.start.x, d * light.direction.y + light.start.y,
	        d * light.direction.z + light.start.z};
}

glm::vec3 Start(Light& light)
{
	light.head = HeadPosition(light);
	return light.head;
}

void Draw(Light& light, uint32_t milliseconds, int32_t& timer, const RandomFn& random, Frame& out)
{
	out.drawn.clear();
	out.depthAlways = false;
	out.cameraPosition.reset();
	const auto msSigned = static_cast<int32_t>(milliseconds);
	if (light.state == State::Done)
	{
		return;
	}
	if (light.state == State::Falling)
	{
		const glm::vec3 head = Start(light);
		if (light.arrived) // hold for 500 ms
		{
			light.state = State::Hold;
			timer = k_HoldMs;
		}
		for (int i = 0; i < k_Sprites; ++i)
		{
			auto& sprite = light.sprites.at(static_cast<size_t>(i));
			if (i == 19) // the streak turns, the frame's game ms x the spin + angle
			{
				sprite.angle = static_cast<float>(msSigned) * k_Spin + sprite.angle;
			}
			sprite.position = head;
			if (i >= 18) // 18 and 19 stay at the head
			{
				continue;
			}
			const float k = static_cast<float>(-i) * k_Spacing;
			sprite.position = {light.direction.x * k + head.x, light.direction.y * k + head.y, light.direction.z * k + head.z};
			if (milliseconds == 0) // no draws while the game time stands
			{
				continue;
			}
			sprite.angle = random(0.0f, k_TwoPi);
			if (i >= 5)
			{
				continue;
			}
			// the five front sprites flicker, (float(15 (19 - i)) x 0.05 + Random(-2, 2)) + 3
			const float jitter = random(-k_Jitter, k_Jitter);
			const float steps = static_cast<float>((19 - i) * 15);
			SetSize(sprite, Floor((steps * k_SizeStep + jitter) + k_SizeBase));
		}
		// ZFUNC ALWAYS around the 20 draws once the fall has run 8237 ms
		out.depthAlways = light.elapsed > k_ZAlwaysAfterMs;
		out.drawn.assign(light.sprites.begin(), light.sprites.end());
		light.elapsed = static_cast<int32_t>(static_cast<uint32_t>(light.elapsed) + milliseconds);
		// the debug camera's position, 150 back up the beam and 30 above
		glm::vec3 camera = HeadPosition(light);
		const glm::vec3 back {light.direction.x * k_CameraBack, light.direction.y * k_CameraBack,
		                      light.direction.z * k_CameraBack};
		camera.x = camera.x - back.x;
		camera.z = camera.z - back.z;
		camera.y = (camera.y - back.y) + k_CameraUp;
		out.cameraPosition = camera;
		return;
	}
	auto& glow = light.sprites.at(0);
	if (light.state == State::Hold)
	{
		timer = static_cast<int32_t>(static_cast<uint32_t>(timer) - milliseconds);
		if (timer < 0)
		{
			light.state = State::Fade;
			timer = k_FadeMs;
		}
	}
	if (light.state == State::Fade) // (int)((timer / 1000) x (255 - 50) + 50)
	{
		const float fraction = static_cast<float>(timer) / static_cast<float>(k_FadeMs);
		const float alpha = fraction * static_cast<float>(0xFF - k_FadeFloor) + static_cast<float>(k_FadeFloor);
		glow.argb = (static_cast<uint32_t>(static_cast<int32_t>(alpha)) << 24) | 0xFFFFFFu;
		timer = static_cast<int32_t>(static_cast<uint32_t>(timer) - milliseconds);
		if (timer < 0)
		{
			light.state = State::Done;
		}
	}
	// ZFUNC ALWAYS; sprite 0 three times, a new Random(0, 2 pi) angle after the first and second
	out.depthAlways = true;
	out.drawn.push_back(glow);
	glow.angle = random(0.0f, k_TwoPi);
	out.drawn.push_back(glow);
	glow.angle = random(0.0f, k_TwoPi);
	out.drawn.push_back(glow);
}
} // namespace light

std::optional<glm::vec3> GripPoint(const graphics::L3DMesh& mesh, const L3DAnim& clip, int32_t time, const glm::mat4& model,
                                   const std::array<float, 12>& eBoneMatrix, int32_t bone)
{
	// the cycle time against 1817 (nothing past it)
	if (!(static_cast<float>(time) <= k_GripLastMs))
	{
		return std::nullopt;
	}
	// the pose at the time: every bone in the world
	std::vector<glm::mat4> pose;
	graphics::ComputePose(mesh, clip, static_cast<float>(time), pose);
	if (bone < 0 || static_cast<size_t>(bone) >= pose.size())
	{
		return std::nullopt; // (pending) the original indexes the buffer with it all the same
	}
	// the EBone's bone times the EBone matrix 0: its translation is the EBone point (cells 9..11) through the bone
	const glm::vec4 point =
	    model * pose.at(static_cast<size_t>(bone)) * glm::vec4(eBoneMatrix.at(9), eBoneMatrix.at(10), eBoneMatrix.at(11), 1.0f);
	return glm::vec3(point.x, point.y - k_GripDrop, point.z);
}

// ---- the game's Intro ----------------------------------------------------------------------------------------------

namespace
{
/// The intro hand, an animated 3D object (one render flag pending, as for the SuperVillager eyes)
struct HandObject
{
	entt::entity entity {entt::null};
	/// The HD mesh "Data\\MISC\\hand_intro.l3d"
	const super_villager::HdModel* model {nullptr};
	/// The clip's id in the animation manager (0: none)
	entt::id_type clip {0};
	/// the object's cycle time
	int32_t time {0};
	/// the last SetPosition (T(p) Ry(-a) S(s))
	glm::vec3 position {0.0f};
	float yaw {0.0f};
	float scale {1.0f};
};

struct Runtime
{
	int32_t state {-1};
	std::optional<light::Light> beam; ///< The light
	bool material {false};            ///< The misc0 material made (no GPU object in openblack)
	bool cameraFollow {false};        ///< The debug camera's focus kept on the Son's spot each frame
	bool pickUpClipFinished {false};
	HandObject hand;
	bool playing {false};
	float pickUpYaw {0.0f}; ///< Never written (0)
	int32_t pickUpWait {0}; ///< 1000 once the light is gone, then the pick-up clip runs
	bool followSon {false};
	int32_t lightTimer {0};
	/// this frame: the light's Z object and its callback's draws
	bool lightQueued {false};
	glm::vec3 lightKey {0.0f};
	light::Frame lightFrame;
	/// the last grip written (the value the landscape draw reads)
	std::optional<glm::vec3> grip;
};

/// The IntroSpecial state (Locator::worldEffects)
Runtime& IntroSpecialData()
{
	return openblack::Locator::worldEffects::value().Get<Runtime>();
}

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

float CrtRandom(float a, float b)
{
	return game_random::crt::Random(a, b); // the CRT rand() stream
}

/// Data\MISC\<file>, loaded once into the animation manager. (approximate) the original frees the
/// clip and reads the file again at each special 4; the data is the same
entt::id_type LoadClip(const char* file)
{
	const auto id = resources::HashIdentifier(std::string("misc/") + file);
	auto& animations = Locator::resources::value().GetAnimations();
	if (animations.Contains(id))
	{
		return id;
	}
	try
	{
		const auto path = Locator::filesystem::value().GetPath<filesystem::Path::Misc>() / file;
		animations.Load(id, resources::L3DAnimLoader::FromDiskTag {}, path);
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Intro: cannot load {}: {}", file, e.what());
		return 0;
	}
	return id;
}

const L3DAnim* Clip(entt::id_type id)
{
	if (id == 0)
	{
		return nullptr;
	}
	const auto& animations = Locator::resources::value().GetAnimations();
	return animations.Contains(id) ? &*animations.Handle(id) : nullptr;
}

/// The animated object with the HD mesh: an entity with no Mesh yet (Update gives it one on the frames the hand is
/// drawn)
void CreateHand()
{
	auto& hand = IntroSpecialData().hand;
	hand = HandObject {};
	hand.model = super_villager::LoadHdModel("hand_intro", "hand_intro");
	auto& registry = Entities();
	hand.entity = registry.Create();
	registry.Assign<Transform>(hand.entity, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& animation = registry.Assign<SkeletalAnimation>(hand.entity);
	animation.speed = 0.0f; // the time is the Intro's, it does not run by itself
}

/// The hand's current clip
void SetClip(entt::id_type clip)
{
	auto& state = IntroSpecialData();
	state.hand.clip = clip;
	if (state.hand.entity != entt::null && Entities().Valid(state.hand.entity))
	{
		auto& animation = Entities().Get<SkeletalAnimation>(state.hand.entity);
		animation.clip = clip;
		animation.hasClip = clip != 0;
	}
}

/// The hand's position, yaw and scale
void SetPosition(const glm::vec3& position, float yaw, float scale)
{
	auto& hand = IntroSpecialData().hand;
	hand.position = position;
	hand.yaw = yaw;
	hand.scale = scale;
}

/// The hand object, its clip and its mesh go. (approximate) the mesh and the clip stay in their managers
void FreeHand()
{
	auto& hand = IntroSpecialData().hand;
	if (hand.entity != entt::null && Locator::entitiesRegistry::has_value() && Entities().Valid(hand.entity))
	{
		Entities().Destroy(hand.entity);
		Entities().SetDirty();
	}
	hand = HandObject {};
}

/// The draw of this frame (one render flag pending): the animated object drawn at once at `time`; on the other
/// frames the hand is not drawn
void ShowHand(bool drawn, int32_t time)
{
	auto& hand = IntroSpecialData().hand;
	if (hand.entity == entt::null || !Entities().Valid(hand.entity))
	{
		return;
	}
	auto& registry = Entities();
	const bool hasMesh = registry.AllOf<Mesh>(hand.entity);
	const bool wantMesh = drawn && hand.model != nullptr;
	if (wantMesh && !hasMesh)
	{
		registry.Assign<Mesh>(hand.entity, hand.model->mesh, static_cast<int8_t>(0), static_cast<int8_t>(0));
	}
	else if (!wantMesh && hasMesh)
	{
		registry.Remove<Mesh>(hand.entity);
	}
	auto& transform = registry.Get<Transform>(hand.entity);
	const auto rotation = affine::AngleY(hand.yaw);
	if (transform.position != hand.position || transform.rotation != rotation || transform.scale.x != hand.scale)
	{
		transform.position = hand.position;
		transform.rotation = rotation;
		transform.scale = glm::vec3(hand.scale);
		registry.SetDirty();
	}
	registry.Get<SkeletalAnimation>(hand.entity).time = static_cast<float>(time);
}

/// The light's Z object for this frame, keyed at its head as it is now
void QueueLight()
{
	auto& state = IntroSpecialData();
	state.lightQueued = true;
	state.lightKey = state.beam->head;
}
} // namespace

void Play(int32_t special)
{
	auto& intro = IntroSpecialData();
	switch (special)
	{
	case 0:
		intro.state = 0;
		intro.material = true; // the misc0 material, once
		// a new light; a light still there is dropped without its delete, as in the original
		intro.beam = light::Create(k_SonSpot, k_LightDirection, CrtRandom);
		intro.followSon = false;
		return;
	case 1: // state 0 only
		if (intro.state != 0)
		{
			return;
		}
		script_camera::SetDebugCameraFocus(k_SonSpot);
		intro.cameraFollow = true;
		script_camera::SetDebugCameraMode(2);
		if (intro.beam.has_value()) // (approximate) the original starts a null light too
		{
			script_camera::SetDebugCameraPosition(light::Start(*intro.beam));
		}
		return;
	case 2: // state 0 only
		if (intro.state != 0)
		{
			return;
		}
		intro.cameraFollow = false;
		script_camera::SetDebugCameraMode(0);
		return;
	case 4:
	{
		const bool had = intro.hand.entity != entt::null;
		if (!had)
		{
			CreateHand();
		}
		// else the original frees the hand's clip
		SetClip(LoadClip("hand_intro.anm"));
		intro.hand.time = 0;
		SetPosition(k_HandSpot, PutDownYaw(), k_HandScale);
		intro.state = 4;
		return;
	}
	case 5: // state 4 only
		if (intro.state != 4)
		{
			return;
		}
		if (intro.playing) // the jump to 2379 ms, the Son let go
		{
			intro.hand.time = k_SeekMs;
			intro.followSon = false;
			return;
		}
		intro.playing = true;
		intro.followSon = true;
		return;
	default: // 3, 6 and above: not here (the script's special table)
		return;
	}
}

void Update(uint32_t milliseconds)
{
	auto& intro = IntroSpecialData();
	intro.lightQueued = false;
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	switch (intro.state)
	{
	case 0: // the light falls
		if (!intro.beam.has_value())
		{
			break;
		}
		if (intro.beam->state == light::State::Done) // deleted
		{
			intro.beam.reset();
			break;
		}
		QueueLight();
		if (intro.beam->state == light::State::Hold) // landed last frame, the pick-up hand comes
		{
			intro.state = 12;
		}
		if (intro.cameraFollow)
		{
			script_camera::SetDebugCameraFocus(k_SonSpot);
		}
		break;
	case 4: // the put-down hand
	{
		ShowHand(true, intro.hand.time);
		if (!intro.playing)
		{
			break;
		}
		const auto* clip = Clip(intro.hand.clip);
		bool wrapped = false;
		const int32_t next = clip != nullptr
		                         ? AdvanceTime(intro.hand.time, milliseconds, clip->GetDurationMs(), clip->IsLooping(), wrapped)
		                         : intro.hand.time;
		if (wrapped) // all of it goes (not followSon)
		{
			FreeHand();
			intro.playing = false;
			intro.state = -1;
			break;
		}
		intro.hand.time = next;
		break;
	}
	case 12: // the pick-up hand
	{
		if (intro.hand.entity == entt::null)
		{
			intro.pickUpWait = 0; // (and one more field that is never read)
			CreateHand();
			SetClip(LoadClip("hand_intro2.anm"));
			SetPosition(k_SonSpot, intro.pickUpYaw, k_HandScale); // written again below
		}
		if (intro.beam.has_value())
		{
			if (intro.beam->state == light::State::Done) // the light goes, the clip may run
			{
				intro.beam.reset();
				intro.pickUpWait = k_PickUpGo;
			}
			else
			{
				QueueLight();
			}
		}
		if (intro.pickUpWait != 0)
		{
			const auto* clip = Clip(intro.hand.clip);
			bool wrapped = false;
			const int32_t next =
			    clip != nullptr ? AdvanceTime(intro.hand.time, milliseconds, clip->GetDurationMs(), clip->IsLooping(), wrapped)
			                    : intro.hand.time;
			if (wrapped)
			{
				intro.pickUpClipFinished = true; // (hand_intro2.anm is one shot: never)
			}
			else
			{
				intro.hand.time = next;
			}
		}
		SetPosition(k_PickUpSpot, k_PickUpYaw, k_HandScale);
		ShowHand(true, intro.hand.time);
		break;
	}
	default:
		break;
	}
	if (intro.state != 4 && intro.state != 12)
	{
		ShowHand(false, intro.hand.time); // drawn only by states 4 and 12 (state 0 keeps an old hand undrawn)
	}
	// the light's Z object is drawn in the main view's queue: its callback runs once this frame. Its CRT Random draws
	// happen here: (approximate, pending) the original draws in the Z drain, after the clouds, night lights and smoke;
	// to move to the end of Renderer::PreDraw
	if (intro.lightQueued)
	{
		light::Draw(*intro.beam, milliseconds, intro.lightTimer, CrtRandom, intro.lightFrame);
		if (intro.lightFrame.cameraPosition.has_value())
		{
			script_camera::SetDebugCameraPosition(*intro.lightFrame.cameraPosition);
		}
	}
}

std::optional<glm::vec3> Grip()
{
	auto& intro = IntroSpecialData();
	if (!intro.followSon)
	{
		return std::nullopt;
	}
	const auto& hand = intro.hand;
	// a hand, playing, its time <= 1817, its mesh with an EBone block
	if (hand.entity != entt::null && intro.playing && hand.model != nullptr && hand.model->host.has_value())
	{
		const auto& meshes = Locator::resources::value().GetMeshes();
		const auto* clip = Clip(hand.clip);
		if (clip != nullptr && meshes.Contains(hand.model->mesh))
		{
			const auto model = affine::Model(hand.position, affine::AngleY(hand.yaw), glm::vec3(hand.scale));
			const auto& host = *hand.model->host;
			if (const auto point =
			        GripPoint(*meshes.Handle(hand.model->mesh), *clip, hand.time, model, host.matrices.at(0), host.bones.at(0));
			    point.has_value())
			{
				intro.grip = point;
			}
		}
	}
	return intro.grip;
}

void FillFrame(graphics::IntroLightOverlay& out)
{
	auto& state = IntroSpecialData();
	out.active = state.lightQueued;
	out.sprites.clear();
	if (!out.active)
	{
		return;
	}
	out.keyPoint = state.lightKey;
	out.depthAlways = state.lightFrame.depthAlways;
	out.sprites = state.lightFrame.drawn;
}

bool HasPickUpClipFinished()
{
	return IntroSpecialData().pickUpClipFinished;
}

void ReleaseAll()
{
	auto& intro = IntroSpecialData();
	intro.beam.reset();
	FreeHand(); // the object, the mesh, the clip
	intro.material = false;
	intro.playing = false;
	script_camera::SetDebugCameraMode(0);
	intro.state = -1;
	intro.lightQueued = false;
	// kept, as in the original: followSon, cameraFollow, pickUpClipFinished, pickUpWait, the light timer
}

} // namespace openblack::ecs::intro_special
