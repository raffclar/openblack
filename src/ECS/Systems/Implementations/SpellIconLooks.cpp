/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <optional>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

#include "3D/L3DMesh.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/DrawnOnScreen.h"
#include "ECS/PlayerColour.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/IconChargeLooks.h"
#include "Magic/MagicTables.h"
#include "Magic/MiracleVisuals.h"
#include "Particles/DrawnEffectSteps.h"
#include "Particles/LightSheet.h"
#include "Particles/ParticleEffect.h"
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
/// The seed over a village centre's stone hand or the icon of a worship site has its full size
constexpr float k_HandSeedScale = 1.0f;
/// The stone hand is white, so the effect of the holder of its seed has the full alpha
constexpr uint8_t k_HandHolderAlpha = 0xFF;

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

void SpellIconSystem::UpdateHolderEffects(std::chrono::milliseconds frameRealTime, std::chrono::milliseconds frameGameTime,
                                          bool landDrawn)
{
	const auto step = particles::drawn_steps::StepSeconds(_holderPace, frameRealTime, frameGameTime, landDrawn);
	if (!Locator::particleRuleSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& particles = Locator::particleRuleSystem::value();
	// An icon that went, or that shows another seed, loses the effect of its old seed
	std::erase_if(_holderEffects, [&registry, &particles](const auto& entry) {
		const auto* shown = registry.Valid(entry.first) ? registry.TryGet<const IconSeed>(entry.first) : nullptr;
		if (shown != nullptr && shown->seed == entry.second.seed)
		{
			return false;
		}
		particles.Delete(entry.second.effect);
		return true;
	});
	// The frame draws the village centres and the icons of the worship sites in the order of the land's draw list. Each
	// centre draws its hands in turn.
	std::vector<entt::entity> drawers;
	registry.Each<const CentreHands>([&drawers](entt::entity entity, const CentreHands&) { drawers.push_back(entity); });
	registry.Each<const WorshipIcon, const IconSeed>(
	    [&drawers](entt::entity entity, const WorshipIcon&, const IconSeed&) { drawers.push_back(entity); });
	const auto* drawList = Locator::objectDrawListSystem::has_value() ? &Locator::objectDrawListSystem::value() : nullptr;
	const auto order = drawList != nullptr
	                       ? particles::drawn_steps::DrawOrder(
	                             drawList->GetList(), drawers, drawList->GetView().has_value(),
	                             [&registry, drawList](entt::entity entity) { return drawList->Decides(registry, entity); })
	                       : drawers;
	const auto* sites = Locator::worshipSiteSystem::has_value() ? &Locator::worshipSiteSystem::value() : nullptr;
	for (const auto drawer : order)
	{
		// The icon of a worship site draws its seed only while its site is built and the icon is on the screen. The
		// effect has the alpha of the model of the icon.
		if (const auto* icon = registry.TryGet<const WorshipIcon>(drawer))
		{
			const bool built = sites != nullptr && sites->IsBuilt(icon->site);
			StepHolderEffect(drawer, landDrawn && built && ecs::DrawnOnScreen(registry, drawer), step,
			                 looks::HolderAlpha(ShownTint(drawer)));
			continue;
		}
		// A centre draws its hands only while the list draws it and while it has life
		const auto* life = registry.TryGet<const ObjectLife>(drawer);
		const bool centreDrawn =
		    landDrawn && (drawList == nullptr || drawList->Drawn(registry, drawer)) && (life == nullptr || life->life > 0.0f);
		for (const auto hand : registry.Get<const CentreHands>(drawer).hands)
		{
			if (registry.Valid(hand) && registry.AllOf<IconSeed>(hand))
			{
				StepHolderEffect(hand, centreDrawn && ecs::DrawnOnScreen(registry, hand), step, k_HandHolderAlpha);
			}
		}
	}
}

void SpellIconSystem::StepHolderEffect(entt::entity icon, bool drawn, std::optional<float> step, uint8_t alpha)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& particles = Locator::particleRuleSystem::value();
	const auto& info = Locator::infoConstants::value();
	const auto& shown = registry.Get<const IconSeed>(icon);
	const auto& seed = magic::GetSpellSeedInfo(info, shown.seed);
	if (seed.holderParticle == ParticleType::None)
	{
		return;
	}
	const auto player = world_objects::IconPlayer(registry, icon);
	auto found = _holderEffects.find(icon);
	// A load does not keep the effect, so the icon makes it again
	if (found == _holderEffects.end() || !particles.IsRunning(found->second.effect))
	{
		// The seed of an icon has its full size, so the effect is at the holder's height above the seed's middle
		const auto effect = particles.StartByFrame(
		    seed.holderParticle, shown.middle + glm::vec3(0.0f, seed.holderHeight * k_HandSeedScale, 0.0f), k_HandSeedScale);
		if (effect == ParticleRuleSystemInterface::k_NoEffect)
		{
			return;
		}
		particles.KeepOutOfSaves(effect);
		// The effect acts on the model of the seed. The model has the bat or the dove of the player when the effect starts.
		// Until a frame draws the model, the model stands at the middle of the seed with no turn, at the full size.
		const float alignment = Locator::alignmentSystem::has_value() && player != PlayerNames::NEUTRAL
		                            ? Locator::alignmentSystem::value().GetPlayerAlignment(player)
		                            : 0.0f;
		const HolderTarget target {
		    .model = resources::HashIdentifier(magic::GetSeedGraphicModel(info, shown.seed, alignment)),
		    .placement =
		        glm::translate(glm::mat4(1.0f), shown.middle) * glm::scale(glm::mat4(1.0f), glm::vec3(k_HandSeedScale)),
		};
		particles.AddTarget(effect, icon);
		found =
		    _holderEffects.insert_or_assign(icon, HolderEffect {.effect = effect, .seed = shown.seed, .target = target}).first;
	}
	// It takes the colours of the player of the icon, who can change when another player takes the town
	particles.SetPlayer(found->second.effect, static_cast<int>(player));
	particles.SetShown(found->second.effect, drawn);
	if (!drawn)
	{
		return;
	}
	if (auto* running = particles.Find(found->second.effect); running != nullptr)
	{
		running->SetGlobalAlpha(static_cast<float>(alpha));
	}
	// The effect moves on with the place of the model from the last draw. Then the frame draws the model in its new place.
	if (step.has_value())
	{
		particles.ProcessByFrame(found->second.effect, *step);
	}
	if (seed.useMesh == 0)
	{
		return;
	}
	float size = 1.0f;
	glm::vec3 shape(1.0f);
	if (magic::visuals::IsPhialSeed(shown.seed))
	{
		const auto* spell = magic::GetMagicInfoAs<GMagicCreatureSpellInfo>(info, seed.magicTypes[0]);
		const int receiveType = spell != nullptr ? static_cast<int>(spell->creatureReceiveSpellType) : 0;
		size = magic::visuals::PhialLastDrawSize(receiveType, shown.phialPhase);
		shape = magic::visuals::PhialScale(receiveType, shown.phialPhase);
	}
	found->second.target.placement =
	    magic::visuals::SeedModelPlacement(shown.middle, seed.meshHeight, seed.scale, k_HandSeedScale, shown.spin, size, shape);
}

std::optional<SpellIconSystemInterface::HolderTarget> SpellIconSystem::FindHolderTarget(entt::entity icon) const
{
	const auto found = _holderEffects.find(icon);
	return found != _holderEffects.end() ? std::optional(found->second.target) : std::nullopt;
}
