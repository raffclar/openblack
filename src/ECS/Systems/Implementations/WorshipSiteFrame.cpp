/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <vector>

#include "3D/FieldCrop.h"
#include "Camera/Camera.h"
#include "Common/PortableRotations.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipChants.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/DrawnOnScreen.h"
#include "ECS/NeedFlagRules.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/TownDesireFlags.h"
#include "ECS/WorshipSites.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/WorshipBattery.h"
#include "Particles/LightSheet.h"
#include "Particles/ParticleEffect.h"
#include "WorshipSiteSystem.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace ws = openblack::ecs::worship_site;
namespace looks = openblack::ecs::worship_site_looks;

namespace
{

[[nodiscard]] glm::vec2 Across(const glm::vec3& position)
{
	return {position.x, position.z};
}

} // namespace

void WorshipSiteSystem::UpdateFrame(std::chrono::milliseconds gameTime, std::chrono::milliseconds realTime, bool landDrawn)
{
	auto& registry = _world->Entities();
	// The looks of sites that are gone go too
	std::erase_if(_looks, [&registry](const auto& entry) {
		return !registry.Valid(entry.first) || !registry.AllOf<WorshipSite>(entry.first);
	});
	std::vector<entt::entity> sites;
	registry.Each<const WorshipSite>([&sites](entt::entity site, const WorshipSite&) { sites.push_back(site); });
	if (!landDrawn)
	{
		// Nothing of the land is drawn: the rings wait as they are
		for (const auto site : sites)
		{
			UpdateRing(site, false, 0);
		}
		return;
	}
	const auto gameMilliseconds = static_cast<int32_t>(gameTime.count());
	const auto realMilliseconds = static_cast<int32_t>(realTime.count());
	// The land's things draw first: the flags and the ring of each site
	UpdateNeedFlags(gameMilliseconds);
	for (const auto site : sites)
	{
		UpdateRing(site, IsBuilt(site) && DrawnThisFrame(site), realMilliseconds);
	}
	// Then the miracles draw: each site throbs while it strains, and its motes are as clear as its throb
	auto* particles = Locator::particleSystem::has_value() ? &Locator::particleSystem::value() : nullptr;
	for (const auto site : sites)
	{
		const auto* chants = registry.TryGet<const WorshipChants>(site);
		if (chants == nullptr)
		{
			continue;
		}
		auto& look = _looks[site];
		look.throb = looks::NextThrob(look.throb, chants->strain, gameMilliseconds);
		const auto motes = registry.Get<const WorshipSite>(site).manaPath;
		if (auto* effect = particles != nullptr && motes != 0 ? particles->Find(motes) : nullptr; effect != nullptr)
		{
			effect->SetGlobalAlpha(static_cast<float>(looks::MoteAlpha(chants->strain, look.throb.pulse)));
		}
	}
}

bool WorshipSiteSystem::DrawnThisFrame(entt::entity site) const
{
	return ecs::DrawnOnScreen(_world->Entities(), site);
}

void WorshipSiteSystem::UpdateNeedFlags(int32_t gameMilliseconds)
{
	auto& registry = _world->Entities();
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& rows = Locator::infoConstants::value().showNeeds;
	const auto eye = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : glm::vec3(0.0f);
	const float seconds = looks::FrameSeconds(gameMilliseconds);
	registry.Each<WorshipSiteFlag, Transform>([&](WorshipSiteFlag& flag, Transform& transform) {
		const auto* site = registry.Valid(flag.site) ? registry.TryGet<const WorshipSite>(flag.site) : nullptr;
		const auto* siteTransform = registry.Valid(flag.site) ? registry.TryGet<const Transform>(flag.site) : nullptr;
		// A site shows its flags only while it is built
		if (site == nullptr || siteTransform == nullptr || !IsBuilt(flag.site))
		{
			flag.shown = false;
			return;
		}
		// A flag holds still while its site is not drawn, as the game moves the flags only as it draws the site
		if (!DrawnThisFrame(flag.site))
		{
			return;
		}
		const auto& info = rows.at(flag.need);
		const ecs::need_flag::Row row {
		    .maxNeed = info.maxNeedValue, .showsAfter = info.showNeedGreater, .maxHeight = info.maxHieght};
		const float need = site->needs.at(flag.need);
		// A flag that does not show starts again from the need when it shows
		if (!ecs::need_flag::Shows(need, row))
		{
			flag.level = need;
			flag.speed = 0.0f;
			flag.shown = false;
			return;
		}
		// Flag i flies at point i of the site's model
		const auto point = _world->SitePoint(flag.need);
		if (!point.has_value())
		{
			flag.shown = false;
			return;
		}
		const float fill = ecs::need_flag::Fill(need, row);
		const auto next = field_crop::Advance({.position = flag.level, .speed = flag.speed}, fill, seconds);
		flag.level = next.position;
		flag.speed = next.speed;
		flag.shown = true;
		const auto place = ws::TurnedPoint(Across(siteTransform->position), site->facing, *point);
		const float rise = flag.level * row.maxHeight;
		transform.position = glm::vec3(place.x, _world->LandHeightAt(place) + rise, place.y);
		transform.rotation =
		    glm::mat3(portable_math::EulerAngleY(-ecs::town_desire_flags::FacingAngle(transform.position, eye)));
		transform.scale = glm::vec3(ecs::need_flag::DrawnScale(fill, row));
	});
}

void WorshipSiteSystem::UpdateRing(entt::entity site, bool drawn, int32_t realMilliseconds)
{
	auto& registry = _world->Entities();
	auto& look = _looks[site];
	if (!drawn)
	{
		if (look.ring != nullptr)
		{
			look.ring->SetHidden(true);
		}
		return;
	}
	const auto& component = registry.Get<const WorshipSite>(site);
	const auto* chants = registry.TryGet<const WorshipChants>(site);
	// The ring stands round the middle of the dance
	const auto dance = DancePosition(site);
	if (chants == nullptr || !dance.has_value())
	{
		return;
	}
	const auto centre = *dance;
	if (look.ring == nullptr || look.ringCentre != centre)
	{
		look.ring = std::make_shared<particles::LightSheet>();
		look.ring->Start(looks::RingPoints(centre), 0, looks::k_RingHeight, looks::k_RingShiftSeconds);
		look.ring->SetSpread(looks::k_RingSpread);
		look.ring->SetOwnClock(true);
		look.ringCentre = centre;
		if (Locator::particleSystem::has_value())
		{
			Locator::particleSystem::value().AddLightSheet(look.ring);
		}
	}
	// It is as strong as the site's store of prayer power is full, in its player's colour, throbbing and sliding as the
	// site strains. Then it moves on by the frame's real time.
	const auto rules = _world->ChantRules(component.tribe, component.player);
	const float strength = looks::RingStrength(chants->battery, magic::WorshipMaxBattery(rules, Dancers(site)));
	const auto playerRgb = Player::k_Colours.at(static_cast<size_t>(component.player) & (Player::k_Colours.size() - 1));
	look.ring->SetRgb(looks::RingRgb(playerRgb, chants->strain, look.throb.pulse));
	look.ring->SetStrength(strength);
	look.ring->SetSlideSpeed(looks::RingSlideSpeed(chants->strain));
	look.ring->SetHidden(false);
	look.ring->Update(looks::FrameSeconds(realMilliseconds));
	// The particles keep the sheet until a new land or a load clears their list, so it is given to them again each time
	if (Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().AddLightSheet(look.ring);
	}
}
