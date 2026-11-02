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

#include <algorithm>
#include <array>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <L3DFile.h>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "3D/HandMorph.h"
#include "3D/L3DSkinTexels.h"
#include "Common/MachineClock.h"
#include "ECS/Archetypes/HandArchetype.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/GodHand.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandMiracleFx.h"
#include "ECS/Components/HandMorph.h"
#include "ECS/Components/HandStart.h"
#include "ECS/Components/HandStillness.h"
#include "ECS/Components/HeldDrawTrack.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerHands.h"
#include "ECS/Components/ReportedHand.h"
#include "ECS/Components/Transform.h"
#include "ECS/HandPlayer.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "EngineConfig.h"
#include "Hand/GodHand.h"
#include "Hand/HandStart.h"
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

using Hands = std::array<entt::entity, static_cast<size_t>(HandSystemInterface::Side::_Count)>;

/// The hands that the entity of a player links to. None when no entity of the player has a link.
std::optional<Hands> LinkedHands(const ecs::Registry& registry, PlayerNames name)
{
	const auto player = registry.FindFirst<const Player, const PlayerHands>(
	    [name](entt::entity, const Player& self, const PlayerHands&) { return self.name == name; });
	if (!player.has_value())
	{
		return std::nullopt;
	}
	const auto& hands = registry.Get<const PlayerHands>(*player);
	return Hands {hands.left, hands.right};
}

/// The hands that carry the mark of a player, by their side. The hands of the first player have no mark.
Hands MarkedHands(const ecs::Registry& registry, PlayerNames name)
{
	Hands found {entt::null, entt::null};
	registry.Each<const Hand>([&found, &registry, name](entt::entity entity, const Hand& hand) {
		if (ecs::HandPlayer(registry, entity) == name)
		{
			const auto side = hand.rightHanded ? HandSystemInterface::Side::Right : HandSystemInterface::Side::Left;
			found.at(static_cast<size_t>(side)) = entity;
		}
	});
	return found;
}

/// True when the pair has a hand
bool HasHand(const Hands& hands)
{
	return std::ranges::any_of(hands, [](entt::entity hand) { return hand != entt::null; });
}

/// The first entity of a player, or null
entt::entity PlayerEntityOf(const ecs::Registry& registry, PlayerNames name)
{
	entt::entity found = entt::null;
	registry.Each<const Player>([&found, name](entt::entity entity, const Player& player) {
		if (player.name == name && found == entt::null)
		{
			found = entity;
		}
	});
	return found;
}

/// The entity of a player links to the hands of the player. A player with no hands gets no link.
void Link(ecs::Registry& registry, entt::entity playerEntity, const Hands& hands)
{
	if (!registry.Valid(playerEntity) || !HasHand(hands))
	{
		return;
	}
	registry.AssignOrReplace<PlayerHands>(playerEntity, PlayerHands {.left = hands[0], .right = hands[1]});
}

/// The entity of a player that has a start, or null
entt::entity StartedPlayer(const ecs::Registry& registry, PlayerNames name)
{
	entt::entity found = entt::null;
	registry.Each<const Player, const HandStart>([&found, name](entt::entity entity, const Player& player, const HandStart&) {
		if (player.name == name && found == entt::null)
		{
			found = entity;
		}
	});
	return found;
}

/// The reported hand of a player starts at a point. The camera looks at the point from above. The turn measures the
/// first movement of the hand from the point.
void StartHandAt(ecs::Registry& registry, entt::entity hand, const glm::vec3& point)
{
	if (!registry.Valid(hand))
	{
		return;
	}
	auto& reported = ReportedOf(registry, hand);
	reported.focus = point;
	reported.camera = hand_start::CameraOf(point);
	if (auto* stillness = registry.TryGet<HandStillness>(hand))
	{
		stillness->last = point;
	}
}
} // namespace

bool HandSystem::Initialize() noexcept
{
	Hands hands {};
	hands[static_cast<size_t>(Side::Left)] =
	    HandArchetype::Create(glm::vec3(0.0f), glm::half_pi<float>(), 0.0f, glm::half_pi<float>(), 0.01f, false);
	hands[static_cast<size_t>(Side::Right)] =
	    HandArchetype::Create(glm::vec3(0.0f), glm::half_pi<float>(), 0.0f, glm::half_pi<float>(), 0.01f, true);
	// Usually the land makes its players after the hands. A first player that is there already gets the link now.
	// The gods of the other players get their hands from the command of the game server.
	auto& registry = Locator::entitiesRegistry::value();
	Link(registry, PlayerEntityOf(registry, PlayerNames::PLAYER_ONE), hands);

	return false;
}

std::optional<std::array<entt::entity, static_cast<size_t>(HandSystemInterface::Side::_Count)>>
HandSystem::GetHandsOf(PlayerNames player) const noexcept
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (auto linked = LinkedHands(registry, player); linked.has_value())
	{
		return linked;
	}
	// Before the land makes the entity of the player, the hands of the player are the hands with its mark
	const auto marked = MarkedHands(registry, player);
	return player == PlayerNames::PLAYER_ONE || HasHand(marked) ? std::optional(marked) : std::nullopt;
}

void HandSystem::LinkHandsOf(entt::entity playerEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* player = registry.Valid(playerEntity) ? registry.TryGet<const Player>(playerEntity) : nullptr;
	if (player == nullptr || registry.AllOf<PlayerHands>(playerEntity))
	{
		return;
	}
	Link(registry, playerEntity, MarkedHands(registry, player->name));
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
	Link(registry, PlayerEntityOf(registry, player), hands);
	// Hands that the game makes after the citadel of their player start at the citadel
	if (const auto started = StartedPlayer(registry, player); started != entt::null)
	{
		StartHandAt(registry, hands[static_cast<size_t>(Side::Left)], registry.Get<const HandStart>(started).point);
	}
	if (const auto logger = spdlog::get("game"))
	{
		SPDLOG_LOGGER_INFO(logger, "The god of player {} has hands of its own", static_cast<int>(player));
	}
	return hands;
}

void HandSystem::StartAtCitadel(PlayerNames owner, const glm::vec3& point)
{
	auto& registry = Locator::entitiesRegistry::value();
	// The player keeps the start for the hands and the computer mind that the game makes later. Every god has its
	// player from the start of the land, so the game makes the player when the land has none yet.
	entt::entity found = entt::null;
	if (Locator::playerSystem::has_value() && owner != PlayerNames::NEUTRAL)
	{
		found = Locator::playerSystem::value().EnsurePlayer(owner);
	}
	else
	{
		registry.Each<const Player>([&found, owner](entt::entity entity, const Player& player) {
			if (player.name == owner && found == entt::null)
			{
				found = entity;
			}
		});
	}
	if (found != entt::null)
	{
		registry.AssignOrReplace<HandStart>(found, HandStart {.point = point});
		// The hand of a god that no person plays starts at the citadel too
		auto* godHand = registry.TryGet<HandStillness>(found);
		god_hand::MindReport(godHand != nullptr ? *godHand : registry.Assign<HandStillness>(found), point);
		// The camera of a computer mind also looks at the citadel until the mind moves its hand
		if (auto* mind = registry.TryGet<ComputerPlayer>(found))
		{
			mind->hand.camera = hand_start::CameraOf(point);
		}
	}
	// The reports move only the left hand, so only the left hand starts at the citadel
	if (const auto hands = GetHandsOf(owner); hands.has_value())
	{
		StartHandAt(registry, (*hands)[static_cast<size_t>(Side::Left)], point);
	}
}

void HandSystem::FindAfterLoad()
{
	auto& registry = Locator::entitiesRegistry::value();
	// A world from an older save has no links from the players to their hands. The load makes them from the marks.
	std::vector<entt::entity> unlinked;
	registry.Each<const Player>([&unlinked](entt::entity entity, const Player&) { unlinked.push_back(entity); },
	                            entt::exclude<PlayerHands>);
	// The links come in the order in which the game made the players, as in a new game
	std::ranges::sort(unlinked, {}, [](entt::entity entity) { return entt::to_integral(entity); });
	for (const auto entity : unlinked)
	{
		LinkHandsOf(entity);
	}
	// A world from an older save can have a hand with no miracle effects. The load gives them to the hand.
	if (const auto left = GetPlayerHands().at(static_cast<size_t>(Side::Left));
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
	// The first player first, then the other gods in the order of their player numbers
	for (uint8_t number = 0; number < static_cast<uint8_t>(PlayerNames::_COUNT); ++number)
	{
		if (const auto hands = GetHandsOf(static_cast<PlayerNames>(number)); hands.has_value())
		{
			step((*hands)[static_cast<size_t>(Side::Left)]);
		}
	}
	// No person reports the hand of each other god, so its input goes to the origin of the world. The gods take this
	// step in the order of their player numbers.
	if (!Locator::playerSystem::has_value())
	{
		return;
	}
	const auto& players = Locator::playerSystem::value();
	std::vector<std::pair<PlayerNames, entt::entity>> gods;
	registry.Each<const Player>([&gods](entt::entity entity, const Player& player) {
		if (player.name != PlayerNames::NEUTRAL)
		{
			gods.emplace_back(player.name, entity);
		}
	});
	std::ranges::sort(gods, {}, &std::pair<PlayerNames, entt::entity>::first);
	for (const auto& [name, entity] : gods)
	{
		if (players.IsHumanPlayer(name))
		{
			continue;
		}
		auto* hand = registry.TryGet<HandStillness>(entity);
		god_hand::InputTurn(hand != nullptr ? *hand : registry.Assign<HandStillness>(entity));
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

void HandSystem::ReportSent(const player_commands::HandReport& report)
{
	_reportLatency.Sent(report, machine_clock::Ticks());
}

void HandSystem::Report(PlayerNames player, const player_commands::HandReport& report)
{
	// A report of this computer gives the latency of its reports
	if (IsOwnPlayer(player))
	{
		_reportLatency.Done(report, machine_clock::Ticks());
	}
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
		// The held thing moves from where the hand was at the last turn. Before the first turn that is the start of the hand.
		const auto* stillness = registry.TryGet<const HandStillness>(hand);
		const auto from = stillness != nullptr ? stillness->last : (before != nullptr ? before->handPoint : glm::vec3(0.0f));
		hand_grab::other_held::Reported(*track, from);
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

entt::entity HandSystem::GetReportedPickOf(PlayerNames player) const noexcept
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto hands = GetHandsOf(player);
	const auto hand = hands.has_value() ? (*hands)[static_cast<size_t>(Side::Left)] : entt::null;
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
	reported.mouseWheel = report.mouseWheel;
	reported.cameraTurned = report.cameraTurned;
	reported.cameraIcons = report.cameraIcons;
	reported.keysHeld = report.keysHeld;
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
	    .mouseWheel = reported->mouseWheel,
	    .cameraIcons = reported->cameraIcons,
	    .cameraTurned = reported->cameraTurned,
	    .keysHeld = reported->keysHeld,
	};
}

uint32_t HandSystem::GetHandStillTurnsOf(PlayerNames player) const noexcept
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto hands = GetHandsOf(player);
	const auto hand = hands.has_value() ? (*hands)[static_cast<size_t>(Side::Left)] : entt::null;
	const auto* stillness = registry.Valid(hand) ? registry.TryGet<const HandStillness>(hand) : nullptr;
	return stillness != nullptr ? stillness->stillTurns : 0;
}

std::array<entt::entity, static_cast<size_t>(HandSystemInterface::Side::_Count)> HandSystem::GetPlayerHands() const noexcept
{
	return GetHandsOf(PlayerNames::PLAYER_ONE).value_or(Hands {entt::null, entt::null});
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
	return PositionsOf(GetPlayerHands());
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
/// The texels of a skin of a hand mesh as the game loads them: at half size at the two lowest detail levels
std::vector<uint16_t> TexelsOf(const l3d::L3DFile& file, const l3d::L3DTexture& skin)
{
	const bool lowResolution = Locator::config::has_value() && Locator::config::value().detail.lowResTextures;
	return l3d_skin::LoadedTexels({reinterpret_cast<const uint16_t*>(skin.texels.data()), skin.texels.size()},
	                              static_cast<uint32_t>(file.GetHeader().flags), lowResolution);
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
		const auto from = TexelsOf(base, baseSkins[i]);
		auto& skin = skins.emplace_back(HandMorph::Skin {.id = baseSkins[i].id, .texels = {}});
		skin.texels.resize(from.size());
		const auto to = i < lookSkins.size() ? TexelsOf(look, lookSkins[i]) : from;
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
