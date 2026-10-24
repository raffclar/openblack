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
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>

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
	/// A number that a node can keep for its action: the food that a town wants more than it has
	int32_t amount {0};
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
};

/// The tree of a new mind, with each node at its first weight
[[nodiscard]] Mind MakeMind();

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
	/// The hand starts the next action that a script queued. False when there is none.
	virtual bool ForceQueued() = 0;
	/// The help of the game holds the mind
	[[nodiscard]] virtual bool HeldByHelp() const = 0;
};

/// Looks at the node and the tree below it, and gives how much the node is wanted
float WorkOut(Mind& mind, World& world, uint16_t node, uint32_t turn);
/// Looks at the top nodes whose time has come, and gives how much the strongest is wanted
float WorkOutTopLayer(Mind& mind, World& world, uint32_t turn);
/// Starts the action of the leaf that the root chose. False when it cannot.
bool PerformDecided(Mind& mind, World& world, uint32_t turn);

/// The turn of a mind that is switched on and started on the land
void ProcessTurn(Mind& mind, World& world, uint32_t turn);

/// A land script sets the weight of each node that has the name. False when no node has it.
bool SetPersonality(Mind& mind, std::string_view name, float weight);
/// A land script sets the suppression of each node that has the name. False when no node has it.
bool SetSuppression(Mind& mind, std::string_view name, float suppression);
/// The nodes that a name finds, in the order that the game finds them
[[nodiscard]] std::vector<uint16_t> FindNodes(const Mind& mind, std::string_view name);

} // namespace openblack::computer_mind
