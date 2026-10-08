/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptHighlight.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <numeric>
#include <string>

#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Audio/Audio.h"
#include "CHLApi.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ScriptTypes.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "GameClock.h"
#include "Help/HelpProfile.h"
#include "Help/HelpSystem.h"
#include "Help/ScriptControl.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Particles/PSys.h"
#include "Particles/PSysManager.h"
#include "Particles/ParticleTypes.h"
#include "Resources/ResourceManager.h"

namespace openblack::ecs::script_highlight
{
using components::Mesh;
using components::Transform;
using HighlightComponent = components::ScriptHighlight;

namespace
{
/// The turning highlight's radians per game ms (pi / 1000)
constexpr float k_SpinPerMs = 0.00314159f;
/// 1 / 2 pi and 2 pi, at the original's precision
constexpr float k_InvTwoPi = 0.159155f;
constexpr float k_TwoPi = 6.28319f;
/// The pulse's radians per second of turn (with 0.001 for ms to seconds)
constexpr float k_PulsePerSecond = 5.0f;
/// The draw's distance bounds and 1 / 30
constexpr uint32_t k_NearDistance = 10;
constexpr uint32_t k_FarDistance = 30;
constexpr float k_InvFarDistance = 0.0333333f;
/// The sprite stands 1.4 radii towards the eye, 2 radii wide (at least 0.0001); the click sphere is r + 0.5
constexpr float k_SpriteTowardsEye = 1.4f;
constexpr float k_SpriteSizePerRadius = 2.0f;
constexpr float k_MinSpriteSize = 0.0001f;
constexpr float k_ClickMargin = 0.5f;
/// The glow's Y row x 3
constexpr float k_GlowStretch = 3.0f;
/// 48 entries a category
constexpr uint32_t k_DidYouKnowListSize = 0x30;
constexpr size_t k_DidYouKnowCategoryCount = 5;

/// What this module keeps between calls (Locator::scriptState)
struct ScriptHighlightState
{
	Pulse pulse {};
	/// The list, head first
	std::vector<entt::entity> list {};
	std::array<std::vector<uint32_t>, k_DidYouKnowCategoryCount> didYouKnowRead {};
};

ScriptHighlightState& ScriptHighlightData()
{
	return openblack::Locator::scriptState::value().Get<ScriptHighlightState>();
}

ecs::Registry& Reg()
{
	return Locator::entitiesRegistry::value();
}

const GScriptHighlightInfo* InfoRow(uint32_t index)
{
	if (!Locator::infoConstants::has_value() || index >= k_InfoCount)
	{
		return nullptr;
	}
	return &Locator::infoConstants::value().scriptHighlight.at(index);
}

HighlightComponent* Get(entt::entity thing)
{
	auto& registry = Reg();
	return thing != entt::null && registry.Valid(thing) ? registry.TryGet<HighlightComponent>(thing) : nullptr;
}

float GroundAt(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

glm::vec3 WorldOf(const HighlightComponent& highlight)
{
	return {highlight.x, GroundAt(highlight.x, highlight.z) + highlight.altitude, highlight.z};
}

/// The info's normal or active mesh, as SetActivated and Create choose; the first mesh when out of range
void SetMeshOf(entt::entity thing, const HighlightComponent& highlight)
{
	const auto* info = InfoRow(highlight.infoIndex);
	if (info == nullptr)
	{
		return;
	}
	const MeshId mesh = highlight.active ? info->active : info->normal;
	auto& registry = Reg();
	const auto id = resources::HashIdentifier(mesh);
	if (auto* current = registry.TryGet<Mesh>(thing))
	{
		current->id = id;
	}
	else
	{
		registry.Assign<Mesh>(thing, id, static_cast<int8_t>(0), static_cast<int8_t>(0));
	}
	registry.SetDirty();
}

/// A particle effect at the point (scale 1.0): 0 when the row has no particle type or the type no file
uint32_t StartEffect(ParticleType type, const glm::vec3& point)
{
	if (type == ParticleType::None)
	{
		return 0; // PARTICLE_TYPE 0, none
	}
	const auto file = psys::ParticleTypeFile(type);
	if (file.empty())
	{
		return 0;
	}
	return psys::manager::Start(std::string(file), point, 1.0f, game_random::psys::NetGameType::Local);
}

void DeleteEffect(uint32_t& id)
{
	if (id != 0)
	{
		psys::manager::Delete(id); // at once
		id = 0;
	}
}

/// One highlight's game turn
void Process(entt::entity thing, HighlightComponent& highlight)
{
	// the altitude = the draw height, or the top of what it stands on
	if (highlight.drawHeight)
	{
		highlight.altitude = *highlight.drawHeight;
	}
	else
	{
		const auto coords = map_coords::FromMetres(glm::vec2(highlight.x, highlight.z));
		highlight.altitude = map_cells::TallestOverlapping(coords, thing, coords, object::Get2DRadius(thing), true);
	}
	// a did-you-know not lit yet, every 8 turns by (turn + its unique id) & 7, is lit once its text has been read.
	// (approximate) the unique id is the creation index (openblack has no object heap)
	if (!highlight.active && IsDidYouKnowInfo(highlight.infoIndex))
	{
		const auto unique = static_cast<uint32_t>(object_index::Of(thing));
		if (((game_clock::Turn() + unique) & 7u) == 0 && did_you_know_read::IsRead(highlight.scriptId, highlight.category))
		{
			SetActivated(thing, true);
		}
	}
}
} // namespace

// ---- Pure rules -----------------------------------------------------------------------------------------------------

uint8_t SpriteAlpha(uint32_t index)
{
	switch (index)
	{
	case 1:
		return 0x32;
	case 2:
		return 0x96;
	case 3:
		return 0x64;
	default:
		return 0;
	}
}

float DistanceScale(float scale, float distanceToCamera, bool didYouKnow)
{
	auto distance = static_cast<uint32_t>(static_cast<int64_t>(distanceToCamera)); // truncated, then unsigned compares
	if (distance > k_FarDistance)
	{
		distance = k_FarDistance;
	}
	else if (distance < k_NearDistance)
	{
		distance = k_NearDistance;
	}
	if (didYouKnow)
	{
		return scale;
	}
	// the distance x scale x (1 / 30)
	return static_cast<float>(distance) * scale * k_InvFarDistance;
}

float SpinAngle(float angle, uint32_t frameMs)
{
	const float turned = angle + static_cast<float>(frameMs) * k_SpinPerMs;
	const auto turns = static_cast<int32_t>(turned * k_InvTwoPi); // truncated
	return turned - static_cast<float>(turns) * k_TwoPi;
}

bool SavesGameWhenClicked(uint32_t scriptId)
{
	return scriptId == 0x38 || scriptId == 0x3B || scriptId == 0x3C || scriptId == 0x3D;
}

bool ValidToTap(bool didYouKnow, bool active, uint32_t scriptId)
{
	if (didYouKnow)
	{
		return scriptId != 0;
	}
	return active && scriptId != 0;
}

uint32_t OverwriteTapToolTip(ObjectType infoType)
{
	return static_cast<int>(infoType) == 1 ? 0xEF2u : 0u;
}

void StepPulse(Pulse& pulse, uint32_t msPerTurn)
{
	pulse.phase += static_cast<float>(msPerTurn) * k_PulsePerSecond * 0.001f;
	if (pulse.phase > k_TwoPi) // only when above
	{
		pulse.phase -= k_TwoPi;
	}
	pulse.previous = pulse.value;
	pulse.value = (1.0f - std::cos(pulse.phase)) * 0.5f;
}

float ActivePulse(const Pulse& pulse, float turnFraction)
{
	float t = (pulse.value - pulse.previous) * turnFraction + pulse.previous;
	if (!(t > 0.0f)) // not above 0
	{
		t = 0.0f;
	}
	else if (!(t < 1.0f)) // not below 1
	{
		t = 1.0f;
	}
	return t * 0.6f + 0.4f;
}

uint32_t ActiveGlowArgb(uint32_t index, float activePulse)
{
	if (index == static_cast<uint32_t>(Info::Silver))
	{
		return 0x14B4DCFFu;
	}
	const auto whole = static_cast<uint8_t>(static_cast<int32_t>(activePulse)); // truncated, the low byte
	const auto alpha = static_cast<uint8_t>(whole * 0x50);                      // an 8-bit product
	return (static_cast<uint32_t>(alpha) << 24) | 0xFFFF00u;
}

// ---- The read did-you-knows -----------------------------------------------------------------------------------------

bool did_you_know_read::IsRead(uint32_t text, DykCategory category)
{
	const auto c = static_cast<size_t>(category);
	if (c >= k_DidYouKnowCategoryCount) // not a category: not read
	{
		return false;
	}
	const auto& list = ScriptHighlightData().didYouKnowRead.at(c);
	return std::find(list.begin(), list.end(), text) != list.end();
}

void did_you_know_read::MarkRead(uint32_t text, DykCategory category)
{
	auto& state = ScriptHighlightData();
	const auto c = static_cast<size_t>(category);
	if (c >= k_DidYouKnowCategoryCount || IsRead(text, category) || state.didYouKnowRead.at(c).size() >= k_DidYouKnowListSize)
	{
		return; // (inferred)
	}
	state.didYouKnowRead.at(c).push_back(text);
}

uint32_t did_you_know_read::Total()
{
	const auto& lists = ScriptHighlightData().didYouKnowRead;
	return std::accumulate(lists.begin(), lists.end(), uint32_t {0},
	                       [](uint32_t total, const auto& list) { return total + static_cast<uint32_t>(list.size()); });
}

void did_you_know_read::Clear()
{
	for (auto& list : ScriptHighlightData().didYouKnowRead)
	{
		list.clear();
	}
}

// ---- The highlights -------------------------------------------------------------------------------------------------

entt::entity Create(const glm::vec3& position, uint32_t infoIndex, uint32_t scriptId, float yAngle, float scale)
{
	auto& state = ScriptHighlightData();
	const auto* info = InfoRow(infoIndex);
	if (infoIndex >= k_InfoCount)
	{
		return entt::null;
	}
	auto& registry = Reg();
	const auto thing = registry.Create();
	// the next creation index
	object_index::Assign(thing);
	auto& highlight = registry.Assign<HighlightComponent>(thing);
	highlight.infoIndex = infoIndex;
	highlight.x = position.x;
	highlight.z = position.z;
	highlight.altitude = 0.0f; // the script's point as map coordinates: y - ground (inferred: 0 for these)
	if (Locator::terrainSystem::has_value())
	{
		highlight.altitude = position.y - GroundAt(position.x, position.z);
	}
	highlight.scale = scale;
	highlight.drawAngle = yAngle;
	highlight.scriptId = scriptId;
	state.list.insert(state.list.begin(), thing); // the head
	registry.Assign<Transform>(thing, WorldOf(highlight), affine::AngleY(yAngle), glm::vec3(scale));

	// the 3D object, and the head of its cell's fixed list
	SetMeshOf(thing, highlight);
	map_cells::InsertMapObject(thing);
	// (pending, renderer) two calls on the 3D object (one only for a scroll); the sprite and the glow object
	// (mesh k_GlowMesh, material properties {1, 0, 0, 1, 1}) are ExtrasOf's; one more 3D object call is not read
	// a did-you-know already read is lit at once
	if (IsDidYouKnowInfo(infoIndex) && did_you_know_read::IsRead(highlight.scriptId, highlight.category))
	{
		SetActivated(thing, true);
	}
	// unless a flag is set (inferred clear for a new object), the info's glints at its point, with this as their
	// target, then its render particle
	auto& made = registry.Get<HighlightComponent>(thing);
	if (info != nullptr)
	{
		made.glintsEffect = StartEffect(info->particleTypeGlints, WorldOf(made));
		if (made.glintsEffect != 0)
		{
			// the draw steps and draws it each frame with the frame's game time
			psys::manager::SetPerFrame(made.glintsEffect);
			psys::manager::SetDrawPath(made.glintsEffect, psys::DrawPath::Sorted);
			if (auto* effect = psys::manager::Find(made.glintsEffect); effect != nullptr)
			{
				effect->AddTarget(thing);
			}
		}
	}
	return thing;
}

bool IsHighlight(entt::entity thing)
{
	return Get(thing) != nullptr;
}

uint32_t InfoIndexOf(entt::entity thing)
{
	const auto* highlight = Get(thing);
	return highlight != nullptr ? highlight->infoIndex : script_type::k_NoSubtype;
}

bool IsDidYouKnow(entt::entity thing)
{
	const auto* highlight = Get(thing);
	return highlight != nullptr && IsDidYouKnowInfo(highlight->infoIndex);
}

bool IsActive(entt::entity thing)
{
	const auto* highlight = Get(thing);
	return highlight != nullptr && highlight->active;
}

uint32_t ScriptIdOf(entt::entity thing)
{
	const auto* highlight = Get(thing);
	return highlight != nullptr ? highlight->scriptId : 0;
}

void SetScriptId(entt::entity thing, uint32_t scriptId, DykCategory category)
{
	if (auto* highlight = Get(thing))
	{
		highlight->scriptId = scriptId;
		highlight->category = category;
	}
}

void SetYPos(entt::entity thing, float height)
{
	auto* highlight = Get(thing);
	if (highlight == nullptr)
	{
		return;
	}
	highlight->drawHeight = height;
	highlight->altitude = height;
	// an object with a 3D object: moved to the ground + altitude, with its angle and scale
	if (auto* transform = Reg().TryGet<Transform>(thing))
	{
		transform->position = WorldOf(*highlight);
		Reg().SetDirty();
	}
}

float GetYPos(entt::entity thing)
{
	const auto* highlight = Get(thing);
	return highlight != nullptr ? highlight->altitude : 0.0f;
}

void SetActivated(entt::entity thing, bool on)
{
	auto* highlight = Get(thing);
	if (highlight == nullptr)
	{
		return;
	}
	highlight->active = on;
	DeleteEffect(highlight->activeEffect);
	if (on)
	{
		if (!IsDidYouKnowInfo(highlight->infoIndex))
		{
			// the sound (bank InGame, owner this, sample 0x86). (pending, audio) the options' mapping to
			// audio::PlaySoundEffect(PlayOptions) is not checked
		}
		if (const auto* info = InfoRow(highlight->infoIndex); info != nullptr)
		{
			highlight->activeEffect = StartEffect(info->particleTypeActive, WorldOf(*highlight));
		}
	}
	SetMeshOf(thing, *highlight);
}

void ProcessHighlights()
{
	auto& scriptHighlightState = ScriptHighlightData();
	StepPulse(scriptHighlightState.pulse, game_clock::MsPerTurn());
	// from the head, the next read before Process; only the available ones.
	// (inferred) every highlight in the list is available: openblack has no other state for them
	const auto list = scriptHighlightState.list; // Process may light one (SetActivated), never delete one
	for (const auto thing : list)
	{
		if (auto* highlight = Get(thing))
		{
			Process(thing, *highlight);
		}
	}
}

void UpdateFrame(uint32_t frameMs, const glm::vec3& eye)
{
	auto& registry = Reg();
	bool moved = false;
	for (const auto thing : ScriptHighlightData().list)
	{
		auto* highlight = Get(thing);
		auto* transform = highlight != nullptr ? registry.TryGet<Transform>(thing) : nullptr;
		if (transform == nullptr)
		{
			continue;
		}
		const bool didYouKnow = IsDidYouKnowInfo(highlight->infoIndex);
		// hidden, or the scripts' SET_DRAW_HIGHLIGHT 0 for a scroll: no Draw at
		// all, the 3D object stays where it was (inferred: openblack hides the mesh)
		const bool drawn =
		    highlight->hidden == 0 && (help::script_control::GetCameraControl().drawHighlight != 0 || didYouKnow);
		if (!drawn)
		{
			if (registry.AllOf<Mesh>(thing))
			{
				registry.Remove<Mesh>(thing);
				moved = true;
			}
			continue;
		}
		if (!registry.AllOf<Mesh>(thing))
		{
			SetMeshOf(thing, *highlight);
		}
		const auto point = WorldOf(*highlight);
		// a lit did-you-know faces the eye, the others turn
		if (highlight->active && didYouKnow)
		{
			highlight->drawAngle = graphics::billboard::YawToEyeAngle(point, eye);
			const auto turns = static_cast<int32_t>(highlight->drawAngle * k_InvTwoPi);
			highlight->drawAngle -= static_cast<float>(turns) * k_TwoPi;
		}
		else
		{
			highlight->drawAngle = SpinAngle(highlight->drawAngle, frameMs);
		}
		// the scale by the camera's distance
		const float scale = DistanceScale(highlight->scale, gutils::GetDistance(eye, point), didYouKnow);
		transform->position = point;
		transform->rotation = affine::AngleY(highlight->drawAngle);
		transform->scale = glm::vec3(scale);
		moved = true;
	}
	if (moved)
	{
		registry.SetDirty();
	}
}

DrawExtras ExtrasOf(entt::entity thing, const glm::vec3& eye)
{
	DrawExtras extras;
	const auto* highlight = Get(thing);
	const auto* transform = highlight != nullptr ? Reg().TryGet<const Transform>(thing) : nullptr;
	if (transform == nullptr || !Reg().AllOf<Mesh>(thing))
	{
		return extras;
	}
	extras.drawn = true;
	const bool didYouKnow = IsDidYouKnowInfo(highlight->infoIndex);
	// the mesh's centre through the 3D object's matrix and |M x (the mesh's half extents)|.
	// (approximate) ObjectMetrics exports no box centre: the centre is taken at half the height over the position
	const auto half = object::ObjectHalfExtents(thing).value_or(glm::vec3(0.0f));
	const glm::mat3 matrix = transform->rotation * glm::mat3(transform->scale.x);
	extras.radius = glm::length(matrix * half);
	extras.centre = transform->position + glm::vec3(0.0f, half.y * transform->scale.y, 0.0f);
	// the glow of an active scroll: YawToEye's rows, the Y row x 3, at the centre
	if (highlight->active && !didYouKnow)
	{
		glm::mat3 axes = graphics::billboard::YawToEye(extras.centre, eye);
		axes[1] *= k_GlowStretch;
		glm::mat4 glow(axes);
		glow[3] = glm::vec4(extras.centre, 1.0f);
		extras.glow = glow;
		const auto pulse = ActivePulse(ScriptHighlightData().pulse, game_clock::TurnFraction());
		extras.glowArgb = ActiveGlowArgb(highlight->infoIndex, pulse);
	}
	// a scroll is clicked through an invisible sphere
	if (!didYouKnow)
	{
		extras.clickRadius = extras.radius + k_ClickMargin;
	}
	// the sprite, not for a lit did-you-know
	if (!(didYouKnow && highlight->active))
	{
		const glm::vec3 towards = eye - extras.centre;
		const float length = glm::length(towards);
		const glm::vec3 direction = length > 0.0f ? towards / length : glm::vec3(0.0f);
		graphics::billboard::Sprite sprite {
		    .position = extras.centre + direction * (extras.radius * k_SpriteTowardsEye),
		    .size = std::max(k_SpriteSizePerRadius * extras.radius, k_MinSpriteSize),
		    .argb = (static_cast<uint32_t>(SpriteAlpha(highlight->infoIndex)) << 24) | 0xFFFFFFu,
		    .cell = 0,
		};
		extras.sprite = sprite;
	}
	return extras;
}

bool InterfaceValidToTap(entt::entity thing)
{
	const auto* highlight = Get(thing);
	return highlight != nullptr && ValidToTap(IsDidYouKnowInfo(highlight->infoIndex), highlight->active, highlight->scriptId);
}

bool InterfaceTap(entt::entity thing, bool byLocalPlayer)
{
	auto* highlight = Get(thing);
	if (highlight == nullptr)
	{
		return true;
	}
	// the help event (a script id ? 0x22 : 0x23) for the local player's tap. (inferred) byLocalPlayer stands for the
	// original's check that the tapping player is the local one
	if (byLocalPlayer)
	{
		help_profile::Trigger(highlight->scriptId != 0 ? help_profile::Event::Reminder : help_profile::Event::ScriptActivate);
	}
	if (highlight->scriptId == 0)
	{
		return true;
	}
	SetActivated(thing, true);
	if (IsDidYouKnowInfo(highlight->infoIndex))
	{
		// the local interface taps: the sound, bank InGame, sample 0xAD, the other options at their defaults
		if (byLocalPlayer)
		{
			audio::PlayOptions options;
			options.sample = {audio::Bank(audio::SfxBank::InGame), 0xAD};
			audio::PlaySoundEffect(options);
		}
		// the help bubble shows its text (script id and category)
		if (auto* helpSystem = help::Get(); helpSystem != nullptr)
		{
			const auto same = helpSystem->GetBubbleThing() == entt::to_integral(thing);
			if (!same && did_you_know_read::Total() == 0) // the first one ever
			{
				help::script_control::StartScriptIfNoDialogue(*helpSystem, "FirstDYKExplained", chlapi::ScriptVm());
			}
			helpSystem->SetBubbleProperties(entt::to_integral(thing), highlight->scriptId);
			if (same)
			{
				return true; // closed, nothing marked
			}
		}
		did_you_know_read::MarkRead(highlight->scriptId, highlight->category);
		return true;
	}
	// (pending) a help script holding the dialogue -> the help scripts stop; no dialogue owner -> the temple's
	// challenge room starts the script id
	return true;
}

void OnToBeDeleted(entt::entity thing)
{
	auto& state = ScriptHighlightData();
	auto* highlight = Get(thing);
	if (highlight == nullptr)
	{
		return;
	}
	state.list.erase(std::remove(state.list.begin(), state.list.end(), thing), state.list.end());
	DeleteEffect(highlight->glintsEffect);
	DeleteEffect(highlight->activeEffect);
}

void OnClearMap()
{
	auto& state = ScriptHighlightData();
	state.pulse = Pulse {};
	state.list.clear(); // openblack: the map clear deleted them
}

const std::vector<entt::entity>& All()
{
	return ScriptHighlightData().list;
}

} // namespace openblack::ecs::script_highlight
