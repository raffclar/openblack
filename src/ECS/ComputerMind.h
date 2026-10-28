/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"

/// The mind of a computer god. The mind is a tree of needs. Six broad aims are at the top, and the actions of the hand
/// are at the leaves. Each turn that the god has nothing to do, the mind finds the strongest leaf and does its action.
namespace openblack::computer_mind
{

/// The level of a node in the tree
enum class Layer : uint8_t
{
	Root,
	Top,
	Need,
	Action,
	Object,
};

/// Each node of the tree, by its row in the table of the game. The names are the names that the land scripts use.
enum class NodeKind : uint8_t
{
	Root,
	TopPlayerInteract,
	TopCreatureInteract,
	TopTownInteract,
	TopDefeatPlayer,
	TopReactToPlayer,
	TopWorldInteract,
	NeedGiveToPlayer,
	NeedTalktoPlayer,
	NeedExpandInfluence,
	NeedLookaftercreature,
	NeedTeachCreature,
	NeedPlayWithCreature,
	NeedReactToCreatureEmergency,
	NeedAttendTownDesires,
	NeedExpandTown,
	NeedAttendToWorshippersWorshippers,
	NeedIncreaseTownPopulation,
	NeedDefeatPlayer,
	NeedPlayerWithMoreSpells,
	NeedPlayerWithBetterCreature,
	NeedPlayerWithMoreWorshippers,
	NeedReactToPlayer,
	NeedCheckTownsForBeliefAttack,
	NeedCheckTownForDamage,
	NeedCheckTownForProtection,
	ActionThrowThingAtPlayer,
	ActionSendNiceTextToPlayer,
	ActionSendNastyTextToPlayer,
	ActionTauntPlayer,
	ActionScarePlayer,
	ActionGiveCreatureThingToPlayer,
	ActionGiveToPlayer,
	ActionGiveCreatureFood,
	ActionCastFoodForMyCreature,
	ActionMakeCreatureDrink,
	ActionRestoreCreatureHealth,
	ActionMakeCreatureSleep,
	ActionSendCreatureHome,
	ActionMoveCreaturetoWarmerArea,
	ActionMoveCreaturetoCoollerArea,
	ActionTeachCreatureNormalAction,
	ActionTeachCreatureAggressiveMagic,
	ActionTeachCreatureCompassionateMagic,
	ActionTeachCreatureImpressiveMagic,
	ActionThrowFoodatCreature,
	ActionSlapCreature,
	ActionStrokeCreature,
	ActionAttachLeashtoMovingObject,
	ActionCastSpellOnMyCreatureForALaugh,
	ActionReacttoCreatureStarvingEmergency,
	ActionReacttoCreatureDamagedEmergency,
	ActionReacttoCreatureIllEmergency,
	ActionTownWithHighestFoodDesire,
	ActionTownWithHighestWoodDesire,
	ActionGiveBuildingSiteWorkers,
	ActionGiveBuildingSiteWood,
	ActionGiveMyTownPopulation,
	ActionGiveWorshippersFood,
	ActionGiveWorshippersRest,
	ActionWorshipSiteThatNeedsVillagers,
	ActionTakeOverTown,
	ActionDestroyTown,
	ActionChargeUpSpell,
	ActionAttackCreature,
	ActionReactToAggressiveCreature,
	ActionReactToPlayerCastingSpells,
	ActionReactToPlayerThrowingStuff,
	ActionDefendTown,
	ActionFindBurningAbode,
	ActionFindDamagedAbode,
	ActionFindUnprotectedAbode,
	ActionCombineScaffolds,
	ActionFindPlaceToUseScaffoldForBuilding,
	ActionFillWorkshop,
	ActionTakeOverTownByImpressing,
	ActionTakeOverTownByFulfillingFoodDesire,
	ActionTakeOverTownByFulfillingWoodDesire,
	ActionDefendTownAgainstBeliefAttack,
	ActionMakeDiscipleBreader,
	ObjectGetCreaturetoHelp,
	ObjectGetCreaturetoHelpBuild,
	ObjectShallCastFoodSpell,
	ObjectGetFoodFromField,
	ObjectGetFoodFromFishFarm,
	ObjectGetFoodFromOtherTownForStoragePit,
	ObjectGetFoodFromOtherTownForObject,
	ObjectShallCastWoodSpell,
	ObjectGetWoodFromForest,
	ObjectGetWoodFromOtherTown,
	ObjectCastFoodSpellOnWorshipSite,
	ObjectShallCastWoodSpellAgain,
	ObjectCastFoodSpellOn,
	ObjectCastWoodSpellOn,
	ObjectFindFreeVillagerBuilderInTown,
	ObjectFindFreeVillagerInArea,
	ObjectUseCreatureToImpressTown,
	ObjectThrowThingsToImpress,
	ObjectCastSpellToImpress,
	ObjectCastSpellToFulfilTownNeeds,
	ObjectDestroytownWithSpell,
	ObjectDestroytownWithCreature,
	ObjectDestroytownWithObject,
	ObjectAttackCreatureWithSpells,
	ObjectAttackCreatureWithCreature,
	ObjectThrowrockatCreature,
	ObjectDefendTownWithCreature,
	ObjectDefendtownWithSpells,
	ObjectObjectToLeashTo,
	ObjectCastFoodForMyCreature,
	ObjectRestoreMyCreatureHealth,
	ObjectTeachCreatureAggressiveMagic,
	ObjectTeachCreatureCompassionateMagic,
	ObjectTeachCreatureImpressiveMagic,
	ObjectCastSpellOnMyCreatureForALaugh,
	ObjectRestoreMyCreatureHealthAgain,
	ObjectUseCreatureAsDiverseionAgainstPlayer,
	ObjectCastShieldToProtect,
	ObjectCastPhysicalShieldToProtect,
	ObjectFindShieldSpell,
	ObjectCastWaterSpell,
	ObjectFindScaffoldToCombineWith,
	ObjectFindLocationToUseScaffold,
	ObjectDIRECTPickupAndDropObject,
	ObjectRaiseTotemInTown,
	ObjectTakeVillagerFromTownToWorshipSite,
	ObjectTakeVillagerFromWorshipSiteToTown,
	ObjectCastSpellToDefendAgainstBelief,
	ObjectCreateDiscipleToFulfillTownDesire,
	ObjectCastHealSpellAtWorshipSite,

	_Count
};

/// The most children that a node can have
constexpr size_t k_MostChildren = 8;

/// The fixed facts of one kind of node, the same for every god
struct NodeInfo
{
	/// The name that the land scripts use for the node
	std::string_view name;
	/// The number of the text of the node. A personality file writes it after the name. The root has no number.
	std::optional<uint16_t> text;
	Layer layer {Layer::Root};
	/// The weight that the node starts with: its personality
	float weight {1.0f};
	/// The suppression that the node gets after its action starts
	float afterPerform {1.0f};
	/// The rate at which the suppression of the node comes back to 1
	float recovery {1.0f};
	/// A top node is looked at again during an action only on turns that this number divides; 0 is every turn
	uint32_t timer {0};
	/// The node finds how much it is wanted itself
	bool intensity {false};
	/// The node can find the object that it acts on
	bool object {false};
	/// The node has an action of its own
	bool perform {false};
	/// The kinds of its children, in their order
	std::array<NodeKind, k_MostChildren> children {};
	uint8_t childCount {0};
};

/// The facts of each kind of node
[[nodiscard]] const NodeInfo& InfoOf(NodeKind kind);

/// The suppression that a node gets when its action could not start
constexpr float k_FailedSuppression = 0.01f;
/// The share of its suppression that the last chosen node keeps when its action ends badly
constexpr float k_BadEndShare = 0.5f;
/// The turns that the mind waits after it found nothing to do
constexpr uint32_t k_EmptyWait = 30;
/// The number of magic types that the mind counts requests for
constexpr size_t k_MagicTypes = 42;

/// One node of the tree of a god
struct Node
{
	NodeKind kind {NodeKind::Root};
	/// The node above, none at the root
	std::optional<uint16_t> parent;
	std::array<uint16_t, k_MostChildren> children {};
	uint8_t childCount {0};
	/// Multiplies how much the node is wanted. An action lowers it, and it comes back to 1 with time.
	float suppression {1.0f};
	/// The personality of the node
	float weight {1.0f};
	/// The game turns at which the node was last looked at and last acted on
	uint32_t lastWorkedOut {0};
	uint32_t lastPerformed {0};
	/// How much the node was wanted when it was last looked at
	float value {0.0f};
	/// The leaf that the node chose when it was last looked at
	std::optional<uint16_t> bestLeaf;
	/// The thing that the node acts on: a town, a player, a creature, a site
	entt::entity object {entt::null};
	/// For a node that acts on a building site: the town whose site it is at the building that is the object
	entt::entity siteTown {entt::null};
	/// A number that a node can keep for its action: the food that a town wants more than it has
	int32_t amount {0};
	/// The thing and the place that a land script gave when it made the god do the action of the node. They stay
	/// until the next script action of the node.
	entt::entity argument {entt::null};
	glm::vec3 place {0.0f};
};

/// A thing near the hand that the god saw, and the game turn at which it saw the thing first
struct SeenThing
{
	entt::entity thing {entt::null};
	uint32_t turn {0};
};

/// The god sees things in the cells of the map that are nearer its hand than this radius plus the margin, in metres
constexpr float k_SeenRadius = 30.0f;
constexpr float k_SeenCellMargin = 10.0f;
/// The most cells that the god looks at for things near its hand
constexpr int32_t k_SeenMostCells = 99999;
/// When the god has seen more than this number of things, it forgets the old ones on each turn that the period divides.
/// It forgets a thing that it saw more than the number of turns before.
constexpr size_t k_SeenMost = 50;
constexpr uint32_t k_SeenForgetEvery = 500;
constexpr uint32_t k_SeenForgetAfter = 8000;

/// The most letters of the name of an action that a land script queues
constexpr size_t k_QueuedNameLength = 260;

/// An action that a land script put in the queue of the god: the name of a node and two things
struct QueuedAction
{
	std::string name;
	entt::entity first {entt::null};
	entt::entity second {entt::null};
	/// The game turn at which the script queued the action
	uint32_t turn {0};
};

/// The mind of one god
struct Mind
{
	/// The nodes, the root first, in the order that the game makes them
	std::vector<Node> nodes;
	/// How much the action that the hand does now was wanted when it started
	float doing {0.0f};
	/// The turn at which the mind last found nothing to do, 0 when it found something
	uint32_t lastEmptyTurn {0};
	/// The hand was busy on the last turn: the mind waits one more turn after each action
	bool wasBusy {true};
	/// A script paused the mind
	bool paused {false};
	/// A script holds the hand of the god
	bool scriptHold {false};
	/// The mind looked at its whole tree at least one time
	bool workedOut {false};
	/// How many times the mind wanted each miracle: a worship site charges the miracle wanted most
	std::array<uint32_t, k_MagicTypes> castRequested {};
	/// For each kind of miracle that a creature feels (to impress, kind, angry), the times that the god sent its creature
	/// while the creature knew none of the miracles of that kind
	std::array<uint32_t, 3> untaughtSends {};
	/// The actions that land scripts queued, the first to start first
	std::vector<QueuedAction> queue;
	/// The hand does an action that a land script started. The help of the game does not hold the mind then.
	bool scriptHand {false};
};

/// The tree of a new mind, with each node at its first weight
[[nodiscard]] Mind MakeMind();

/// The mind after the load of a saved game. The save keeps these parts of the mind:
/// - The suppression and the weight of each node.
/// - The pause, the hold of a script, the miracle requests and the queue of script actions.
/// The other parts start again as in a new mind. Thus the turn stamps of the nodes go to 0, and the mind waits one turn.
[[nodiscard]] Mind StartAgain(const Mind& saved);

/// The names of the kinds of node, in the order of NodeKind. All the gods of a game share them. Each entry has the
/// bytes of one name, with room for the first name and its zero byte. A personality load can change the bytes.
using NodeNames = std::vector<std::string>;
/// The names that a game starts with
[[nodiscard]] NodeNames MakeNodeNames();
/// The names that a game starts with, made one time
[[nodiscard]] const NodeNames& FirstNodeNames();
/// The name of a kind of node: the bytes before the first zero byte. A name with no zero byte fills all of its entry.
[[nodiscard]] std::string_view NameOf(const NodeNames& names, NodeKind kind);

/// What the mind reads and changes in the game
class World
{
public:
	virtual ~World() = default;
	/// How much the node is wanted, from its own rule. The rule can set the node's object and amount.
	virtual float Intensity(Mind& mind, uint16_t node) = 0;
	/// The node starts its action on the hand. False when it cannot.
	virtual bool Perform(Mind& mind, uint16_t node) = 0;
	/// The hand has steps of an action left
	[[nodiscard]] virtual bool ActionBusy() const = 0;
	/// The hand does a turn of its action. False when the action ended badly.
	virtual bool StepAction() = 0;
	/// The hand stops its action
	virtual void ClearAction() = 0;
	/// The hand starts the steps of a new action
	virtual void StartAction() = 0;
	/// The action of the hand may not be cut short by a stronger need
	[[nodiscard]] virtual bool ActionLocked() const = 0;
	virtual void UnlockAction() = 0;
	/// The place of the thing when it is a script marker: none for another thing
	[[nodiscard]] virtual std::optional<glm::vec3> MarkerPlace(entt::entity thing) const = 0;
	/// The help of the game holds the mind
	[[nodiscard]] virtual bool HeldByHelp() const = 0;
	/// The names of the kinds of node that a land script uses to find a node
	[[nodiscard]] virtual const NodeNames& Names() const { return FirstNodeNames(); }
};

/// Looks at the node and the tree below it, and gives how much the node is wanted
float WorkOut(Mind& mind, World& world, uint16_t node, uint32_t turn);
/// Looks at the top nodes whose time has come, and gives how much the strongest is wanted
float WorkOutTopLayer(Mind& mind, World& world, uint32_t turn);
/// Starts the action of the leaf that the root chose. False when it cannot.
bool PerformDecided(Mind& mind, World& world, uint32_t turn);

/// The turn of a mind that is switched on and started on the land
void ProcessTurn(Mind& mind, World& world, uint32_t turn);

/// What the god reads in the game when it looks for things near its hand
class SeenWorld
{
public:
	virtual ~SeenWorld() = default;
	/// The things in a cell of the map, in the order that a search finds them
	[[nodiscard]] virtual std::vector<entt::entity> ThingsIn(glm::ivec2 cell) const = 0;
	/// What the god keeps in its list for a thing: the forest of a tree, or the tree when it has no forest; a seed of a
	/// miracle. None for another thing.
	[[nodiscard]] virtual entt::entity SeenAs(entt::entity thing) const = 0;
	/// The thing is in the world, and it is not a villager on its way to dying
	[[nodiscard]] virtual bool Available(entt::entity thing) const = 0;
};

/// The god adds the things near its hand to its list of seen things, the newest first. A long list first forgets the
/// things that the god saw long ago.
void SeeNearHand(std::vector<SeenThing>& seen, const map_coords::MapCoords& hand, uint32_t turn, const SeenWorld& world);

/// A land script makes the god do the action of the first node that the name finds, with two things and a value.
/// False when no node has the name or the action cannot start.
bool ForceAction(Mind& mind, World& world, std::string_view name, entt::entity first, entt::entity second, float value,
                 uint32_t turn);
/// A land script puts an action at the end of the queue of the god
void QueueAction(Mind& mind, std::string_view name, entt::entity first, entt::entity second, uint32_t turn);
/// The god starts the first action of its queue and removes it from the queue. False when the queue is empty or the
/// action cannot start.
bool ForceNext(Mind& mind, World& world, uint32_t turn);

/// A land script sets the weight of each node that has the name. False when no node has it.
bool SetPersonality(Mind& mind, std::string_view name, float weight, const NodeNames& names = FirstNodeNames());
/// A land script sets the suppression of each node that has the name. False when no node has it.
bool SetSuppression(Mind& mind, std::string_view name, float suppression, const NodeNames& names = FirstNodeNames());
/// The nodes that a name finds, in the order that the game finds them. The name of a node ends at its last colon. A
/// longer name that starts with the name of a node also finds the node.
[[nodiscard]] std::vector<uint16_t> FindNodes(const Mind& mind, std::string_view name,
                                              const NodeNames& names = FirstNodeNames());

/// The name of a kind of node in a personality file: the name for the land scripts, then a colon, the number of its
/// text and a space. The root has only its name.
[[nodiscard]] std::string PersonalityName(NodeKind kind);
/// The personality file of a god, as a land script saves it. For each node, root first and then each branch of a child
/// in turn, the file has the name of the node with a zero byte after it, then the weight in four bytes, low byte first.
/// A name with no zero byte in its entry goes into the file without a zero byte.
[[nodiscard]] std::vector<uint8_t> PersonalityFile(const Mind& mind, const NodeNames& names = FirstNodeNames());
/// A land script loads a personality file into the god. The load reads the nodes in the order of the save, and does
/// not compare the names in the file with the names of the nodes:
/// - For each node, it reads as many bytes as the name of the node has now, with its zero byte. These bytes become the
///   name of the kind of node for all the gods.
/// - Then it reads the four bytes of the weight.
/// - When the file ends, the load stops. A name or a weight that the file cuts keeps the bytes that the file does not
///   give.
void ReadPersonalityFile(Mind& mind, NodeNames& names, std::span<const uint8_t> bytes);

} // namespace openblack::computer_mind
