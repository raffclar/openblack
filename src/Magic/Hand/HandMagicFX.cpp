/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandMagicFX.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <spdlog/spdlog.h>

#include "3D/FrameAnim.h"
#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Audio.h"
#include "Camera/Camera.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/HandFxPart.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ObjectColour.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandMagicStateInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "Enums.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/ArgbColour.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Magic/MagicTables.h"
#include "Particles/PSysManager.h"
#include "Particles/ParticleTypes.h"
#include "Particles/Rules/SurfRevol.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Resources/SharedAssets.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;

namespace
{
/// Power_Up_Band.L3d, the same shared mesh as the worship icons' power-up band
constexpr auto k_BandMesh = resources::shared_assets::k_PowerUpBandMesh;

// The hand effect's constants
constexpr float k_BandScale = 10.0f;
constexpr float k_BandSpin = 12.0f;   ///< rad/s, x (1 + 0.2 index)
constexpr float k_BandOffset = 10.0f; ///< Along the root bone's z (the forearm)
constexpr float k_BandStep = 40.0f;   ///< Per index
constexpr float k_ChargeDurationFrom = 3.5f;
constexpr float k_ChargeDurationTo = 1.0f;
constexpr float k_ChargeIntervalFrom = 6.0f;
constexpr float k_ChargeIntervalTo = 0.3f;
constexpr uint8_t k_TemporaryAlpha1 = 120;
constexpr uint8_t k_TemporaryAlpha0 = 20;
constexpr uint8_t k_PermanentAlpha1 = 130;
constexpr uint8_t k_PermanentAlpha0 = 20;
constexpr float k_BandDuration = 0.85f;
constexpr float k_FlyScale = 4.0f; ///< The distance in front of the camera (inferred; no near + 0.2 clamp)
constexpr float k_FlyShrink = 0.5f;
constexpr float k_GlowAlpha = 0.8f;  ///< The glow's target alpha
constexpr float k_GlowRate = -20.0f; ///< Frames per second
constexpr int k_GlowFrames = 32;
constexpr float k_DelayedStart = 2.4f; ///< The delay of delayed bands, in seconds
constexpr int k_MaxPermanentBands = 5;

/// One power-up ring on the hand
struct Band
{
	entt::entity entity {entt::null}; ///< The band's mesh
	int index {0};
	float angle {0.0f};
	float time {0.0f};
	float start {0.0f};
	float duration {0.0f};
	uint8_t alpha0 {0};
	uint8_t alpha1 {0};
	bool permanent {false};
	bool done {false};
	bool reverse {false};
};

struct HandFxState
{
	std::vector<Band> permanent; ///< The newest first
	std::vector<Band> temporary; ///< The newest first
	float glowAlpha {0.0f};
	float glowFrame {0.0f};
	float chargeTimer {0.0f};
	bool wasCharging {false};
	bool charging {false};
	glm::vec2 glowUv {0.0f};
	// The in-hand effect
	uint32_t inHandEffect {0};
	entt::entity inHandSeed {entt::null};
	bool inHandAtBone {false};
};

/// The bands, the glow and the in-hand effect (Locator::handMagicState)
HandFxState& Fx()
{
	if (!Locator::handMagicState::has_value())
	{
		std::fputs("magic::hand_fx: no hand magic state in the locator (Locator::handMagicState)\n", stderr);
		std::abort();
	}
	return Locator::handMagicState::value().Get<HandFxState>();
}

bool LoadBandMesh()
{
	return resources::shared_assets::LoadPowerUpBand("Hand FX") != resources::shared_assets::LoadResult::Failed;
}

/// The hand effect's own id as a sound channel owner: one hand FX
audio::Owner HandFxOwner()
{
	static const uint32_t s_Id = audio::NewObjectId();
	return audio::Owner::Object(s_Id);
}

/// The band's entity, invisible until it starts
Band MakeBand(int index, float start, bool permanent, float duration, uint8_t alpha0, uint8_t alpha1, bool reverse)
{
	Band band {
	    .index = index,
	    .start = start,
	    .duration = duration,
	    .alpha0 = alpha0,
	    .alpha1 = alpha1,
	    .permanent = permanent,
	    .reverse = reverse,
	};
	if (LoadBandMesh() && Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		band.entity = registry.Create();
		registry.Assign<Transform>(band.entity, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Mesh>(band.entity, k_BandMesh.value(), static_cast<int8_t>(0), static_cast<int8_t>(0));
		registry.Assign<Alpha>(band.entity, 0.0f);
		// Every draw: the local player's colour (openblack: PLAYER_ONE, inferred) with the band's alpha
		// (components::Alpha, DrawBand); the specular colour is 0 for every band
		const uint32_t rgb = psys::surf_revol::PlayerColour(static_cast<int>(PlayerNames::PLAYER_ONE));
		registry.Assign<ObjectColour>(band.entity, ObjectColour {{static_cast<uint8_t>(argb_colour::Red(rgb)),
		                                                          static_cast<uint8_t>(argb_colour::Green(rgb)),
		                                                          static_cast<uint8_t>(argb_colour::Blue(rgb))}});
		// No projected shadow falls on the band (RenderingSystem's ReceivesDynamicShadow leaves HandFxPart out in
		// local/shaders), and it casts no shadow
		registry.Assign<HandFxPart>(band.entity);
		registry.SetDirty();
	}
	return band;
}

void DestroyBand(Band& band)
{
	if (band.entity != entt::null && Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		if (registry.Valid(band.entity))
		{
			registry.Destroy(band.entity);
			registry.SetDirty();
		}
	}
	band.entity = entt::null;
}

/// A permanent band (index = count + 1), at the head
void AddPermanentBand(float start)
{
	auto band = MakeBand(static_cast<int>(Fx().permanent.size()) + 1, start, true, k_BandDuration, k_PermanentAlpha0,
	                     k_PermanentAlpha1, false);
	Fx().permanent.insert(Fx().permanent.begin(), band);
}

/// A temporary band (index = count), at the head
void AddTemporaryBand(float start)
{
	auto band = MakeBand(static_cast<int>(Fx().temporary.size()), start, false, k_BandDuration, k_TemporaryAlpha0,
	                     k_TemporaryAlpha1, false);
	Fx().temporary.insert(Fx().temporary.begin(), band);
}

/// A charge band of charge c: duration lerp(3.5, 1, c), alpha lerp(3, 5, c) -> lerp(15, 50, c)
void AddChargeBand(float charge)
{
	const float duration = k_ChargeDurationFrom + (k_ChargeDurationTo - k_ChargeDurationFrom) * charge;
	const auto alpha0 = static_cast<uint8_t>(static_cast<int>(3.0f + 2.0f * charge));
	const auto alpha1 = static_cast<uint8_t>(static_cast<int>(15.0f + 35.0f * charge));
	auto band = MakeBand(static_cast<int>(Fx().temporary.size()), 0.0f, false, duration, alpha0, alpha1, false);
	Fx().temporary.insert(Fx().temporary.begin(), band);
}

/// The hand's root bone in the world
glm::mat4 HandBoneMatrix()
{
	if (!Locator::handSystem::has_value())
	{
		return glm::mat4(1.0f);
	}
	const auto& hand = Locator::handSystem::value();
	glm::mat4 bone(1.0f);
	if (const auto* bones = hand.GetBoneMatrices(); bones != nullptr && !bones->empty())
	{
		bone = (*bones)[0];
	}
	return hand.GetHandMatrix() * bone;
}

/// The camera's world matrix (inferred) with the band 4 m in front, at half its size. The handedness
/// (right = up x forward) and the column order are also inferred: they decide the side the bands fly from.
glm::mat4 CameraFlyMatrix()
{
	if (!Locator::camera::has_value())
	{
		return glm::mat4(1.0f);
	}
	const auto& camera = Locator::camera::value();
	const auto forward = glm::normalize(camera.GetForward());
	const auto up = glm::normalize(camera.GetUp());
	const auto right = glm::normalize(glm::cross(up, forward));
	glm::mat4 world(glm::vec4(right, 0.0f), glm::vec4(up, 0.0f), glm::vec4(forward, 0.0f), glm::vec4(camera.GetOrigin(), 1.0f));
	return world * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, k_FlyScale)) *
	       glm::scale(glm::mat4(1.0f), glm::vec3(k_FlyShrink));
}

void SetTransform(entt::entity entity, const glm::mat4& matrix, float alpha)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (entity == entt::null || !registry.Valid(entity))
	{
		return;
	}
	auto& transform = registry.Get<Transform>(entity);
	const float scale = glm::length(glm::vec3(matrix[0]));
	transform.position = glm::vec3(matrix[3]);
	transform.scale = glm::vec3(scale);
	transform.rotation = scale > 1e-6f ? glm::mat3(matrix) / scale : glm::mat3(1.0f);
	registry.Get<Alpha>(entity).value = alpha;
}

/// Spins once grown, flies from the camera (the fly matrix) onto the hand's root bone
/// (a permanent band lerps the matrices, a temporary one slerps), alpha lerp(alpha0, alpha1, f)
void DrawBand(Band& band, float dt, const glm::mat4& bone, const glm::mat4& fly)
{
	band.time += dt;
	const float t = band.time - band.start;
	if (t <= 0.0f)
	{
		SetTransform(band.entity, glm::mat4(1.0f), 0.0f);
		return;
	}
	band.angle = std::fmod(band.angle + (1.0f + 0.2f * static_cast<float>(band.index)) * k_BandSpin * dt, glm::two_pi<float>());
	float f = t / band.duration;
	if (f > 1.0f)
	{
		band.done = true;
	}
	if (band.reverse)
	{
		f = 1.0f - f;
	}
	f = std::clamp(f, 0.0f, 1.0f);
	const float alpha =
	    (static_cast<float>(band.alpha0) + (static_cast<float>(band.alpha1) - static_cast<float>(band.alpha0)) * f) / 255.0f;
	// The local matrix is 10 I with the translation (0, 0, 10 + 40 index): the band sits on the root bone's own Z
	// axis (the forearm), like a bracelet. Once it has arrived (f >= 1) every row's (x, y) turns by the angle about
	// that Z ((x, y) -> (c x + s y, c y - s x), c rounded to a float; the translation's (x, y) stays 0) =
	// TurnRows(2). Then local * bone (rows) = bone * local
	glm::mat3 rows(k_BandScale);
	if (f >= 1.0f)
	{
		const auto c = static_cast<double>(static_cast<float>(std::cos(static_cast<double>(band.angle))));
		affine::TurnRows(rows, 2, c, std::sin(static_cast<double>(band.angle)));
	}
	glm::mat4 local(rows);
	local[3] = glm::vec4(0.0f, 0.0f, k_BandOffset + static_cast<float>(band.index) * k_BandStep, 1.0f);
	const glm::mat4 onHand = bone * local;
	if (f >= 1.0f)
	{
		SetTransform(band.entity, onHand, alpha);
		return;
	}
	glm::mat4 result;
	if (band.permanent)
	{
		result = fly + (onHand - fly) * f;
	}
	else
	{
		// The rotations slerped, the translations and the scales lerped
		const float s0 = glm::length(glm::vec3(fly[0]));
		const float s1 = glm::length(glm::vec3(onHand[0]));
		const auto q0 = glm::quat_cast(glm::mat3(fly) / std::max(s0, 1e-6f));
		const auto q1 = glm::quat_cast(glm::mat3(onHand) / std::max(s1, 1e-6f));
		const auto rotation = glm::mat4_cast(glm::slerp(q0, q1, f));
		const glm::vec3 position = glm::mix(glm::vec3(fly[3]), glm::vec3(onHand[3]), f);
		result =
		    glm::translate(glm::mat4(1.0f), position) * rotation * glm::scale(glm::mat4(1.0f), glm::vec3(s0 + (s1 - s0) * f));
	}
	SetTransform(band.entity, result, alpha);
}

const ecs::components::SpellSeed* InHandSeed()
{
	if (Fx().inHandSeed == entt::null || !Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	auto& registry = Locator::entitiesRegistry::value();
	return ecs::IsAvailable(Fx().inHandSeed) ? registry.TryGet<const ecs::components::SpellSeed>(Fx().inHandSeed) : nullptr;
}
} // namespace

void hand_fx::RemoveAllPermanentBands()
{
	for (auto& band : Fx().permanent)
	{
		DestroyBand(band);
	}
	Fx().permanent.clear();
}

void hand_fx::RemoveHandSpellVisuals()
{
	// LH_SAMPLE_G_SHAKEHAND_01 from the InGame bank, owned by the hand effect, not 3D; then one temporary band
	// going back: alpha 5 -> 50 reversed, 1 s
	{
		audio::PlayOptions options;
		options.sample = {audio::Bank(audio::SfxBank::InGame), 0x77};
		options.owner = HandFxOwner();
		options.is3D = false;
		audio::PlaySoundEffect(options);
	}
	auto band = MakeBand(static_cast<int>(Fx().temporary.size()), 0.0f, false, k_ChargeDurationTo, 5, 50, true);
	Fx().temporary.insert(Fx().temporary.begin(), band);
}

void hand_fx::AddSpellToHandVisuals(bool delayed)
{
	const float base = delayed ? k_DelayedStart : 0.0f;
	for (int i = 1; i <= 5; ++i)
	{
		AddTemporaryBand(static_cast<float>(i) * 0.1f + base);
	}
	// G_SpellPowerUpBand: no owner, mode 3, no loops, not 3D, InGame bank
	audio::PlaySoundEffect(audio::Owner::None(), 0x23, 3, 0, false, false, audio::SfxBank::InGame);
}

void hand_fx::SetPowerUpLevel(int level, bool delayed)
{
	const int difference = level - PowerUpLevel();
	if (difference > 0)
	{
		for (int i = 0; i < difference && PowerUpLevel() != k_MaxPermanentBands; ++i)
		{
			AddPermanentBand(delayed ? k_DelayedStart : 0.0f);
		}
	}
	else
	{
		for (int i = 0; i < -difference && !Fx().permanent.empty(); ++i)
		{
			// The newest one goes
			DestroyBand(Fx().permanent.front());
			Fx().permanent.erase(Fx().permanent.begin());
		}
	}
}

int hand_fx::PowerUpLevel()
{
	return static_cast<int>(Fx().permanent.size());
}

void hand_fx::StartTribalPowerRing(int /*tribe*/)
{
	// The tribal power column (the tribe's name spinning): not drawn
}

void hand_fx::StopTribalPowerRing() {}

void hand_fx::ReleaseOrCreateTribalPowerRing() {}

void hand_fx::Update(float seconds)
{
	auto& s = Fx();
	auto& registry = Locator::entitiesRegistry::value();
	// the glow: a Magic / MagicLiving object (info class 3 / 10) or a spell seed in the hand
	// TODO(hand): the info class 3 / 10 test (only the spell seed test is ported)
	s.glowAlpha = 0.0f;
	if (Locator::handSystem::has_value())
	{
		if (const auto held = Locator::handSystem::value().GetHeldObject();
		    held && ecs::IsAvailable(*held) && registry.AllOf<ecs::components::SpellSeed>(*held))
		{
			s.glowAlpha = k_GlowAlpha;
		}
	}
	// the charge bands while one of the player's icons charges for this hand
	s.wasCharging = s.charging;
	const auto* icons = gestures::GetIconProvider();
	s.charging = icons != nullptr && icons->AnyIconChargingForHand();
	const bool started = !s.wasCharging && s.charging;
	// (StopImmersion(10) on the way out, StartImmersion(10) while charging: force feedback, not ported)
	if (s.charging)
	{
		s.chargeTimer += seconds;
		const float charge = std::clamp(icons->MaxChargeFraction(), 0.0f, 1.0f); // clamped
		if (started || s.chargeTimer >= k_ChargeIntervalFrom + (k_ChargeIntervalTo - k_ChargeIntervalFrom) * charge)
		{
			s.chargeTimer = 0.0f;
			AddChargeBand(charge);
		}
	}
	// the flowing texture's frame: += dt x -20 in [0, 64), the cell (frame % 32) of an 8 x 4 atlas; the frame is
	// rounded to the nearest, not truncated (frame_anim::HandFlowFrame)
	if (s.glowAlpha > 0.01f)
	{
		s.glowUv = graphics::frame_anim::HandFlowFrame(s.glowFrame, seconds, k_GlowRate, k_GlowFrames);
	}
	// the bands on the hand's root bone: the permanent ones, then the temporary ones (a finished one goes)
	const auto bone = HandBoneMatrix();
	const auto fly = CameraFlyMatrix();
	for (auto& band : s.permanent)
	{
		DrawBand(band, seconds, bone, fly);
	}
	for (auto it = s.temporary.begin(); it != s.temporary.end();)
	{
		if (it->done)
		{
			DestroyBand(*it);
			it = s.temporary.erase(it);
			continue;
		}
		DrawBand(*it, seconds, bone, fly);
		++it;
	}
}

hand_fx::Glow hand_fx::GetGlow()
{
	return {Fx().glowAlpha, Fx().glowUv};
}

void hand_fx::CreateInHandEffect(entt::entity seed)
{
	ReleaseInHandEffect();
	auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::IsAvailable(seed) || !registry.AllOf<ecs::components::SpellSeed>(seed))
	{
		return;
	}
	const auto& component = registry.Get<const ecs::components::SpellSeed>(seed);
	const auto& info = seed::InfoOf(component);
	// the power-up level's particleTypeInHand
	const auto type = MagicInfoForPowerUpLevel(Locator::infoConstants::value(), info, component.powerUp).particleTypeInHand;
	Fx().inHandSeed = seed;
	Fx().inHandAtBone = info.attachInHandEffectToBone == 1; // (inferred: read, but the bone is not applied yet)
	const auto file = psys::ParticleTypeFile(type);
	if (file.empty())
	{
		return;
	}
	glm::vec3 handPos(0.0f);
	if (Locator::handSystem::has_value())
	{
		handPos = glm::vec3(Locator::handSystem::value().GetHandMatrix()[3]);
	}
	// at the hand's origin, with the seed's player; stepped by the hand
	Fx().inHandEffect = psys::manager::StartForSpell(std::string(file), handPos, glm::vec3(0.0f), 1.0f, nullptr);
	psys::manager::SetPerFrame(Fx().inHandEffect);
	// drawn at once inside the hand's Z-sorted object
	psys::manager::SetDrawPath(Fx().inHandEffect, psys::manager::DrawPath::Immediate);
	if (auto* effect = psys::manager::Find(Fx().inHandEffect); effect != nullptr)
	{
		effect->SetPlayer(static_cast<int>(component.creator.player));
	}
}

void hand_fx::ReleaseInHandEffect()
{
	if (Fx().inHandEffect != 0)
	{
		psys::manager::Delete(Fx().inHandEffect);
	}
	Fx().inHandEffect = 0;
	Fx().inHandSeed = entt::null;
}

void hand_fx::UpdateInHandEffect(float milliseconds)
{
	auto& s = Fx();
	if (s.inHandEffect == 0)
	{
		return;
	}
	const auto* seed = InHandSeed();
	auto* effect = psys::manager::Find(s.inHandEffect);
	if (effect == nullptr || !Locator::handSystem::has_value())
	{
		s.inHandEffect = 0;
		return;
	}
	const auto& hand = Locator::handSystem::value();
	psys::ProcessInfo info;
	// the hand bone's position with attachInHandEffectToBone (which bone is unverified), else the hand's origin.
	// (inferred: inHandAtBone is ignored, the hand origin is always used)
	info.handPos = glm::vec3(hand.GetHandMatrix()[3]);
	info.interfacePos = info.handPos;
	info.power = seed != nullptr ? seed->psysPower : 1.0f; // the seed's PSys power
	info.enabled = true;
	effect->SetMagnitude(hand.GetHandScale());
	// a step of max(1, the game time step) ms; drawn only once the seed is ready (in the hand pass)
	if (seed != nullptr && !seed->ready)
	{
		return; // (inferred) not stepped either until it is drawn
	}
	if (!psys::manager::ProcessForSpell(s.inHandEffect, info, std::max(1.0f, milliseconds) * 0.001f))
	{
		s.inHandEffect = 0; // finished
	}
}

void hand_fx::Reset()
{
	RemoveAllPermanentBands();
	for (auto& band : Fx().temporary)
	{
		DestroyBand(band);
	}
	Fx() = HandFxState {};
}
