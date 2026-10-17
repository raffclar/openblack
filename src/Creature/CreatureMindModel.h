/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <MindFile.h>

#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureWatching.h"

/// What a creature has learnt and remembers, beyond its desires: its opinions of actions, the examples its decision
/// trees grow from, what it did lately, how it feels about other creatures, the skills and miracles it knows, and its
/// thoughts. A mind file sets all of it, and the mind can be written back to one.
namespace openblack::creature_mind_model
{
using creature_desires::k_DesireCount;

/// The two trees of each desire, in the order that a mind file keeps them. The activity tree tells how good the whole
/// thing that a desire is about is (a town to help, a creature to befriend). The act-on tree tells how good a thing is
/// to act on for the desire.
enum class TreeKind : uint8_t
{
	Activity,
	ActOn,
};
constexpr size_t k_TreeKinds = 2;
/// A mind file keeps a word for each leash type, by the number of the type
constexpr size_t k_LeashWords = 4;

/// The most thoughts kept, the newest last
constexpr size_t k_MaxThoughts = 8;

/// The kind number and the desire that a mind file gives a tree. The game reads them into the tree and writes them back
/// as they are.
struct TreeHeader
{
	int32_t type {0};
	int32_t desire {0};
};
/// The kind number that the game gives a new tree: 0 for the activity tree, 1 for the act-on tree
[[nodiscard]] constexpr int32_t NewTreeType(TreeKind kind)
{
	return kind == TreeKind::Activity ? 0 : 1;
}
/// The kind of lesson that an example the creature learns keeps in a mind file: 1 for the activity tree, 2 for the
/// act-on tree
[[nodiscard]] constexpr int32_t LessonKind(TreeKind kind)
{
	return kind == TreeKind::Activity ? 1 : 2;
}

struct Learnt
{
	/// The opinion of each action, -1 to 1, and turns since each was last done
	std::vector<float> opinions;
	std::vector<uint32_t> turnsSinceDone;
	/// Each desire's examples, by tree kind, and the trees grown from them
	std::array<std::array<std::vector<creature_tree::Episode>, k_DesireCount>, k_TreeKinds> episodes {};
	std::array<std::array<creature_tree::Tree, k_DesireCount>, k_TreeKinds> trees {};
	/// What a mind file gave each tree; none for a tree that no file set, which has its new kind number and its desire
	std::array<std::array<std::optional<TreeHeader>, k_DesireCount>, k_TreeKinds> treeHeaders {};
	std::vector<creature_learning::Context> contexts;
	std::vector<creature_learning::CreatureAttitude> creatures;
	creature_watching::Knowledge knowledge;
	std::optional<creature_watching::Mimicry> mimicry;
	std::vector<std::string> thoughts;
	/// Each desire's sources' thresholds as the creature started, to tell how much it has come to like it
	std::array<std::vector<float>, k_DesireCount> initialThresholds {};
	std::u16string name;
	float alignment {0.0f};
	uint32_t developmentTimer {0};
	/// A word for each leash type, as a mind file keeps them: not 0 when the creature knows the leash. The first type is
	/// no leash, which only the file uses.
	std::array<uint32_t, k_LeashWords> leashWords {};
	/// The mind file it was loaded from, for what the file keeps that the mind doesn't use, written back as it was
	std::shared_ptr<const creaturemind::MindFileData> file;
};

/// A fresh mind's learning: each action's opinion as the action table starts it, nothing seen but what is known from
/// the start
[[nodiscard]] Learnt Fresh(const creature_desires::Desires& desires, std::span<const float> startingOpinions,
                           creature_watching::Knowledge knowledge);
/// The example that every new creature starts with: an enemy villager that is alive and not on fire is good to eat
[[nodiscard]] creature_tree::Episode StartingLesson();
/// The feedback of the starting example
constexpr float k_StartingLessonFeedback = 0.8f;
/// A new creature learns the starting example for its hunger, only when it knows nothing yet about what to eat. The
/// tree grows again with the attributes allowed.
void TeachStartingLesson(Learnt& learnt, std::span<const creature_tree::Attribute> hungerAllowed);
/// Adds a thought, forgetting the oldest
void Think(Learnt& learnt, std::string thought);
/// Adds an example to a desire's tree and grows the tree again, testing the attributes allowed. The example gets the
/// kind of lesson of the tree.
void Learn(Learnt& learnt, TreeKind kind, creature_desires::Desire desire, creature_tree::Episode episode,
           std::span<const creature_tree::Attribute> allowed);
/// A lesson about a thing: the desire learns the feedback, and every other desire that depends on it learns the
/// feedback times how much it depends on it. Each tree grows again with the attributes allowed for its desire.
void LearnSpread(Learnt& learnt, TreeKind kind, creature_desires::Desire desire, const creature_tree::Belief& belief,
                 float feedback, const creature_learning::Dependencies& dependencies,
                 const std::array<std::vector<creature_tree::Attribute>, k_DesireCount>& allowed);
/// The word to write for a leash type. The word stays as it is while it agrees with what the creature knows. Else it
/// is 1 for a leash that the creature knows and 0 for a leash that it does not know.
[[nodiscard]] constexpr uint32_t LeashWord(uint32_t word, bool known)
{
	return (word != 0) == known ? word : (known ? 1u : 0u);
}
/// Grows every tree again from its examples
void RebuildTrees(Learnt& learnt, const std::array<std::vector<creature_tree::Attribute>, k_DesireCount>& allowed);

/// Everything a mind file sets
struct LoadedMind
{
	creature_desires::Desires desires;
	Learnt learnt;
	uint32_t developmentPhase {0};
	float attitudeToPlayer {0.0f};
	/// The game turns its player looked at it
	uint32_t turnsLookedAt {0};
	uint32_t speciesRow {0};
	/// The body as the file keeps it, for the body to take up
	float strength {0.0f};
	std::optional<float> size;
	std::optional<std::vector<uint8_t>> tattoo;
};
/// The mind a file holds, over a fresh mind of its species for what the file doesn't keep (how desires fade, how fast
/// sources fade, and so on). As the game does it:
/// - For each desire, the file's first tree goes to the activity tree, and its second tree to the act-on tree.
/// - The examples of a file's tree come after the examples that the mind has, up to the most that a tree keeps. A loaded
///   example keeps no kind of lesson (0).
/// - The opinions, the attitude to the player and the body values are kept as the file has them.
/// - The word after the known actions is 1.
/// - The leash words are kept as the file has them. A file without them keeps the words of the mind under it.
[[nodiscard]] LoadedMind FromFile(const creaturemind::MindFileData& file, const creature_desires::Desires& fresh,
                                  const Learnt& freshLearnt);
/// A mind written as a file of the current version, over the file it was loaded from when there was one, so that
/// what the mind doesn't use is kept. For each desire, the activity tree goes first and the act-on tree second, each
/// with the kind number and desire that a file gave it. A file of an older version gets, for the fields that it does
/// not have, the values of a new creature.
[[nodiscard]] creaturemind::MindFileData ToFile(const creature_desires::Desires& desires, const Learnt& learnt,
                                                uint32_t developmentPhase, float attitudeToPlayer, uint32_t turnsLookedAt,
                                                uint32_t speciesRow);

} // namespace openblack::creature_mind_model
