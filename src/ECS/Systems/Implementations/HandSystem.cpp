/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HandSystem.h"

#include <span>

#include <L3DFile.h>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "3D/HandMorph.h"
#include "ECS/Archetypes/HandArchetype.h"
#include "ECS/Components/GodHand.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandMiracleFx.h"
#include "ECS/Components/HandMorph.h"
#include "ECS/Components/HandStillness.h"
#include "ECS/Components/HeldDrawTrack.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ReportedHand.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Hand/HandStillness.h"
#include "Hand/OtherHeldDraw.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The report of a hand. A hand with no report yet gets an empty one.
ReportedHand& ReportedOf(ecs::Registry& registry, entt::entity hand)
{
	if (auto* reported = registry.TryGet<ReportedHand>(hand))
	{
		return *reported;
	}
	return registry.Assign<ReportedHand>(hand);
}
} // namespace

bool HandSystem::Initialize() noexcept
{
	_hands[static_cast<size_t>(Side::Left)] =
	    HandArchetype::Create(glm::vec3(0.0f), glm::half_pi<float>(), 0.0f, glm::half_pi<float>(), 0.01f, false);
	_hands[static_cast<size_t>(Side::Right)] =
	    HandArchetype::Create(glm::vec3(0.0f), glm::half_pi<float>(), 0.0f, glm::half_pi<float>(), 0.01f, true);
	// The gods of the other players get their hands from the game server's command
	_godHands.clear();

	return false;
}

std::optional<std::array<entt::entity, static_cast<size_t>(HandSystemInterface::Side::_Count)>>
HandSystem::GetHandsOf(PlayerNames player) const noexcept
{
	if (player == PlayerNames::PLAYER_ONE)
	{
		return _hands;
	}
	const auto found = _godHands.find(player);
	return found != _godHands.end() ? std::optional(found->second) : std::nullopt;
}

std::array<entt::entity, static_cast<size_t>(HandSystemInterface::Side::_Count)> HandSystem::MakeHandsOf(PlayerNames player)
{
	if (const auto hands = GetHandsOf(player); hands.has_value())
	{
		return *hands;
	}
	// The same hands as the first player's, which the world tells apart by their player
	auto& registry = Locator::entitiesRegistry::value();
	std::array<entt::entity, 2> hands {};
	hands[static_cast<size_t>(Side::Left)] =
	    HandArchetype::Create(glm::vec3(0.0f), glm::half_pi<float>(), 0.0f, glm::half_pi<float>(), 0.01f, false);
	hands[static_cast<size_t>(Side::Right)] =
	    HandArchetype::Create(glm::vec3(0.0f), glm::half_pi<float>(), 0.0f, glm::half_pi<float>(), 0.01f, true);
	for (const auto hand : hands)
	{
		registry.Assign<GodHand>(hand, player);
	}
	_godHands[player] = hands;
	if (const auto logger = spdlog::get("game"))
	{
		SPDLOG_LOGGER_INFO(logger, "The god of player {} has hands of its own", static_cast<int>(player));
	}
	return hands;
}

void HandSystem::FindAfterLoad()
{
	_hands = {entt::null, entt::null};
	_godHands.clear();
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const Hand>([this, &registry](entt::entity entity, const Hand& hand) {
		const auto side = static_cast<size_t>(hand.rightHanded ? Side::Right : Side::Left);
		if (const auto* god = registry.TryGet<const GodHand>(entity))
		{
			auto [found, made] = _godHands.try_emplace(god->player, std::array<entt::entity, 2> {entt::null, entt::null});
			found->second.at(side) = entity;
			return;
		}
		_hands.at(side) = entity;
	});
	// A world from an older save can have a hand with no miracle effects. The load gives them to the hand.
	if (const auto left = _hands.at(static_cast<size_t>(Side::Left));
	    registry.Valid(left) && !registry.AllOf<HandMiracleFx>(left))
	{
		registry.Assign<HandMiracleFx>(left);
	}
}

void HandSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	// The world counts the stillness of each left hand that the reports give, as each computer has the same reports
	const auto step = [&registry](entt::entity hand) {
		if (!registry.Valid(hand))
		{
			return;
		}
		const auto* reported = registry.TryGet<const ReportedHand>(hand);
		auto* stillness = registry.TryGet<HandStillness>(hand);
		if (reported != nullptr && stillness != nullptr)
		{
			hand_stillness::Step(*stillness, reported->handPoint);
		}
	};
	step(_hands[static_cast<size_t>(Side::Left)]);
	for (const auto& [player, hands] : _godHands)
	{
		step(hands[static_cast<size_t>(Side::Left)]);
	}
}

std::optional<glm::vec3> HandSystem::GetDrawnHandPosition() const noexcept
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto hand = GetOwnHands()[static_cast<size_t>(Side::Left)];
	const auto* transform = registry.Valid(hand) ? registry.TryGet<const Transform>(hand) : nullptr;
	if (transform == nullptr || transform->position == glm::zero<glm::vec3>())
	{
		return std::nullopt;
	}
	return transform->position;
}

void HandSystem::Report(PlayerNames player, const player_commands::HandReport& report)
{
	// The report moves the left hand of its player. A player with no hands yet has no report.
	auto& registry = Locator::entitiesRegistry::value();
	const auto hands = GetHandsOf(player);
	const auto hand = hands.has_value() ? (*hands)[static_cast<size_t>(Side::Left)] : entt::null;
	if (!registry.Valid(hand))
	{
		return;
	}
	// In a network game the frames of this computer move what the hand of another player holds from the place before
	// the report to the reported place
	if (Locator::time::has_value() && Locator::time::value().IsNetworkGame())
	{
		const auto* before = registry.TryGet<const ReportedHand>(hand);
		auto* track = registry.TryGet<HeldDrawTrack>(hand);
		if (track == nullptr)
		{
			track = &registry.Assign<HeldDrawTrack>(hand);
		}
		hand_grab::other_held::Reported(*track, before != nullptr ? before->handPoint : glm::vec3(0.0f));
	}
	auto& reported = ReportedOf(registry, hand);
	reported.collidePoint = report.collidePoint;
	reported.handPoint = report.handPoint;
	reported.camera = report.camera;
	reported.focus = report.focus;
}

void HandSystem::ReportPick(PlayerNames player, entt::entity object)
{
	// The pick belongs to the left hand of its player
	auto& registry = Locator::entitiesRegistry::value();
	const auto hands = GetHandsOf(player);
	const auto hand = hands.has_value() ? (*hands)[static_cast<size_t>(Side::Left)] : entt::null;
	if (registry.Valid(hand))
	{
		ReportedOf(registry, hand).pickedObject = object;
	}
}

entt::entity HandSystem::GetReportedPick() const noexcept
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto hand = _hands[static_cast<size_t>(Side::Left)];
	const auto* reported = registry.Valid(hand) ? registry.TryGet<const ReportedHand>(hand) : nullptr;
	return reported != nullptr ? reported->pickedObject : entt::null;
}

std::optional<HandSystemInterface::ReportedCamera> HandSystem::GetReportedCamera(PlayerNames player) const noexcept
{
	const auto hands = GetHandsOf(player);
	if (!hands.has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto hand = (*hands)[static_cast<size_t>(Side::Left)];
	const auto* reported = registry.Valid(hand) ? registry.TryGet<const ReportedHand>(hand) : nullptr;
	// All the values are zero until the first report
	if (reported == nullptr || (reported->camera == glm::vec3(0.0f) && reported->focus == glm::vec3(0.0f)))
	{
		return std::nullopt;
	}
	return ReportedCamera {.origin = reported->camera, .focus = reported->focus};
}

void HandSystem::ReportView(PlayerNames player, const player_commands::ViewReport& report)
{
	// The view belongs to the left hand of its player. A player with no hands yet has no view.
	auto& registry = Locator::entitiesRegistry::value();
	const auto hands = GetHandsOf(player);
	const auto hand = hands.has_value() ? (*hands)[static_cast<size_t>(Side::Left)] : entt::null;
	if (!registry.Valid(hand))
	{
		return;
	}
	auto& reported = ReportedOf(registry, hand);
	reported.viewReported = true;
	reported.viewEye = report.eye;
	reported.viewFocus = report.focus;
	reported.viewLens = report.lens;
	reported.viewNearClip = report.nearClip;
	reported.viewScreenWidth = report.screenWidth;
	reported.viewScreenHeight = report.screenHeight;
	reported.viewAspect = report.aspect;
	reported.inclusionDistance = report.inclusionDistance;
	reported.cameraOrders = report.cameraOrders;
	reported.cameraArrived = report.cameraArrived;
	reported.gripsLand = report.gripsLand;
	reported.otherButton = report.otherButton;
}

std::optional<HandSystemInterface::ReportedView> HandSystem::GetReportedView(PlayerNames player) const noexcept
{
	const auto hands = GetHandsOf(player);
	if (!hands.has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto hand = (*hands)[static_cast<size_t>(Side::Left)];
	const auto* reported = registry.Valid(hand) ? registry.TryGet<const ReportedHand>(hand) : nullptr;
	if (reported == nullptr || !reported->viewReported)
	{
		return std::nullopt;
	}
	return ReportedView {
	    .eye = reported->viewEye,
	    .focus = reported->viewFocus,
	    .lens = reported->viewLens,
	    .nearClip = reported->viewNearClip,
	    .screen = glm::ivec2(reported->viewScreenWidth, reported->viewScreenHeight),
	    .aspect = reported->viewAspect,
	    .inclusionDistance = reported->inclusionDistance,
	    .cameraOrders = reported->cameraOrders,
	    .cameraArrived = reported->cameraArrived,
	    .gripsLand = reported->gripsLand,
	    .otherButton = reported->otherButton,
	};
}

uint32_t HandSystem::GetHandStillTurns() const noexcept
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto hand = _hands[static_cast<size_t>(Side::Left)];
	const auto* stillness = registry.Valid(hand) ? registry.TryGet<const HandStillness>(hand) : nullptr;
	return stillness != nullptr ? stillness->stillTurns : 0;
}

glm::vec3 HandSystem::GetReportedHandVelocity() const noexcept
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto hand = _hands[static_cast<size_t>(Side::Left)];
	const auto* stillness = registry.Valid(hand) ? registry.TryGet<const HandStillness>(hand) : nullptr;
	return stillness != nullptr ? stillness->velocity : glm::vec3(0.0f);
}

std::array<entt::entity, static_cast<size_t>(HandSystemInterface::Side::_Count)> HandSystem::GetPlayerHands() const noexcept
{
	return _hands;
}

namespace
{
/// Where the world has a pair of hands: the left hand from the last report, the right hand where its Transform puts it.
/// A hand at the origin of the world is off the land, and has none.
std::array<std::optional<glm::vec3>, static_cast<size_t>(HandSystemInterface::Side::_Count)>
PositionsOf(std::span<const entt::entity, static_cast<size_t>(HandSystemInterface::Side::_Count)> hands)
{
	const auto& registry = Locator::entitiesRegistry::value();
	// The left hand is where the reports put it. The right hand has no reports, and its Transform never moves.
	const auto left = hands[static_cast<size_t>(HandSystemInterface::Side::Left)];
	const auto right = hands[static_cast<size_t>(HandSystemInterface::Side::Right)];
	const auto* reported = registry.Valid(left) ? registry.TryGet<const ReportedHand>(left) : nullptr;
	const auto* rightAt = registry.Valid(right) ? registry.TryGet<const Transform>(right) : nullptr;
	const auto onLand = [](const glm::vec3& point) {
		return point == glm::zero<glm::vec3>() ? std::nullopt : std::optional(point);
	};
	return {
	    reported != nullptr ? onLand(reported->handPoint) : std::nullopt,
	    rightAt != nullptr ? onLand(rightAt->position) : std::nullopt,
	};
}
} // namespace

std::array<std::optional<glm::vec3>, static_cast<size_t>(HandSystemInterface::Side::_Count)>
HandSystem::GetPlayerHandPositions() const noexcept
{
	return PositionsOf(_hands);
}

std::array<std::optional<glm::vec3>, static_cast<size_t>(HandSystemInterface::Side::_Count)>
HandSystem::GetHandPositionsOf(PlayerNames player) const noexcept
{
	const auto hands = GetHandsOf(player);
	return hands.has_value() ? PositionsOf(*hands) : std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)> {};
}

std::array<std::optional<glm::vec3>, static_cast<size_t>(HandSystemInterface::Side::_Count)>
HandSystem::GetOwnHandPositions() const noexcept
{
	return PositionsOf(GetOwnHands());
}

namespace
{
std::span<const uint16_t> TexelsOf(const l3d::L3DTexture& skin)
{
	return {reinterpret_cast<const uint16_t*>(skin.texels.data()), skin.texels.size()};
}

/// The hand's skins drawn at an alignment: each of the base mesh's blended towards the one at the same place in the
/// evil or good mesh's list, the base's own where that mesh has fewer
std::vector<HandMorph::Skin> BlendSkins(const l3d::L3DFile& base, const l3d::L3DFile& look, float drawn)
{
	std::vector<HandMorph::Skin> skins;
	const auto& baseSkins = base.GetSkins();
	const auto& lookSkins = look.GetSkins();
	skins.reserve(baseSkins.size());
	for (size_t i = 0; i < baseSkins.size(); ++i)
	{
		const auto from = TexelsOf(baseSkins[i]);
		auto& skin = skins.emplace_back(HandMorph::Skin {.id = baseSkins[i].id, .texels = {}});
		skin.texels.resize(from.size());
		const auto to = i < lookSkins.size() ? TexelsOf(lookSkins[i]) : from;
		hand_morph::BlendSkin(from, to, drawn, skin.texels);
	}
	return skins;
}
} // namespace

void HandSystem::UpdateAlignmentMorph(std::optional<map_coords::MapCoords> picked, bool holdPoint)
{
	if (!Locator::alignmentSystem::has_value())
	{
		return;
	}
	// Only the hands of the player at this computer are drawn, so only they show an alignment: the alignment of their own
	// player, and the influence of that player at the point that this computer picks
	const auto player = GetOwnPlayer();
	const auto alignment = Locator::alignmentSystem::value().GetPlayerAlignment(player);
	auto& files = Locator::resources::value().GetL3DFiles();
	const auto fileOf = [&files](size_t index) -> const l3d::L3DFile* {
		const auto id = HandMorph::k_SkinFileIds.at(index);
		return files.Contains(id) ? &*files.Handle(id) : nullptr;
	};
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto entity : GetOwnHands())
	{
		auto* morph = registry.Valid(entity) ? registry.TryGet<HandMorph>(entity) : nullptr;
		if (morph == nullptr)
		{
			continue;
		}
		// The frame goes by the influence tested after the last one, then tests it again for the next
		const auto change = hand_morph::Advance(morph->state, alignment, morph->pointInInfluence);
		morph->point = hand_morph::NextPoint(morph->point, picked, holdPoint);
		if (Locator::influenceSystem::has_value())
		{
			morph->pointInInfluence = Locator::influenceSystem::value().HandPointInfluence(player, morph->point) > 0.0f;
		}
		if (change.shape)
		{
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "The hand is drawn anew for alignment {:.3f}", morph->state.drawn);
		}
		if (!change.skin)
		{
			continue;
		}
		const auto* base = fileOf(0);
		const auto* look = fileOf(hand_morph::LookOf(*change.skin) == hand_morph::Look::Evil ? 1 : 2);
		morph->skins = base != nullptr ? BlendSkins(*base, look != nullptr ? *look : *base, *change.skin)
		                               : std::vector<HandMorph::Skin> {};
		++morph->revision;
	}
}
