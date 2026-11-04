/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cctype>
#include <cmath>
#include <cstdint>
#include <ctime>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iterator>
#include <limits>
#include <numbers>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <LHVMTypes.h>
#include <SaveSlots.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/matrix.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/TempleInteriorInterface.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/ScriptCameraModel.h"
#include "Sound/GameMusic.h"
#if defined(OPENBLACK_DEVELOPER_TOOLS)
#include "Common/DrawTrace.h"
#endif
#include "CHLApi/NativeHelpers.h"
#include "CHLApi/Natives.h"
#include "Creature/CreatureDesires.h"
#include "ECS/Archetypes/AnimatedStaticArchetype.h"
#include "ECS/Archetypes/BallArchetype.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Archetypes/FeatureArchetype.h"
#include "ECS/Archetypes/MistArchetype.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/PuzzleTotemArchetype.h"
#include "ECS/Archetypes/ScriptMarkerArchetype.h"
#include "ECS/Archetypes/SharkArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/ChallengePictures.h"
#include "ECS/ChallengeRecords.h"
#include "ECS/Components/ChallengePicture.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Dances.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/GestureSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MistSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/PuzzleGameSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/ScriptHighlightSystemInterface.h"
#include "ECS/Systems/ScriptMusicSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TurnCheerSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WalkPathSystemInterface.h"
#include "Enums.h"
#include "FileSystem/FileSystemInterface.h"
#include "Hand/HandGrabRules.h"
#include "Help/DialogueText.h"
#include "Help/HelpProfile.h"
#include "Help/Spirits.h"
#include "InfoConstants.h"
#include "Input/GameActionMapInterface.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "MainThreadWork.h"
#include "Physics/Body.h"
#include "ScriptHeaders/ScriptChallengeSnapshots.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "Visual/RendererRequestsInterface.h"

namespace openblack::chlapi
{

using openblack::Locator;

/// What a script gave for a challenge's record, taken off its stack: every value, however many arguments it gave its
/// reminder script, so that nothing is left behind for the script's next calls
script::challenge_snapshots::Snapshot PopChallengeSnapshot(script::challenge_snapshots::Call call)
{
	auto& lhvm = Locator::vm::value();
	const auto snapshot =
	    script::challenge_snapshots::Read(call, {
	                                                .pop =
	                                                    [&lhvm]() {
		                                                    script::challenge_snapshots::Value value;
		                                                    value.value = lhvm.Pop(value.type);
		                                                    return value;
	                                                    },
	                                                .text = [&lhvm](uint32_t offset) { return lhvm.GetString(offset); },
	                                            });
	if (snapshot.tooManyArguments)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Too many arguments for a challenge's reminder script: {}",
		                    snapshot.arguments.size());
	}
	return snapshot;
}

/// The player's record of a challenge is kept up to date from a script's snapshot, and a snapshot of a challenge already
/// recorded moves the first player's alignment
void KeepChallengeRecord(const script::challenge_snapshots::Snapshot& snapshot, script::challenge_snapshots::Call call)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	const ecs::challenge_records::References references {
	    .add =
	        [](entt::entity thing) {
		        if (Locator::scriptObjects::has_value())
		        {
			        Locator::scriptObjects::value().AddReference(thing);
		        }
	        },
	    .remove =
	        [](entt::entity thing) {
		        if (Locator::scriptObjects::has_value())
		        {
			        Locator::scriptObjects::value().RemoveReference(thing);
		        }
	        },
	};
	const auto outcome = ecs::challenge_records::Record(registry, snapshot, call, turn, references);
	if (outcome.takesPicture && Locator::rendererRequests::has_value())
	{
		// The picture is of the screen as the player sees it now
		auto* picture = registry.TryGet<ecs::components::ChallengePicture>(outcome.record);
		if (picture == nullptr)
		{
			picture = &registry.Assign<ecs::components::ChallengePicture>(outcome.record);
		}
		// The renderer takes the picture at the end of a draw, so the request goes to the main thread
		PostToMainThread(MainThreadApi::Renderer, [name = ecs::challenge_pictures::Ask(*picture, outcome.record)] {
			Locator::rendererRequests::value().RequestScreenPicture(name, &ecs::challenge_pictures::Take);
		});
	}
	if (!outcome.alignment.has_value() || !Locator::alignmentSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto player =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	auto& alignment = Locator::alignmentSystem::value();
	const auto change =
	    ecs::challenge_records::AlignmentChange(alignment.GetPlayerAlignment(player), *outcome.alignment,
	                                            Locator::infoConstants::value().player.maxScriptAlignmentChange);
	// The totals of the alignment changes count the change before the alignment takes it
	alignment.AddToHistory(player, ecs::components::AlignmentCause::Script, change);
	alignment.AddPlayerAlignment(player, change);
}

void CHLApi::RecordChallenge(const script::challenge_snapshots::Snapshot& snapshot, script::challenge_snapshots::Call call)
{
	KeepChallengeRecord(snapshot, call);
}

void Snapshot() // 057 SNAPSHOT
{
	KeepChallengeRecord(PopChallengeSnapshot(script::challenge_snapshots::Call::Start),
	                    script::challenge_snapshots::Call::Start);
}

void UpdateSnapshot() // 238 UPDATE_SNAPSHOT
{
	KeepChallengeRecord(PopChallengeSnapshot(script::challenge_snapshots::Call::Update),
	                    script::challenge_snapshots::Call::Update);
}

/// The kinds of reward a script can give are numbered 1 to 60
constexpr int32_t k_LastRewardType = 60;

/// A reward chest of a kind for the first player, given to a town or none: on the land, or from the sky
uint32_t GiveReward(int32_t type, glm::vec3 position, entt::entity town, bool fromSky)
{
	if (type < 1 || type > k_LastRewardType)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CREATE_REWARD: invalid type {}", type);
		return 0;
	}
	if (!Locator::rewardSystem::has_value())
	{
		return 0;
	}
	const auto player =
	    Locator::playerSystem::has_value() ? std::optional(Locator::playerSystem::value().GetLocalPlayer()) : std::nullopt;
	const auto chest =
	    Locator::rewardSystem::value().Create(position, static_cast<RewardObjectInfo>(type), player, town, fromSky);
	RegisterCreated(chest);
	return static_cast<uint32_t>(chest);
}

void CreateReward() // 239 CREATE_REWARD
{
	const auto fromSky = Pop().intVal != 0;
	const auto position = PopVec();
	const auto type = Pop().intVal;
	Pusho(GiveReward(type, position, entt::null, fromSky));
}

void CreateRewardInTown() // 240 CREATE_REWARD_IN_TOWN
{
	const auto fromSky = Pop().intVal != 0;
	const auto position = PopVec();
	const auto town = PopObject();
	const auto type = Pop().intVal;
	// The town must be one
	if (!Locator::entitiesRegistry::value().Valid(town) ||
	    !Locator::entitiesRegistry::value().AllOf<ecs::components::Town>(town))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CREATE_REWARD_IN_TOWN: not a town");
		Pusho(0);
		return;
	}
	Pusho(GiveReward(type, position, town, fromSky));
}

void CreateHighlight() // 272 CREATE_HIGHLIGHT
{
	const auto challenge = Pop().uintVal;
	const auto position = PopVec();
	const auto kind = Pop().uintVal;
	// A kind past the info table makes none (the game reads past its table)
	const auto highlight = Locator::scriptHighlightSystem::value().Create(kind, position, challenge);
	if (highlight == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Highlight not created");
		PushObject(entt::null);
		return;
	}
	RegisterCreated(highlight);
	PushObject(highlight);
}

void SetDrawHighlight() // 306 SET_DRAW_HIGHLIGHT
{
	// The challenge scrolls show or hide; the signs always show
	Locator::chlapi::value().SetHighlightDrawOn(Pop().intVal != 0);
}

void HighlightProperties() // 334 HIGHLIGHT_PROPERTIES
{
	const auto category = Pop().uintVal;
	const auto text = Pop().uintVal;
	const auto object = PopObject();
	if (object == entt::null || !Locator::entitiesRegistry::value().AllOf<ecs::components::ScriptHighlight>(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing not valid");
		return;
	}
	Locator::scriptHighlightSystem::value().SetProperties(object, text, category);
}

void UpdateSnapshotPicture() // 351 UPDATE_SNAPSHOT_PICTURE
{
	KeepChallengeRecord(PopChallengeSnapshot(script::challenge_snapshots::Call::Picture),
	                    script::challenge_snapshots::Call::Picture);
}

} // namespace openblack::chlapi
