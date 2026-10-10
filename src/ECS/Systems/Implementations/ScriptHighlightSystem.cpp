/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ScriptHighlightSystem.h"

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "3D/AllMeshes.h"
#include "ECS/Components/HiddenByState.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "GameScriptHighlightWorld.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace rules = openblack::ecs::script_highlights;

namespace
{
constexpr float k_SecondsPerMillisecond = 0.001f;
/// A highlight is made at its full size, facing north
constexpr float k_Scale = 1.0f;
} // namespace

ScriptHighlightSystem::ScriptHighlightSystem()
    : ScriptHighlightSystem(std::make_unique<GameScriptHighlightWorld>())
{
}

ScriptHighlightSystem::ScriptHighlightSystem(std::unique_ptr<rules::ScriptHighlightWorldInterface> world)
    : _world(std::move(world))
{
	_connections.emplace_back(
	    _world->Entities().OnDestroy<ScriptHighlight>().connect<&ScriptHighlightSystem::OnHighlightGone>(*this));
}

ScriptHighlightSystem::~ScriptHighlightSystem() = default;

void ScriptHighlightSystem::OnHighlightGone(entt::registry& registry, entt::entity entity)
{
	// Its effects end with it; its glow goes at the next frame
	const auto& highlight = registry.get<ScriptHighlight>(entity);
	for (const auto effect : {highlight.glints, highlight.activeEffect})
	{
		if (effect != 0)
		{
			_world->DeleteEffect(effect);
		}
	}
	if (_tipShownBy == entity)
	{
		_tipShownBy = entt::null;
	}
}

entt::entity ScriptHighlightSystem::Create(uint32_t kind, glm::vec3 at, uint32_t challenge)
{
	const auto info = _world->InfoOf(kind);
	if (!info.has_value())
	{
		return entt::null;
	}
	auto& registry = _world->Entities();
	const auto entity = registry.Create();
	auto& highlight = registry.Assign<ScriptHighlight>(entity);
	highlight.kind = static_cast<HighlightInfo>(kind);
	highlight.scriptId = challenge;
	// It stands on the land where it is made, facing north, until its first turn finds what it stands on
	const auto ground = glm::vec3(at.x, _world->LandHeight({at.x, at.z}), at.z);
	registry.Assign<Transform>(entity, ground, glm::mat3(1.0f), glm::vec3(k_Scale));
	registry.Assign<Mesh>(entity, resources::HashIdentifier(info->normal), static_cast<int8_t>(0), static_cast<int8_t>(1));
	// A sign whose tip has been read already shows as started
	if (rules::IsTipSign(highlight.kind) && _tipsRead.Has(highlight.scriptId, highlight.category))
	{
		SetActive(entity, true);
	}
	ShowModel(entity, registry.Get<ScriptHighlight>(entity));
	if (rules::SendsSparks(highlight.kind))
	{
		registry.Get<ScriptHighlight>(entity).sparks = rules::MakeSparks(
		    [this](float x) { return _world->LocalFloatRandom(x); }, [this](uint32_t n) { return _world->LocalRandom(n); });
	}
	// Its glints, as long as it stands, and stepped as it is drawn
	if (info->glints != ParticleType::None)
	{
		auto& made = registry.Get<ScriptHighlight>(entity);
		made.glints = _world->StartEffect(info->glints, ground);
		if (made.glints != 0)
		{
			_world->TargetEffect(made.glints, entity);
		}
	}
	registry.SetDirty();
	return entity;
}

void ScriptHighlightSystem::SetProperties(entt::entity highlight, uint32_t text, uint32_t category)
{
	auto* found = _world->Entities().TryGet<ScriptHighlight>(highlight);
	if (found != nullptr)
	{
		found->scriptId = text;
		found->category = category;
	}
}

void ScriptHighlightSystem::ShowModel(entt::entity entity, const ScriptHighlight& highlight)
{
	const auto info = _world->InfoOf(static_cast<uint32_t>(highlight.kind));
	auto* mesh = _world->Entities().TryGet<Mesh>(entity);
	if (!info.has_value() || mesh == nullptr)
	{
		return;
	}
	// A model the game doesn't have is drawn as the first of its models
	auto shown = highlight.active ? info->active : info->normal;
	if (!_world->BoxOf(shown).has_value())
	{
		shown = MeshId::Dummy;
	}
	mesh->id = resources::HashIdentifier(shown);
	_world->Entities().SetDirty();
}

void ScriptHighlightSystem::SetActive(entt::entity entity, bool active)
{
	auto& registry = _world->Entities();
	auto* highlight = registry.TryGet<ScriptHighlight>(entity);
	if (highlight == nullptr)
	{
		return;
	}
	highlight->active = active;
	// Its active effect starts again each time it starts, and ends as it stops
	if (highlight->activeEffect != 0)
	{
		_world->DeleteEffect(highlight->activeEffect);
		highlight->activeEffect = 0;
	}
	if (active)
	{
		if (!rules::IsTipSign(highlight->kind))
		{
			_world->PlaySound(rules::k_ScrollStartedSound);
		}
		const auto info = _world->InfoOf(static_cast<uint32_t>(highlight->kind));
		if (info.has_value() && info->activeEffect != ParticleType::None)
		{
			highlight->activeEffect = _world->StartEffect(info->activeEffect, StandingPoint(entity, *highlight));
		}
	}
	ShowModel(entity, *highlight);
}

void ScriptHighlightSystem::SetDrawHeight(entt::entity entity, float height)
{
	auto* highlight = _world->Entities().TryGet<ScriptHighlight>(entity);
	if (highlight != nullptr)
	{
		highlight->drawHeight = height;
		highlight->heightAbove = height;
	}
}

bool ScriptHighlightSystem::Tap(entt::entity entity, bool byThisPlayer)
{
	auto* highlight = _world->Entities().TryGet<ScriptHighlight>(entity);
	if (highlight == nullptr || !rules::ValidToTap(highlight->kind, highlight->scriptId, highlight->active))
	{
		return false;
	}
	const auto outcome = rules::Tap(highlight->kind, highlight->scriptId, byThisPlayer);
	if (outcome.helpEvent.has_value())
	{
		_world->HelpEvent(*outcome.helpEvent);
	}
	if (!outcome.starts)
	{
		return true;
	}
	SetActive(entity, true);
	if (outcome.signSound)
	{
		_world->PlaySound(rules::k_SignTappedSound);
	}
	if (outcome.showsTip)
	{
		// The bubble shows the sign's tip, or, tapped again, shows none. The first tip the player ever reads has the
		// advisors explain the signs.
		if (_tipShownBy == entity)
		{
			_tipShownBy = entt::null;
			_world->HideTip();
		}
		else
		{
			if (_tipsRead.Empty())
			{
				_world->StartHelpScript(rules::k_FirstTipScript);
			}
			_tipsRead.Add(highlight->scriptId, highlight->category);
			_tipShownBy = entity;
			_world->ShowTip(entity, highlight->scriptId, highlight->category);
		}
	}
	if (outcome.replaysChallenge)
	{
		_world->ReplayChallenge(highlight->scriptId);
	}
	return true;
}

glm::vec3 ScriptHighlightSystem::StandingPoint(entt::entity entity, const ScriptHighlight& highlight) const
{
	const auto& at = _world->Entities().Get<const Transform>(entity).position;
	return {at.x, _world->LandHeight({at.x, at.z}) + highlight.heightAbove, at.z};
}

float ScriptHighlightSystem::ReachAcross(entt::entity entity) const
{
	const auto& registry = _world->Entities();
	const auto* mesh = registry.TryGet<const Mesh>(entity);
	const auto* highlight = registry.TryGet<const ScriptHighlight>(entity);
	if (mesh == nullptr || highlight == nullptr)
	{
		return 0.0f;
	}
	const auto info = _world->InfoOf(static_cast<uint32_t>(highlight->kind));
	if (!info.has_value())
	{
		return 0.0f;
	}
	const auto box = _world->BoxOf(highlight->active ? info->active : info->normal);
	return box.has_value() ? std::max(box->halfSize.x, box->halfSize.z) * k_Scale : 0.0f;
}

void ScriptHighlightSystem::ProcessTurn()
{
	rules::StepPulse(_pulse, _world->MillisecondsPerTurn());
	auto& registry = _world->Entities();
	const auto turn = _world->Turn();
	std::vector<entt::entity> highlights;
	registry.Each<ScriptHighlight>(
	    [&highlights](entt::entity entity, const ScriptHighlight& /*unused*/) { highlights.push_back(entity); });
	for (const auto entity : highlights)
	{
		auto& highlight = registry.Get<ScriptHighlight>(entity);
		// It stands at the height a script gave it, else on top of the highest thing under it that keeps still
		if (highlight.drawHeight.has_value())
		{
			highlight.heightAbove = *highlight.drawHeight;
		}
		else
		{
			const auto& at = registry.Get<const Transform>(entity).position;
			const auto things = _world->ThingsBelow(entity, at);
			highlight.heightAbove = rules::HeightOnThings(rules::InCell({at.x, at.z}), ReachAcross(entity), things);
		}
		// A sign checks now and then whether its tip has been read meanwhile
		if (!highlight.active && rules::IsTipSign(highlight.kind) &&
		    rules::ChecksTipThisTurn(entt::to_integral(entity), turn) && _tipsRead.Has(highlight.scriptId, highlight.category))
		{
			SetActive(entity, true);
		}
	}
}

bool ScriptHighlightSystem::Drawn(const ScriptHighlight& highlight) const
{
	return rules::IsTipSign(highlight.kind) || _world->ScrollsDrawn();
}

void ScriptHighlightSystem::UpdateFrame(float frameMilliseconds, float /*turnFraction*/, glm::vec3 camera)
{
	auto& registry = _world->Entities();
	RemoveLoneGlows();
	bool moved = false;
	registry.Each<ScriptHighlight>(
	    [this, &registry, &moved, frameMilliseconds, camera](entt::entity entity, ScriptHighlight& highlight) {
		    const bool drawn = Drawn(highlight);
		    const bool hidden = registry.AllOf<HiddenByState>(entity);
		    if (drawn == hidden)
		    {
			    if (drawn)
			    {
				    registry.Remove<HiddenByState>(entity);
			    }
			    else
			    {
				    registry.Assign<HiddenByState>(entity);
			    }
			    registry.SetDirty();
		    }
		    if (drawn)
		    {
			    UpdateHighlight(entity, highlight, frameMilliseconds, camera);
			    moved = true;
		    }
		    UpdateGlow(entity, highlight, drawn && rules::GlowShown(highlight.kind, highlight.active), camera);
		    UpdateSparks(entity, drawn, frameMilliseconds);
	    });
	if (moved)
	{
		registry.SetDirty();
	}
}

void ScriptHighlightSystem::UpdateHighlight(entt::entity entity, ScriptHighlight& highlight, float frameMilliseconds,
                                            glm::vec3 camera)
{
	auto& registry = _world->Entities();
	auto& transform = registry.Get<Transform>(entity);
	const auto point = StandingPoint(entity, highlight);
	// A started sign turns to face the camera; anything else spins
	highlight.yAngle = rules::IsTipSign(highlight.kind) && highlight.active ? rules::FacingAngle(camera, point)
	                                                                        : rules::Spin(highlight.yAngle, frameMilliseconds);
	const float scale = rules::DrawnScale(highlight.kind, k_Scale, glm::distance(point, camera));
	transform.position = point;
	transform.rotation = glm::mat3(glm::eulerAngleY(-highlight.yAngle));
	transform.scale = glm::vec3(scale);

	// Where its model's middle is drawn, and how far the model reaches from it
	const auto info = _world->InfoOf(static_cast<uint32_t>(highlight.kind));
	const auto box = info.has_value() ? _world->BoxOf(highlight.active ? info->active : info->normal) : std::nullopt;
	if (box.has_value())
	{
		highlight.centre = point + (transform.rotation * (box->centre * scale));
		highlight.radius = glm::length(box->halfSize) * scale;
	}
	else
	{
		highlight.centre = point;
		highlight.radius = 0.0f;
	}

	// Its effects step as it is drawn; the active one plays about the model's middle
	const float seconds = frameMilliseconds * k_SecondsPerMillisecond;
	if (highlight.glints != 0)
	{
		_world->StepEffect(highlight.glints, seconds);
	}
	if (highlight.activeEffect != 0)
	{
		_world->MoveEffect(highlight.activeEffect, highlight.centre);
		_world->StepEffect(highlight.activeEffect, seconds);
	}
}

void ScriptHighlightSystem::UpdateGlow(entt::entity entity, const ScriptHighlight& highlight, bool shown, glm::vec3 camera)
{
	auto& registry = _world->Entities();
	if (!shown || highlight.radius <= 0.0f)
	{
		if (highlight.glow != entt::null && registry.Valid(highlight.glow))
		{
			registry.Remove<Sprite>(highlight.glow);
		}
		return;
	}
	const auto look = _world->GlowLook();
	if (!look.has_value())
	{
		return;
	}
	auto glow = highlight.glow;
	if (glow == entt::null || !registry.Valid(glow))
	{
		glow = registry.Create();
		registry.Assign<ScriptHighlightGlow>(glow, entity);
		registry.Assign<Transform>(glow, highlight.centre, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Get<ScriptHighlight>(entity).glow = glow;
	}
	const float halfSize = rules::GlowHalfSize(highlight.radius);
	auto& transform = registry.Get<Transform>(glow);
	transform.position = rules::GlowPosition(highlight.centre, camera, highlight.radius);
	transform.scale = glm::vec3(halfSize, halfSize, 1.0f);
	auto* sprite = registry.TryGet<Sprite>(glow);
	if (sprite == nullptr)
	{
		sprite = &registry.Assign<Sprite>(glow, *look);
	}
	sprite->tint = glm::vec4(1.0f, 1.0f, 1.0f, static_cast<float>(rules::GlowAlpha(highlight.kind)) / 255.0f);
}

void ScriptHighlightSystem::UpdateSparks(entt::entity entity, bool drawn, float frameMilliseconds)
{
	auto& registry = _world->Entities();
	auto& highlight = registry.Get<ScriptHighlight>(entity);
	if (!highlight.sparks.has_value())
	{
		return;
	}
	if (drawn)
	{
		// The frame's game time, in whole milliseconds as the game counts it
		rules::StepSparks(*highlight.sparks, static_cast<uint32_t>(std::max(frameMilliseconds, 0.0f)),
		                  [this](float x) { return _world->LocalFloatRandom(x); });
	}
	// They rise from the top of the model
	const auto from = highlight.centre + glm::vec3(0.0f, highlight.radius, 0.0f);
	for (size_t i = 0; i < rules::k_Sparks; ++i)
	{
		auto& sprite = highlight.sparkSprites.at(i);
		const auto look = drawn ? rules::LookOf(*highlight.sparks, i, from) : std::nullopt;
		const auto picture = look.has_value() ? _world->SparkLook(look->picture) : std::nullopt;
		if (!picture.has_value())
		{
			if (sprite != entt::null && registry.Valid(sprite))
			{
				registry.Remove<Sprite>(sprite);
			}
			continue;
		}
		if (sprite == entt::null || !registry.Valid(sprite))
		{
			sprite = registry.Create();
			registry.Assign<ScriptHighlightGlow>(sprite, entity);
			registry.Assign<Transform>(sprite, from, glm::mat3(1.0f), glm::vec3(1.0f));
		}
		const auto made = sprite;
		auto& transform = registry.Get<Transform>(made);
		transform.position = look->position;
		// Turned about the line to the camera, as it faces it
		transform.rotation = glm::mat3(glm::eulerAngleZ(look->angle));
		transform.scale = glm::vec3(look->halfSize, look->halfSize, 1.0f);
		auto& drawnSprite = registry.AllOf<Sprite>(made) ? registry.Get<Sprite>(made) : registry.Assign<Sprite>(made, *picture);
		drawnSprite = *picture;
		drawnSprite.tint = glm::vec4(1.0f, 1.0f, 1.0f, static_cast<float>(look->alpha) / 255.0f);
	}
}

void ScriptHighlightSystem::RemoveLoneGlows()
{
	auto& registry = _world->Entities();
	std::vector<entt::entity> lone;
	registry.Each<const ScriptHighlightGlow>([&registry, &lone](entt::entity entity, const ScriptHighlightGlow& glow) {
		if (!registry.Valid(glow.highlight) || !registry.AllOf<ScriptHighlight>(glow.highlight))
		{
			lone.push_back(entity);
		}
	});
	for (const auto entity : lone)
	{
		registry.Destroy(entity);
	}
}

void ScriptHighlightSystem::Reset()
{
	_pulse = {};
	_tipShownBy = entt::null;
}
