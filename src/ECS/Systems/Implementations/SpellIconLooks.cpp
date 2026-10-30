/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <vector>

#include "3D/L3DMesh.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/DrawnOnScreen.h"
#include "ECS/PlayerColour.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"
#include "Magic/IconChargeLooks.h"
#include "Particles/LightSheet.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "SpellIconSystem.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace looks = openblack::magic::icon_charge_looks;

namespace
{
constexpr float k_SecondsPerMillisecond = 0.001f;
constexpr uint32_t k_RgbMask = 0xFFFFFFu;
constexpr uint32_t k_AlphaShift = 24;

/// The height of the box of a thing's model, none without the model
[[nodiscard]] float ModelHeight(const Mesh& mesh)
{
	if (!Locator::resources::has_value())
	{
		return 0.0f;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	return meshes.Contains(mesh.id) ? meshes.Handle(mesh.id)->GetBoundingBox().Size().y : 0.0f;
}
} // namespace

void SpellIconSystem::UpdateChargeLooks(int32_t gameMilliseconds, bool landDrawn)
{
	auto& registry = Locator::entitiesRegistry::value();
	_pulses.clear();
	// An icon that went loses its look
	std::erase_if(_looks, [&registry](auto& entry) {
		const bool gone = !registry.Valid(entry.first) || !registry.AllOf<WorshipIcon>(entry.first);
		if (gone && entry.second.ring != nullptr)
		{
			entry.second.ring->SetHidden(true);
		}
		return gone;
	});
	// A frame that does not draw the land draws no icons, and their clock stops
	if (!landDrawn)
	{
		for (auto& [icon, look] : _looks)
		{
			if (look.ring != nullptr)
			{
				look.ring->SetHidden(true);
			}
		}
		return;
	}
	registry.Each<const WorshipIcon, const Transform, const Mesh>(
	    [this, &registry, gameMilliseconds](entt::entity entity, const WorshipIcon& icon, const Transform&, const Mesh&) {
		    if (ecs::DrawnOnScreen(registry, entity))
		    {
			    ShowCharge(entity, icon, gameMilliseconds);
		    }
		    else if (auto found = _looks.find(entity); found != _looks.end() && found->second.ring != nullptr)
		    {
			    found->second.ring->SetHidden(true);
		    }
	    });
	// The clock moves on after the icons used it
	_decayClock = looks::Advance(_decayClock, gameMilliseconds);
}

void SpellIconSystem::ShowCharge(entt::entity entity, const WorshipIcon& icon, int32_t gameMilliseconds)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* site = registry.Valid(icon.site) ? registry.TryGet<const WorshipSite>(icon.site) : nullptr;
	if (site == nullptr)
	{
		return;
	}
	const auto& transform = registry.Get<const Transform>(entity);
	const auto model = PlacedMatrix(transform.position, transform.rotation, transform.scale);
	const auto playerRgb = player_colour::Of(site->player) & k_RgbMask;
	auto& look = _looks[entity];

	// The game draws the model with the shine of the last frame that drew it, then works out the new shine
	if (look.shownTint != look.tint)
	{
		// The model is drawn in a different colour, so the draw uploads the things again
		look.shownTint = look.tint;
		registry.SetDirty();
	}
	const looks::IconState state {
	    .seedHeld = HeldSeedOf(icon) != entt::null,
	    .store = icon.store,
	    .required = Required(icon),
	    .charging = icon.charging,
	};
	const auto shown = looks::ShownCharge(state);
	const float chargeSeconds = Locator::time::has_value()
	                                ? looks::ChargeSeconds(Locator::time::value().GetTurn(), icon.chargeStartTurn,
	                                                       Locator::time::value().GetTurnFraction())
	                                : 0.0f;
	look.tint = looks::Tint(playerRgb, shown, icon.charging, _decayClock.phase, chargeSeconds);

	// The ring of light stands where the icon stood when the first frame drew it
	if (look.ring == nullptr)
	{
		look.ring = std::make_shared<particles::LightSheet>();
		look.ring->Start(looks::RingPoints(model), playerRgb, looks::k_RingHeight, looks::k_RingShiftSeconds);
		look.ring->SetSpread(looks::k_RingSpread);
		look.ring->SetOwnClock(true);
	}
	look.ring->SetRgb(playerRgb);
	look.ring->SetStrength(shown.fraction);
	look.ring->SetHidden(false);
	look.ring->Update(static_cast<float>(gameMilliseconds) * k_SecondsPerMillisecond);
	// The particles keep the sheet until a new land or a load clears their list, so it is given to them again each time
	if (Locator::particleRuleSystem::has_value())
	{
		Locator::particleRuleSystem::value().AddLightSheet(look.ring);
	}

	// While the icon charges, the pulse model shrinks into it once in each round of the clock
	if (!icon.charging)
	{
		return;
	}
	const auto share = looks::PulseShare(_decayClock.phase);
	if (!share.has_value())
	{
		return;
	}
	_pulses.push_back({
	    .model = looks::PulseModel(model, ModelHeight(registry.Get<const Mesh>(entity)), *share),
	    .argb = playerRgb | (static_cast<uint32_t>(looks::PulseAlpha(*share)) << k_AlphaShift),
	});
}

uint32_t SpellIconSystem::ShownTint(entt::entity icon) const
{
	const auto found = _looks.find(icon);
	return found != _looks.end() ? found->second.shownTint : 0;
}

std::vector<SpellIconSystemInterface::ChargePulse> SpellIconSystem::ChargePulses() const
{
	return _pulses;
}
