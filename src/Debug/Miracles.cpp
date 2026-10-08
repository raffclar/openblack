/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Miracles.h"

#include <cstring>

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <SDL_events.h>
#include <SDL_mouse.h>
#include <fmt/format.h>
#include <imgui.h>

#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerReaction.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/ReactionsSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Chants.h"
#include "Magic/Core/Spell.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Magic/MagicTables.h"
#include "MiraclesCaster.h"

using namespace openblack;
using namespace openblack::debug::gui;
using namespace openblack::debug::miracles;

namespace
{
constexpr float k_MinMultiplier = 0.1f;
constexpr float k_MaxMultiplier = 1.0f;

std::string_view NameOf(const std::array<char, 0x30>& name)
{
	return {name.data(), strnlen(name.data(), name.size())};
}

std::string MagicName(const InfoConstants& info, MagicType type)
{
	if (type == MagicType::None)
	{
		return "none";
	}
	const auto name = NameOf(magic::GetMagicEffectInfo(info, type).debugString);
	return name.empty() ? fmt::format("Magic type {}", static_cast<uint32_t>(type)) : std::string(name);
}

std::string SeedName(const InfoConstants& info, SpellSeedType seed)
{
	if (seed == SpellSeedType::None)
	{
		return "none";
	}
	const auto name = NameOf(magic::GetSpellSeedInfo(info, seed).debugString);
	return name.empty() ? fmt::format("Seed {}", static_cast<int>(seed)) : std::string(name);
}

std::string AbodeName(const InfoConstants& info, AbodeInfo abode)
{
	const auto name = NameOf(info.abode.at(static_cast<size_t>(abode)).debugString);
	return name.empty() ? fmt::format("Abode {}", static_cast<int>(abode)) : std::string(name);
}

std::string_view PlayerName(PlayerNames player)
{
	return k_PlayerNamesStrs.at(static_cast<size_t>(player));
}

/// A left button press or release
bool IsLeftClick(const SDL_Event& event)
{
	return (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && event.button.button == SDL_BUTTON_LEFT;
}

/// The villagers, creatures and animals a click may pick
std::vector<ThingAt> ThingsToPick(const ecs::Registry& registry)
{
	std::vector<ThingAt> things;
	const auto add = [&things](entt::entity entity, const ecs::components::Transform& transform) {
		things.push_back({.entity = entity, .position = transform.position});
	};
	registry.Each<const ecs::components::Villager, const ecs::components::Transform>(
	    [&add](entt::entity entity, const auto&, const auto& transform) { add(entity, transform); });
	registry.Each<const ecs::components::Creature, const ecs::components::Transform>(
	    [&add](entt::entity entity, const auto&, const auto& transform) { add(entity, transform); });
	registry.Each<const ecs::components::Animal, const ecs::components::Transform>(
	    [&add](entt::entity entity, const auto&, const auto& transform) { add(entity, transform); });
	return things;
}
} // namespace

Miracles::Miracles() noexcept
    : Window("Miracles", ImVec2(520.0f, 620.0f))
    , _caster(std::make_unique<GameSpellCaster>())
    , _dispenserCreator(std::make_unique<GameDispenserCreator>())
{
}

void Miracles::Draw() noexcept
{
	if (!Locator::infoConstants::has_value() || !Locator::entitiesRegistry::has_value())
	{
		ImGui::TextUnformatted("No miracles: no info.dat or no land loaded");
		return;
	}
	DrawChoice();
	DrawCast();
	DrawDispenser();
	DrawHand();
	DrawPrayer();
	DrawRunning();
	DrawReactions();
}

void Miracles::DrawChoice() noexcept
{
	const auto& info = Locator::infoConstants::value();
	if (_choice.seed == SpellSeedType::None)
	{
		const auto seeds = CastableSeeds(info);
		if (seeds.empty())
		{
			ImGui::TextUnformatted("The tables have no seed that casts a miracle");
			return;
		}
		_choice.seed = seeds.front();
	}

	ImGui::SeparatorText("Miracle");
	if (ImGui::BeginCombo("Seed", SeedName(info, _choice.seed).c_str()))
	{
		for (const auto seed : CastableSeeds(info))
		{
			if (ImGui::Selectable(SeedName(info, seed).c_str(), seed == _choice.seed))
			{
				_choice.seed = seed;
				_choice.powerUpLevel = KeepLevel(magic::GetSpellSeedInfo(info, seed), _choice.powerUpLevel);
			}
		}
		ImGui::EndCombo();
	}
	if (ImGui::BeginCombo("Power-up", LevelName(_choice.powerUpLevel).c_str()))
	{
		for (const auto level : PowerUpLevels(magic::GetSpellSeedInfo(info, _choice.seed)))
		{
			if (ImGui::Selectable(LevelName(level).c_str(), level == _choice.powerUpLevel))
			{
				_choice.powerUpLevel = level;
			}
		}
		ImGui::EndCombo();
	}
	ImGui::TextDisabled("Casts %s", MagicName(info, MagicTypeOf(info, _choice)).c_str());
	ImGui::SliderFloat("Charge", &_multiplier, k_MinMultiplier, k_MaxMultiplier, "%.2f");

	if (ImGui::BeginCombo("Player", PlayerName(_player).data()))
	{
		for (size_t i = 0; i < k_PlayerNamesStrs.size(); ++i)
		{
			const auto player = static_cast<PlayerNames>(i);
			if (ImGui::Selectable(k_PlayerNamesStrs.at(i).data(), player == _player))
			{
				_player = player;
			}
		}
		ImGui::EndCombo();
	}
}

void Miracles::DrawCast() noexcept
{
	if (_choice.seed == SpellSeedType::None)
	{
		return;
	}
	const auto type = MagicTypeOf(Locator::infoConstants::value(), _choice);
	const auto point = CameraPoint();

	ImGui::SeparatorText("Cast");
	if (ImGui::Button("Where the camera looks"))
	{
		CastAt(point);
	}
	ImGui::TextUnformatted("A left click on the land:");
	auto clickCast = static_cast<int>(_clickCast);
	ImGui::RadioButton("does what the game does", &clickCast, static_cast<int>(ClickCast::Off));
	ImGui::SameLine();
	ImGui::RadioButton("casts there", &clickCast, static_cast<int>(ClickCast::OnLand));
	ImGui::SameLine();
	ImGui::RadioButton("casts on the nearest thing", &clickCast, static_cast<int>(ClickCast::OnNearestThing));
	_clickCast = static_cast<ClickCast>(clickCast);
	ImGui::TextDisabled("Camera focus (%.0f, %.0f): %s", point.x, point.z,
	                    _caster->CanCastAt(type, _player, point) ? "the hand may cast there" : "the hand may not cast there");
	if (!_last.empty())
	{
		ImGui::TextUnformatted(_last.c_str());
	}
}

void Miracles::DrawDispenser() noexcept
{
	if (_choice.seed == SpellSeedType::None)
	{
		return;
	}
	const auto& info = Locator::infoConstants::value();
	ImGui::SeparatorText("Dispenser");
	auto kind = static_cast<int>(_dispenserKind);
	ImGui::RadioButton("one-shot orb", &kind, static_cast<int>(DispenserKind::OneShot));
	ImGui::SameLine();
	ImGui::RadioButton("one-shot in the hand", &kind, static_cast<int>(DispenserKind::OneShotInHand));
	ImGui::SameLine();
	ImGui::RadioButton("permanent dispenser", &kind, static_cast<int>(DispenserKind::Permanent));
	_dispenserKind = static_cast<DispenserKind>(kind);

	if (_dispenserKind == DispenserKind::Permanent)
	{
		const auto abodes = DispenserAbodes(info);
		if (!_dispenserAbode.has_value())
		{
			_dispenserAbode = DefaultDispenserAbode(abodes);
			if (_dispenserAbode.has_value())
			{
				_dispenserPeriod = static_cast<int>(DefaultPeriod(info, *_dispenserAbode));
			}
		}
		if (!_dispenserAbode.has_value())
		{
			ImGui::TextDisabled("The tables have no miracle dispenser");
			return;
		}
		if (ImGui::BeginCombo("Building", AbodeName(info, *_dispenserAbode).c_str()))
		{
			for (const auto abode : abodes)
			{
				if (ImGui::Selectable(AbodeName(info, abode).c_str(), abode == *_dispenserAbode))
				{
					_dispenserAbode = abode;
					_dispenserPeriod = static_cast<int>(DefaultPeriod(info, abode));
				}
			}
			ImGui::EndCombo();
		}
		ImGui::InputInt("Period (turns, 0: inactive)", &_dispenserPeriod);
		_dispenserPeriod = std::max(_dispenserPeriod, 0);
	}

	if (_dispenserKind != DispenserKind::OneShotInHand)
	{
		auto place = static_cast<int>(_dispenserPlace);
		ImGui::RadioButton("where the camera looks", &place, static_cast<int>(DispenserPlace::CameraFocus));
		ImGui::SameLine();
		ImGui::RadioButton("at the next click on the land", &place, static_cast<int>(DispenserPlace::NextClick));
		ImGui::SameLine();
		ImGui::RadioButton("under the hand", &place, static_cast<int>(DispenserPlace::Hand));
		_dispenserPlace = static_cast<DispenserPlace>(place);
	}

	if (ImGui::Button("Create"))
	{
		if (_dispenserKind == DispenserKind::OneShotInHand || _dispenserPlace == DispenserPlace::CameraFocus)
		{
			CreateDispenserAt(CameraPoint());
		}
		else if (_dispenserPlace == DispenserPlace::NextClick)
		{
			_dispenserAtClick = true;
			_last = "Click on the land to place it";
		}
		else if (const auto hand = HandLandPoint())
		{
			CreateDispenserAt(*hand);
		}
		else
		{
			_last = "The hand is not on the land";
		}
	}
	if (_dispenserAtClick)
	{
		ImGui::SameLine();
		if (ImGui::Button("Cancel the click"))
		{
			_dispenserAtClick = false;
			_last.clear();
		}
	}
}

void Miracles::DrawHand() noexcept
{
	ImGui::SeparatorText("The hand's seed");
	const auto& info = Locator::infoConstants::value();
	bool any = false;
	Locator::entitiesRegistry::value().Each<const ecs::components::SpellSeed>(
	    [&info, &any](entt::entity, const ecs::components::SpellSeed& seed) {
		    if (!seed.inInterface)
		    {
			    return;
		    }
		    any = true;
		    ImGui::Text("%s, %s: charge %.0f, multiplier %.2f, %s, %d turns in the hand", SeedName(info, seed.seedType).c_str(),
		                LevelName(seed.powerUp).c_str(), seed.chantStore, seed.castMultiplier,
		                seed.ready ? "ready" : "not ready", seed.turnsInHand);
	    });
	if (!any)
	{
		ImGui::TextDisabled("The hand holds no seed");
	}
	if (const auto* icons = magic::gestures::GetIconProvider(); icons != nullptr && icons->AnyIconChargingForHand())
	{
		ImGui::Text("An icon charges for the hand: %.0f%%", icons->MaxChargeFraction() * 100.0f);
	}
}

void Miracles::DrawPrayer() noexcept
{
	ImGui::SeparatorText("Prayer power");
	bool infinite = _prayer.IsOn();
	if (ImGui::Checkbox("Infinite prayer power at every worship site", &infinite))
	{
		if (infinite)
		{
			_prayer.TurnOn();
		}
		else
		{
			TurnPrayerOff();
		}
	}
	if (_prayer.IsOn())
	{
		ImGui::TextDisabled("%zu sites; stays on with the window closed", _prayer.HeldSites());
	}
}

void Miracles::DrawRunning() noexcept
{
	const auto& info = Locator::infoConstants::value();
	auto& registry = Locator::entitiesRegistry::value();
	if (ImGui::CollapsingHeader("Running miracles", ImGuiTreeNodeFlags_DefaultOpen))
	{
		constexpr auto k_Flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
		if (ImGui::BeginTable("Spells", 6, k_Flags))
		{
			ImGui::TableSetupColumn("Miracle");
			ImGui::TableSetupColumn("Age");
			ImGui::TableSetupColumn("Prayer power");
			ImGui::TableSetupColumn("Strength");
			ImGui::TableSetupColumn("Upkeep");
			ImGui::TableSetupColumn("State");
			ImGui::TableHeadersRow();
			registry.Each<const ecs::components::Spell>([&info](entt::entity entity, const ecs::components::Spell& spell) {
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(MagicName(info, spell.magicType).c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(SpellAge(spell.age, spell.duration).c_str());
				ImGui::TableNextColumn();
				ImGui::Text("%.0f/%.0f", static_cast<double>(spell.chants), static_cast<double>(spell.initialChants));
				ImGui::TableNextColumn();
				ImGui::Text("%.2f", static_cast<double>(magic::GetSpellStrength(entity)));
				ImGui::TableNextColumn();
				ImGui::Text("%.1f", static_cast<double>(magic::ChantContextOf(entity).costToMaintain));
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(SpellState(spell.closedDown).data());
			});
			ImGui::EndTable();
		}
	}

	if (ImGui::CollapsingHeader("Dispensers"))
	{
		constexpr auto k_Flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
		if (ImGui::BeginTable("Dispensers", 3, k_Flags))
		{
			registry.Each<const ecs::components::SpellDispenser>(
			    [&info, &registry](entt::entity, const ecs::components::SpellDispenser& dispenser) {
				    const bool hasOrb = dispenser.oneShot != entt::null && registry.Valid(dispenser.oneShot);
				    ImGui::TableNextRow();
				    ImGui::TableNextColumn();
				    ImGui::TextUnformatted(MagicName(info, dispenser.magicType).c_str());
				    ImGui::TableNextColumn();
				    ImGui::TextUnformatted(DispenserState(hasOrb, dispenser.active).data());
				    ImGui::TableNextColumn();
				    ImGui::Text("%u/%u turns", dispenser.tick, dispenser.period);
			    });
			ImGui::EndTable();
		}
	}

	if (ImGui::CollapsingHeader("Creatures' spells", ImGuiTreeNodeFlags_DefaultOpen))
	{
		registry.Each<const ecs::components::CreatureSpells>(
		    [](entt::entity entity, const ecs::components::CreatureSpells& component) {
			    ImGui::TextUnformatted(CreatureSpellsLine(entt::to_integral(entity), component.spells).c_str());
			    if (component.freeze > 0.0f || component.fizz > 0.0f)
			    {
				    ImGui::TextDisabled("  frozen %.2f, fizzed %.2f", static_cast<double>(component.freeze),
				                        static_cast<double>(component.fizz));
			    }
		    });
	}
}

void Miracles::DrawReactions() noexcept
{
	if (!ImGui::CollapsingHeader("Villagers' reactions", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}
	if (!Locator::reactionsSystem::has_value())
	{
		ImGui::TextDisabled("No reactions");
		return;
	}
	ImGui::Checkbox("Every reaction, not only the miracles'", &_allReactions);
	const auto& registry = Locator::entitiesRegistry::value();
	std::unordered_map<uint32_t, uint32_t> followers;
	registry.Each<const ecs::components::VillagerReactionSlot>(
	    [&followers](entt::entity, const ecs::components::VillagerReactionSlot& slot) { ++followers[slot.reaction]; });
	const auto rows = ReactionRows(
	    Locator::reactionsSystem::value().List(), _allReactions,
	    [&registry](entt::entity initiator) {
		    return registry.Valid(initiator) && registry.AllOf<ecs::components::Spell>(initiator);
	    },
	    [&followers](uint32_t reaction) {
		    const auto found = followers.find(reaction);
		    return found != followers.end() ? found->second : 0u;
	    });
	if (rows.empty())
	{
		ImGui::TextDisabled(_allReactions ? "No reactions" : "No reactions to a miracle");
		return;
	}
	constexpr auto k_Flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
	if (ImGui::BeginTable("Reactions", 5, k_Flags))
	{
		ImGui::TableSetupColumn("Id");
		ImGui::TableSetupColumn("Reaction");
		ImGui::TableSetupColumn("Player");
		ImGui::TableSetupColumn("Radius");
		ImGui::TableSetupColumn("Villagers");
		ImGui::TableHeadersRow();
		for (const auto& row : rows)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("%u", row.id);
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(row.name.data(), row.name.data() + row.name.size());
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(PlayerName(row.player).data());
			ImGui::TableNextColumn();
			ImGui::Text("%.0f", row.radius);
			ImGui::TableNextColumn();
			ImGui::Text("%u", row.followers);
		}
		ImGui::EndTable();
	}
}

void Miracles::CastAt(const Target& target) noexcept
{
	const auto& info = Locator::infoConstants::value();
	const auto forward = Locator::camera::has_value() ? Locator::camera::value().GetForward() : glm::vec3(0.0f, 0.0f, 1.0f);
	glm::vec3 point(0.0f);
	if (const auto* at = std::get_if<glm::vec3>(&target))
	{
		point = *at;
	}
	else if (const auto* transform =
	             Locator::entitiesRegistry::value().TryGet<const ecs::components::Transform>(std::get<entt::entity>(target)))
	{
		point = transform->position;
	}
	const auto plan = PlanCast(info, _choice, _multiplier, point, forward);
	_last = miracles::Cast(*_caster, plan, MagicName(info, plan.type), _player, target).message;
}

void Miracles::CastAtClick() noexcept
{
	if (!Locator::camera::has_value() || !Locator::infoConstants::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto hit = Locator::camera::value().RaycastMouseToLand(false);
	if (!hit.has_value())
	{
		_last = "No land under the click";
		return;
	}
	if (_clickCast == ClickCast::OnLand)
	{
		CastAt(hit->position);
		return;
	}
	const auto things = ThingsToPick(Locator::entitiesRegistry::value());
	if (const auto thing = NearestThing(things, hit->position))
	{
		CastAt(*thing);
	}
	else
	{
		_last = "Nothing near the click";
	}
}

void Miracles::CreateDispenserAt(glm::vec3 point) noexcept
{
	const auto& info = Locator::infoConstants::value();
	const DispenserPlan plan {
	    .kind = _dispenserKind,
	    .choice = _choice,
	    .abode = _dispenserAbode.value_or(AbodeInfo::NorseSpellDispenser),
	    .periodTurns = static_cast<uint32_t>(_dispenserPeriod),
	};
	_last =
	    miracles::CreateDispenser(*_dispenserCreator, info, plan, MagicName(info, MagicTypeOf(info, _choice)), _player, point)
	        .message;
}

void Miracles::CreateDispenserAtClick() noexcept
{
	if (!Locator::camera::has_value() || !Locator::infoConstants::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto hit = Locator::camera::value().RaycastMouseToLand(false);
	if (!hit.has_value())
	{
		_last = "No land under the click";
		return;
	}
	CreateDispenserAt(hit->position);
}

std::optional<glm::vec3> Miracles::HandLandPoint() noexcept
{
	if (!Locator::handSystem::has_value())
	{
		return std::nullopt;
	}
	using Side = ecs::systems::HandSystemInterface::Side;
	const auto positions = Locator::handSystem::value().GetPlayerHandPositions();
	auto point = HandPoint(positions.at(static_cast<size_t>(Side::Left)), positions.at(static_cast<size_t>(Side::Right)));
	if (point.has_value() && Locator::terrainSystem::has_value())
	{
		point->y = Locator::terrainSystem::value().GetHeightAt({point->x, point->z});
	}
	return point;
}

void Miracles::TurnPrayerOff() noexcept
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	_prayer.TurnOff([&registry](entt::entity site, PrayerCheats cheats) {
		if (!registry.Valid(site))
		{
			return;
		}
		if (auto* worship = registry.TryGet<ecs::components::WorshipSite>(site))
		{
			worship->infiniteChants = cheats.infinite;
			worship->freeMaintenance = cheats.freeMaintenance;
		}
	});
}

glm::vec3 Miracles::CameraPoint() noexcept
{
	glm::vec3 point(0.0f);
	if (Locator::camera::has_value())
	{
		point = Locator::camera::value().GetFocus();
	}
	if (Locator::terrainSystem::has_value())
	{
		point.y = Locator::terrainSystem::value().GetHeightAt({point.x, point.z});
	}
	return point;
}

void Miracles::Update() noexcept
{
	if (_clicked)
	{
		_clicked = false;
		if (_dispenserAtClick)
		{
			_dispenserAtClick = false;
			CreateDispenserAtClick();
		}
		else
		{
			CastAtClick();
		}
	}
}

void Miracles::UpdateAlways() noexcept
{
	if (!_prayer.IsOn() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	Locator::entitiesRegistry::value().Each<ecs::components::WorshipSite>(
	    [this](entt::entity site, ecs::components::WorshipSite& worship) {
		    PrayerCheats cheats {.infinite = worship.infiniteChants, .freeMaintenance = worship.freeMaintenance};
		    _prayer.Apply(site, cheats);
		    worship.infiniteChants = cheats.infinite;
		    worship.freeMaintenance = cheats.freeMaintenance;
	    });
}

bool Miracles::TakesEvent(const SDL_Event& event) const noexcept
{
	return IsLeftClick(event) &&
	       _leftCapture.Takes(event.type == SDL_MOUSEBUTTONDOWN, _clickCast != ClickCast::Off || _dispenserAtClick,
	                          ImGui::GetIO().WantCaptureMouse);
}

void Miracles::ProcessEventOpen(const SDL_Event& event) noexcept
{
	if (!TakesEvent(event))
	{
		return;
	}
	const bool press = event.type == SDL_MOUSEBUTTONDOWN;
	_leftCapture.Seen(press);
	if (press)
	{
		_clicked = true;
	}
}

void Miracles::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
