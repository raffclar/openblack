/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureMindModel.h"

#include <algorithm>
#include <array>
#include <span>
#include <string_view>

#include "Creature/CreaturePlanner.h"

using namespace openblack;
using namespace openblack::creature_mind_model;
using creature_desires::Desire;

namespace
{
/// What a fresh file says it was saved as and who it belonged to
constexpr std::string_view k_SavedAs = "openblack.erc";
constexpr std::u16string_view k_Profile = u"openblack";
/// The counts of the sections of a current mind file
constexpr size_t k_Counters = 7;
constexpr size_t k_PlayerDesires = 40;
constexpr size_t k_TownDesires = 17;
constexpr size_t k_LearningCounts = 45;
/// The tattoo word of a new creature's empty slot: no design, no site, no colour
constexpr uint32_t k_EmptyTattoo = 0xF0;
/// The two numbers at the end of a new creature's file
constexpr std::array<int32_t, 2> k_NewCreatureLast {0, -1};

creaturemind::MindFileData Skeleton(size_t skills, size_t miracles)
{
	creaturemind::MindFileData file;
	file.version = creaturemind::k_CurrentVersion;
	file.savedAs.assign(k_SavedAs.begin(), k_SavedAs.end());
	file.savedAs.push_back(0);
	for (const auto c : k_Profile)
	{
		file.profile.push_back(static_cast<uint8_t>(c & 0xff));
		file.profile.push_back(static_cast<uint8_t>(c >> 8));
	}
	file.profile.insert(file.profile.end(), {0, 0});
	file.counters.assign(k_Counters, 0);
	file.desireMemories.assign(k_DesireCount, {});
	file.actionsSeen.assign(skills, {.count = 0, .turn = 0});
	file.miraclesSeen.assign(miracles, {.count = 0, .turn = 0});
	file.playerDesires = std::vector<float>(k_PlayerDesires, 0.0f);
	file.townDesires = std::vector<float>(k_TownDesires, 0.0f);
	file.unknown1 = 0;
	file.alignment = 0.0f;
	file.physique.scratch = 0.0f;
	file.physique.unknown3 = 0;
	file.physique.size = 1.0f;
	file.database = std::vector<creaturemind::MindDatabaseEntry> {};
	file.unknown2 = 0;
	file.leashWords = std::array<uint32_t, 4> {};
	std::array<uint32_t, 8> tattoos {};
	tattoos.fill(k_EmptyTattoo);
	file.tattooHeader = tattoos;
	file.tattoo = std::vector<uint8_t> {};
	file.learningCounts = std::vector<creaturemind::MindLearningCount>(k_LearningCounts, {.c = 0});
	file.drawnScale = 1.0f;
	file.unknown4 = 0.0f;
	file.unknown5 = k_NewCreatureLast;
	return file;
}

/// The ids known, keeping the order a file listed them in and adding the newly known after
std::vector<creaturemind::MindKnownAction> KnownList(const std::vector<bool>& known,
                                                     const std::vector<creaturemind::MindKnownAction>& before)
{
	std::vector<creaturemind::MindKnownAction> list;
	for (const auto& action : before)
	{
		if (action.id < known.size() && known[action.id])
		{
			list.push_back({.id = action.id});
		}
	}
	for (uint32_t id = 0; id < known.size(); ++id)
	{
		if (known[id] && std::ranges::find(list, id, &creaturemind::MindKnownAction::id) == list.end())
		{
			list.push_back({.id = id});
		}
	}
	return list;
}

void Sightings(const std::vector<creature_watching::Sighting>& sightings, std::vector<creaturemind::MindSighting>& file)
{
	file.resize(std::max(file.size(), sightings.size()));
	for (size_t i = 0; i < sightings.size(); ++i)
	{
		file[i].count = sightings[i].count;
		file[i].turn = sightings[i].turn.value_or(file[i].turn.value_or(0));
	}
}
} // namespace

Learnt creature_mind_model::Fresh(const creature_desires::Desires& desires, std::span<const float> startingOpinions,
                                  creature_watching::Knowledge knowledge)
{
	Learnt learnt {
	    .opinions = {startingOpinions.begin(), startingOpinions.end()},
	    .turnsSinceDone = std::vector<uint32_t>(startingOpinions.size(), creature_planner::k_NoveltyTurns),
	    .knowledge = std::move(knowledge),
	};
	for (size_t d = 0; d < k_DesireCount; ++d)
	{
		for (const auto& source : desires.desires.at(d).sources)
		{
			learnt.initialThresholds.at(d).push_back(source.threshold);
		}
	}
	return learnt;
}

creature_tree::Episode creature_mind_model::StartingLesson()
{
	using namespace creature_tree;
	// The values in the order of the attributes of a villager: an enemy, natural, not alive, player 0, not harder than
	// the creature, no creature type, a villager, male, job 0, high life, not on fire
	constexpr std::array<uint32_t, 11> k_Slots {
	    allegiance::k_Enemy, origin::k_Natural, 0, 0, 0, 0, belief_types::k_Villager, 0, 0, 1, 1,
	};
	// The example is of the act-on tree. A mind file keeps it with no action and no place.
	return {
	    .belief = FromSlots(belief_types::k_Villager, k_Slots),
	    .feedback = k_StartingLessonFeedback,
	    .lessonKind = LessonKind(TreeKind::ActOn),
	};
}

void creature_mind_model::TeachStartingLesson(Learnt& learnt, std::span<const creature_tree::Attribute> hungerAllowed)
{
	const auto& episodes = learnt.episodes.at(static_cast<size_t>(TreeKind::ActOn)).at(static_cast<size_t>(Desire::Hunger));
	if (episodes.empty())
	{
		Learn(learnt, TreeKind::ActOn, Desire::Hunger, StartingLesson(), hungerAllowed);
	}
}

void creature_mind_model::Think(Learnt& learnt, std::string thought)
{
	learnt.thoughts.push_back(std::move(thought));
	if (learnt.thoughts.size() > k_MaxThoughts)
	{
		learnt.thoughts.erase(learnt.thoughts.begin());
	}
}

void creature_mind_model::Learn(Learnt& learnt, TreeKind kind, Desire desire, creature_tree::Episode episode,
                                std::span<const creature_tree::Attribute> allowed)
{
	const auto k = static_cast<size_t>(kind);
	const auto d = static_cast<size_t>(desire);
	auto& episodes = learnt.episodes.at(k).at(d);
	episode.lessonKind = LessonKind(kind);
	creature_tree::AddEpisode(episodes, std::move(episode));
	learnt.trees.at(k).at(d) = creature_tree::Build(episodes, allowed);
}

void creature_mind_model::LearnSpread(Learnt& learnt, TreeKind kind, Desire desire, const creature_tree::Belief& belief,
                                      float feedback, const creature_learning::Dependencies& dependencies,
                                      const std::array<std::vector<creature_tree::Attribute>, k_DesireCount>& allowed)
{
	for (const auto& [other, share] : creature_learning::Spread(desire, feedback, dependencies))
	{
		Learn(learnt, kind, other, {.belief = belief, .feedback = share}, allowed.at(static_cast<size_t>(other)));
	}
}

void creature_mind_model::RebuildTrees(Learnt& learnt,
                                       const std::array<std::vector<creature_tree::Attribute>, k_DesireCount>& allowed)
{
	for (size_t k = 0; k < k_TreeKinds; ++k)
	{
		for (size_t d = 0; d < k_DesireCount; ++d)
		{
			learnt.trees.at(k).at(d) = creature_tree::Build(learnt.episodes.at(k).at(d), allowed.at(d));
		}
	}
}

void creature_mind_model::DrawAsSourcesAreRead(
    const creaturemind::MindFileData& file, const std::function<creature_desires::DesireSetup::Spread(uint32_t type)>& spreadOf,
    const std::function<float(float)>& upTo)
{
	for (const auto& desire : file.desires)
	{
		for (const auto& source : desire.sources)
		{
			const auto spread = spreadOf(source.type);
			// The results do not stay: the numbers of the file replace them
			static_cast<void>(upTo(2.0f * spread.value));
			static_cast<void>(upTo(2.0f * spread.threshold));
		}
	}
}

LoadedMind creature_mind_model::FromFile(const creaturemind::MindFileData& file, const creature_desires::Desires& fresh,
                                         const Learnt& freshLearnt)
{
	LoadedMind mind {.desires = fresh, .learnt = freshLearnt};
	for (size_t d = 0; d < k_DesireCount && d < file.desires.size(); ++d)
	{
		const auto& saved = file.desires[d];
		auto& state = mind.desires.desires.at(d);
		const auto start = state;
		state.activated = saved.activated != 0;
		state.value = saved.value;
		state.max = saved.max;
		state.increaseSeconds = saved.increaseSeconds;
		state.suppressedTurns = 0;
		state.sources.clear();
		for (const auto& source : saved.sources)
		{
			const auto same = std::ranges::find(start.sources, source.type, &creature_desires::Source::type);
			state.sources.push_back({
			    .type = source.type,
			    .value = source.value,
			    .threshold = source.threshold,
			    .multiplier = same != start.sources.end() ? same->multiplier : 1.0f,
			});
		}
	}
	// The file's trees go by their place in the file, two to each desire: the activity tree, then the act-on tree. The
	// kind number and the desire in the file go into the tree as they are.
	for (size_t i = 0; i < file.trees.size() && i / k_TreeKinds < k_DesireCount; ++i)
	{
		const auto& tree = file.trees[i];
		const auto kind = static_cast<size_t>(i % k_TreeKinds == 0 ? TreeKind::Activity : TreeKind::ActOn);
		const auto desire = i / k_TreeKinds;
		mind.learnt.treeHeaders.at(kind).at(desire) = TreeHeader {.type = tree.type, .desire = tree.desireAgain};
		// The file's examples come after the examples that the mind has, such as the starting lesson. When the tree is
		// full, the remaining examples of the file are lost. A loaded example has no kind of lesson.
		auto& episodes = mind.learnt.episodes.at(kind).at(desire);
		for (const auto& saved : tree.episodes)
		{
			if (episodes.size() >= creature_tree::k_MaxEpisodes)
			{
				continue;
			}
			auto belief = creature_tree::FromSlots(saved.belief.type, saved.belief.attributes);
			belief.x = saved.belief.x;
			belief.z = saved.belief.z;
			episodes.push_back({
			    .belief = std::move(belief),
			    .feedback = saved.feedback,
			    .action = saved.action,
			    .savedDesire = saved.desire,
			});
		}
	}
	// The opinions are kept as the file has them, as many as it has
	if (file.opinions.has_value())
	{
		for (size_t a = 0; a < mind.learnt.opinions.size() && a < file.opinions->size(); ++a)
		{
			mind.learnt.opinions[a] = (*file.opinions)[a];
		}
	}
	auto& knowledge = mind.learnt.knowledge;
	for (const auto& action : file.known[0])
	{
		if (action.id < knowledge.skillsKnown.size())
		{
			knowledge.skillsKnown[action.id] = true;
		}
	}
	for (const auto& action : file.known[1])
	{
		if (action.id < knowledge.miraclesKnown.size())
		{
			knowledge.miraclesKnown[action.id] = true;
		}
	}
	for (size_t i = 0; i < knowledge.skillsSeen.size() && i < file.actionsSeen.size(); ++i)
	{
		knowledge.skillsSeen[i] = {.count = file.actionsSeen[i].count, .turn = file.actionsSeen[i].turn};
	}
	for (size_t i = 0; i < knowledge.miraclesSeen.size() && i < file.miraclesSeen.size(); ++i)
	{
		knowledge.miraclesSeen[i] = {.count = file.miraclesSeen[i].count, .turn = file.miraclesSeen[i].turn};
	}
	mind.learnt.name = file.name;
	mind.learnt.alignment = file.alignment.value_or(0.0f);
	mind.learnt.developmentTimer = file.developmentTimer;
	if (file.leashWords.has_value())
	{
		mind.learnt.leashWords = *file.leashWords;
	}
	// The word after the known actions is always 1 after a load
	auto kept = std::make_shared<creaturemind::MindFileData>(file);
	kept->unknown1 = 1;
	mind.learnt.file = std::move(kept);
	mind.developmentPhase = static_cast<uint32_t>(std::max(file.developmentPhase, 0));
	mind.attitudeToPlayer = file.attitudeToPlayer;
	mind.turnsLookedAt = file.turnsLookedAt;
	mind.speciesRow = file.speciesRow;
	mind.strength = file.physique.strength;
	mind.size = file.physique.size;
	mind.tattoo = file.tattoo;
	return mind;
}

creaturemind::MindFileData creature_mind_model::ToFile(const creature_desires::Desires& desires, const Learnt& learnt,
                                                       uint32_t developmentPhase, float attitudeToPlayer,
                                                       uint32_t turnsLookedAt, uint32_t speciesRow)
{
	const auto& knowledge = learnt.knowledge;
	auto file = learnt.file != nullptr ? *learnt.file : Skeleton(knowledge.skillsSeen.size(), knowledge.miraclesSeen.size());
	// Written as the current version. For each field that an older version lacks, the game writes the value of the new
	// creature that the file was loaded over: no leash known, empty tattoo slots, no learning counts, a drawn scale of 1
	// and the last two numbers 0 and -1.
	if (file.version != creaturemind::k_CurrentVersion)
	{
		const auto skeleton = Skeleton(knowledge.skillsSeen.size(), knowledge.miraclesSeen.size());
		file.version = creaturemind::k_CurrentVersion;
		file.counters.resize(k_Counters, 0);
		for (auto& sighting : file.actionsSeen)
		{
			sighting.turn = sighting.turn.value_or(0);
		}
		for (auto& sighting : file.miraclesSeen)
		{
			sighting.turn = sighting.turn.value_or(0);
		}
		file.playerDesires = file.playerDesires.value_or(*skeleton.playerDesires);
		file.townDesires = file.townDesires.value_or(*skeleton.townDesires);
		file.unknown1 = file.unknown1.value_or(0);
		file.physique.scratch = file.physique.scratch.value_or(0.0f);
		file.physique.unknown3 = file.physique.unknown3.value_or(0);
		file.physique.size = file.physique.size.value_or(1.0f);
		file.database = file.database.value_or(*skeleton.database);
		file.unknown2 = file.unknown2.value_or(0);
		file.leashWords = file.leashWords.value_or(*skeleton.leashWords);
		file.tattooHeader = file.tattooHeader.value_or(*skeleton.tattooHeader);
		file.tattoo = file.tattoo.value_or(*skeleton.tattoo);
		file.learningCounts = file.learningCounts.value_or(*skeleton.learningCounts);
		for (auto& count : *file.learningCounts)
		{
			count.c = count.c.value_or(0);
		}
		file.drawnScale = file.drawnScale.value_or(1.0f);
		file.unknown4 = file.unknown4.value_or(0.0f);
		file.unknown5 = file.unknown5.value_or(*skeleton.unknown5);
	}
	file.speciesRow = speciesRow;
	file.name = learnt.name;
	file.desires.resize(k_DesireCount);
	file.desireMemories.resize(k_DesireCount);
	for (size_t d = 0; d < k_DesireCount; ++d)
	{
		const auto& state = desires.desires.at(d);
		auto& saved = file.desires[d];
		saved.activated = state.activated ? 1 : 0;
		saved.value = state.value;
		saved.max = state.max;
		saved.increaseSeconds = state.increaseSeconds;
		saved.before7.reset();
		saved.from6To9.reset();
		saved.before15.reset();
		saved.sources.clear();
		for (const auto& source : state.sources)
		{
			saved.sources.push_back({.value = source.value, .threshold = source.threshold, .type = source.type});
		}
	}
	file.from6To9.reset();
	file.treeDesireCount = static_cast<int32_t>(k_DesireCount);
	file.trees.clear();
	for (size_t d = 0; d < k_DesireCount; ++d)
	{
		for (const auto treeKind : {TreeKind::Activity, TreeKind::ActOn})
		{
			const auto k = static_cast<size_t>(treeKind);
			const auto header = learnt.treeHeaders.at(k).at(d).value_or(
			    TreeHeader {.type = NewTreeType(treeKind), .desire = static_cast<int32_t>(d)});
			auto& tree = file.trees.emplace_back();
			tree.type = header.type;
			tree.desire = header.desire;
			tree.desireAgain = header.desire;
			for (const auto& episode : learnt.episodes.at(k).at(d))
			{
				tree.episodes.push_back({
				    .kind = episode.lessonKind,
				    .desire = episode.savedDesire.value_or(static_cast<int32_t>(d)),
				    .action = episode.action,
				    .belief = {.type = episode.belief.type,
				               .x = episode.belief.x,
				               .z = episode.belief.z,
				               .attributes = creature_tree::ToSlots(episode.belief)},
				    .feedback = episode.feedback,
				});
			}
		}
	}
	file.opinions = learnt.opinions;
	Sightings(knowledge.skillsSeen, file.actionsSeen);
	Sightings(knowledge.miraclesSeen, file.miraclesSeen);
	file.attitudeToPlayer = attitudeToPlayer;
	file.turnsLookedAt = turnsLookedAt;
	file.known[0] = KnownList(knowledge.skillsKnown, file.known[0]);
	file.known[1] = KnownList(knowledge.miraclesKnown, file.known[1]);
	file.before11.reset();
	file.alignment = learnt.alignment;
	file.developmentTimer = learnt.developmentTimer;
	file.developmentPhase = static_cast<int32_t>(developmentPhase);
	file.leashWords = learnt.leashWords;
	return file;
}
