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

#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureCastAgenda.h"
#include "Creature/CreatureDrink.h"
#include "Creature/CreatureFriendAgendas.h"
#include "Creature/CreatureIdleMind.h"

/// The actions of the game's table that a creature can carry out here, and the agenda of steps each becomes. The planner
/// only weighs these; the rest (miracles, building, dancing and so on) wait for the systems they need.
namespace openblack::creature_plan_actions
{
/// The kind of thing an action is done to
enum class Target : uint8_t
{
	/// Nothing: done on the spot
	None,
	/// Anything it can eat, or only living things it can eat
	Food,
	/// Anything it can pick up
	Pickable,
	/// Anything it can knock down, or any tree
	Destroyable,
	Tree,
	/// Villagers, villagers hurt enough to be healed, other creatures, or either
	Villager,
	HurtVillager,
	Creature,
	Living,
	/// A creature that is not dead or lying out cold, to fight
	FightableCreature,
	/// A creature that seems friendly, to amuse with a spell
	FriendlyCreature,
	/// Another creature that has no partner in a joint action now, and one that has
	CreatureFreeForJoint,
	CreatureNotFreeForJoint,
	/// What frightens creatures: other creatures, bats, vultures and lions, and miracles
	Frightening,
	/// Anything at all it can see
	Anything,
	/// A thing it can look over: a thing that lives, grows, lies about or is set into the land, but not a creature, a
	/// field or a fish farm. Never a miracle or a whole thing.
	Examinable,
	/// A villager that does something that the creature finds interesting: its work teaches a skill that the creature
	/// does not know
	InterestingVillager,
	/// Anything on fire, or anything not on fire
	Burning,
	Unburnt,
	/// A storage pit, built or not
	StoragePit,
	/// A store that food can be cast into: a storage pit or a worship site that stands fully built
	BuiltFoodStore,
	/// A pile of food the food miracle made, away from any storage pit
	MagicFoodPile,
	/// A toy it can pick up, or only a ball (the football or the toy ball)
	Toy,
	ToyBall,
	/// Anything alive: villagers, animals, birds and creatures
	AnyLiving,
	/// What thrown things can knock down: the living while they live, and what is built or set into the land
	Stonable,
	/// What it can pick up that does damage when thrown: statics other than toys, rocks and dead trees
	ThrowingDamage,
	/// What it may throw into the sea in play
	SeaThrowable,
	/// A tree it can pick up, or one it can pick up to plant somewhere else: not one of the last trees it planted, nor
	/// one near where the player did what it last copied
	PickableTree,
	PlantableTree,
	/// What it can pick up and throw to impress
	ThrowableToImpress,
	/// A rock (bonfires too) it can pick up, or any rock
	PickableRock,
	Rock,
	/// What it can stomp on, what it can kick, and a living thing that it can knock down with a rock and pick up to eat
	Stompable,
	Kickable,
	StonableAndPickable,
	/// A cuddly toy, a die, or a toy (but not the football) more than 50 from the creature's home
	CuddlyToy,
	Die,
	ToyAwayFromHome,
	/// A thing that the creature can bring home: more than 50 m from its home, not alive, and a thing that it can pick up
	BringableHome,
	/// A thing that the creature can steal: a toy, a scaffold or a one-shot miracle that it can pick up, out of the
	/// influence of its player
	Stealable,
	/// A built storage pit of the player of the creature; a built storage pit of another player, or of no player; a
	/// field of another player, or of no player (see creature_town_theft)
	OwnStoragePit,
	OthersStoragePit,
	OthersField,
	/// A thing still going up (a building, a worship site or the temple), or a building that its town repairs and that
	/// is below its full life; or a house (any of a town's buildings) below its full life
	BuildingBeingBuilt,
	NeedsRepair,
	/// What a creature can poo by: a thing that is not alive and is taller than k_PooedOnTallerThan, but not a field or a
	/// fish farm
	PooedOn,
	/// A worship site, a workshop, or a storage pit that stands fully built
	WorshipSite,
	Workshop,
	BuiltStoragePit,
	/// A tree. A field that has no food to give: not ripe, or ripe and empty. A tree or such a field.
	AnyTree,
	DryField,
	Waterable,
	/// A one-shot miracle that lies about, or that the creature holds, whose miracle attacks, is kind, or is playful
	OneShotAggressive,
	OneShotCompassionate,
	OneShotPlayful,
	/// A one-shot miracle that lies about, whose miracle restores health
	OneShotRestoreHealth,
	/// A part of a temple, of any player: a temple, or a worship site
	TemplePart,
	/// A villager nearer to the creature than 200 metres, to dance with
	DanceVillager,
	/// The things that a creature eats from: a sown field with food in its crop; a storage pit with food in its food pile,
	/// if the pit is not wider than 15 times the size of the creature; a tree that is as tall as 15 times that size, or
	/// taller
	FieldWithFood,
	StoragePitWithFood,
	TreeBigEnough,
	/// A thing that the creature can pick up and that has lost some of its life, to stroke and heal
	Strokeable,
	/// A pile of wood that the wood miracle made
	MagicWoodPile,
	/// A farm animal (or a tortoise) that the creature can pick up, to drop in a store
	Cow,
	/// A villager that does not dance in worship
	VillagerNotWorshipping,
	/// A villager that the creature may make a breeder: grown up, of a town with fewer than three breeders, and no
	/// disciple yet
	BreederVillager,
	/// A villager more than 40 m from its home
	VillagerFarFromHome,
	/// The totem of a town of the player who watches: the town centre and the town's worship site built, and the
	/// player's temple there
	PlayerTotem,
};
/// A creature poos only by a thing taller than this, in metres
constexpr float k_PooedOnTallerThan = 1.5f;
/// How many kinds of target there are
constexpr size_t k_TargetCount = static_cast<size_t>(Target::PlayerTotem) + 1;

/// How the agenda is made
enum class Build : uint8_t
{
	Eat,
	EatAlive,
	Sleep,
	Poo,
	Fart,
	Puke,
	Drink,
	ExamineByPickingUp,
	ExamineByLooking,
	ExamineByFollowing,
	ThrowAbout,
	Hurl,
	Destroy,
	Stomp,
	StompAndEat,
	StoneAndEat,
	Kick,
	ShowHate,
	SitDown,
	BeIdle,
	HangAround,
	ShowDesire,
	/// An action on the spot, or facing the player, or at what it goes up to
	Emote,
	FaceCameraEmote,
	ApproachEmote,
	/// The actions with a friend: waving at it (which it does too), howling at it, pining for it and looking for it
	WaveAtFriend,
	HowlAtFriend,
	PineForFriend,
	LookForFriend,
	/// The joint actions side by side with a friend, and with a friend at a town (see creature_mind::BeHappyWithFriend
	/// and the others)
	BeHappyWithFriend,
	PooWithFriend,
	SitWithFriend,
	SleepWithFriend,
	DrinkWithFriend,
	HelpTownWithFriend,
	AttackTownWithFriend,
	/// The other joint actions with a friend (see creature_mind::KissFriend and the others)
	KissFriend,
	TalkToFriend,
	TellFriendAJoke,
	DanceWithFriend,
	ArgueWithFriend,
	MopeAboutWithFriend,
	ShowOffToFriend,
	GetAttentionFromFriend,
	TellCreatureToSodOff,
	ExamineOtherCreatureWithFriend,
	SitOnTopOfHillWithFriend,
	EatFromFieldWithFriend,
	PlayThrowingGameWithFriend,
	GiveFriendToy,
	ShowFriendCitadel,
	RunFromObject,
	RunFromPlayer,
	LookAbout,
	/// Casting a miracle at what it is done to: a lightning bolt, a helpful miracle, a spell on another creature, a
	/// miracle that attacks (a tornado, the lightning storm, the power-ups of the explosion), an explosion, a fireball
	CastLightning,
	CastHelpful,
	CastPlayful,
	CastAggressive,
	CastExplosion,
	CastFireball,
	/// The creature casts food into a store from near it, and a shield from where it stands. It casts water on itself to
	/// stop its own fire, and a miracle to impress the people of a villager.
	CastFood,
	CastShield,
	CastWaterOnItself,
	CastToImpress,
	/// The creature casts a spell that it picks at random to amuse another creature
	CastAmusing,
	/// The creature casts a forest at the place where a villager is when it makes the agenda
	CastForest,
	/// The creature casts water at the tree that it uses, for a town
	WaterTreeForTown,
	/// Going to a fish farm's shoal, bringing food out of the sea there and eating it
	FishAndEat,
	/// Bringing food out of the sea, unless it has some in its hand, and throwing it into a storage pit, or putting it
	/// down at its home
	GiveFishToStore,
	TakeFishHome,
	/// Bringing a thing home, bringing food home, and taking wheat from a field home, where the creature puts it down
	BringHome,
	BringFoodHome,
	TakeFieldFoodHome,
	/// Casting the water miracle at a burning thing
	CastWater,
	/// Setting a thing alight by tossing a burning thing at it
	SetFire,
	/// The actions scripts force to stage scenes: looking at a thing for good, or a little without going up to it;
	/// looking at the camera; pointing at a thing or at the camera
	LookForever,
	LookButDontApproach,
	LookAtCamera,
	PointAtThing,
	/// Looking at the camera while a script's cut scene is on, as every creature free to think for itself is made to
	LookAtCameraInCutScene,
	PointAtCamera,
	/// Hurling, sleeping at home, smiling or waving at someone, being frightened on the spot, putting down what it
	/// holds and lying dead for good, as the game builds them whoever chose them
	SleepAtHome,
	/// Walking home and doing a poo there by itself
	PooAtHome,
	SmileAt,
	WaveAt,
	/// Showing off to a thing: going near it and playing two impressive actions
	ShowOff,
	BeFrightened,
	PutDown,
	DeadForever,
	/// Pointing out a lesson's highlight to the player
	PointOutHighlight,
	/// The creature shows the player that it saw a deed of the player, done to the thing
	Notice,
	/// Sleeping in a clear place by a thing, or at home when there is no such place
	SleepByObject,
	/// Taking food from a pile and eating it
	EatFromPile,
	/// The creature takes a handful from the food pile of a storage pit, from a field or from a tree, and eats it
	EatFromStoragePit,
	EatFromField,
	EatFromTree,
	/// Looking another creature over
	InspectCreature,
	/// Fighting another creature: it asks the other creature to fight, and the fight then has the two creatures
	Fight,
	/// Looking at a thing flying through the air
	LookAtFlyingObject,
	/// Looking at the player's hand
	LookAtHand,
	/// Giving the tree it uses to the store as wood, and planting the tree it uses by the thing
	GiveWoodToStore,
	PlantTree,
	/// Throwing what it holds, or else the thing after picking it up, at a point off to one side of it
	PracticeThrow,
	/// Picking up the toy it uses and throwing it at the thing
	ThrowToy,
	/// Picking up the thing, unless it holds a magic mushroom, and eating it
	GetHigh,
	/// Kicking the ball about a few times
	KickBallAround,
	/// Practising a throw with the rock, and breaking it in two as it flies
	BreakRock,
	/// Stroking a cuddly toy, taking a toy home, throwing a die
	StrokeToy,
	TakeToyHome,
	ThrowDie,
	/// Picking up the toy it uses and throwing it to its player's hand
	ThrowToPlayer,
	/// Bringing a tree to a house and mending the house with it
	HelpRepairHouse,
	/// Dancing with the villagers of a town, by the executor's row of the dance table
	DanceWithVillagers,
	/// Dancing alone where it stands
	PracticeDance,
	/// Casting the one-shot miracle that it holds at the thing
	CastOneShotInHand,
	/// Picking up the one-shot miracle that it uses, unless its hand holds something, and casting it at the thing
	PickUpAndCastOneShot,
	/// The actions with its player: go to the hand, point at the hand, show the player how nice the creature thinks the
	/// player is, pray to the player, throw a thing at the camera and watch the player
	GoToHand,
	PointAtHand,
	ShowHowNice,
	PrayToPlayer,
	ThrowAtCamera,
	WatchPlayer,
	/// The actions with its player that first follow the camera: go to the middle of the screen, follow the player, be
	/// cross with the player and show the player a thing. Also the action to be silly with the player.
	GoToMiddleOfScreen,
	FollowPlayer,
	BeCrossWithPlayer,
	ShowPlayerAnObject,
	BeSillyWithPlayer,
	/// Resting to get better where it is, or at home after it goes there; healing itself with the heal miracle; resting on
	/// the spot a while
	RestToGetBetter,
	GoHomeToRecover,
	HealItself,
	RestOnTheSpot,
	/// Casting at itself the one-shot miracle in its hand that restores health, or one that it picks up first
	CastRestoreHealthInHand,
	PickUpAndCastRestoreHealth,
	/// Going home, running home, going into its temple, praying at its temple, and looking at a part of a temple
	GoHome,
	RunHome,
	EnterTemple,
	PrayAtTemple,
	LookAtTemple,
	/// The emotes: howling at the player, being sad, pulling silly faces at a villager, being pathetic to the player,
	/// stroking a hurt thing to heal it, and looking at the sun or the moon
	Howl,
	BeSad,
	PullSillyFaces,
	BePathetic,
	StrokeAndHeal,
	LookAtSun,
	LookAtMoon,
	/// The town help actions (see creature_town_help): giving a handful of food from a field, fruit from a tree, magic
	/// food or magic wood to a store; putting food from a field or fish by a worship site; dropping a cow in a store;
	/// making a villager a breeder; taking a villager home to sleep; bringing a villager to a worship site; raising and
	/// lowering a totem; frightening, playing with and kissing a villager in play
	GiveFieldFoodToStore,
	GiveFruitToStore,
	GiveMagicFoodToStore,
	GiveMagicWoodToStore,
	PutFieldFoodBySite,
	PutFishBySite,
	DropCowInStore,
	MakeBreeder,
	TakeVillagerHome,
	BringVillagerToSite,
	RaiseTotem,
	LowerTotem,
	FrightenVillagers,
	PlayWithVillager,
	KissVillager,
	/// The actions that use the creature's map of the land: going to a hill to look, going to a hill, going to a hill
	/// and walking along its ridge, exploring a coast or a town, sitting on a beach, looking at the mountains or out to
	/// sea, and throwing a thing into the sea
	GoToHillAndLook,
	GoToHillAndSit,
	WalkAlongRidge,
	ExploreCoast,
	ExploreTowns,
	SitDownOnBeach,
	LookAtMountains,
	LookOutToSea,
	ThrowInTheSea,
	/// The creature goes to the nearest coast, turns to the nearest sea and dances alone there
	DanceByTheSea,
	/// The odd actions: showing a lesson that it learnt, looking confused, behaving strangely
	ShowLearntLesson,
	LookConfused,
	BehaveStrangely,
	/// An action whose agenda can never be made
	Never,
	/// Taking a living thing to the altar of the first worship site of its temple, and putting it down there
	Sacrifice,
	/// Stealing a thing and putting it down near the home of the creature
	StealAndPutByTemple,
	/// Copying the player again: the creature makes no steps, but copies again the deed that it saw last
	MimicPlayer,
	/// Dancing outside its player's temple, and around a thing (see creature_temple_dance)
	DanceOutsideTemple,
	DanceAroundThing,
	/// The thefts: a thing to a town of its player, and food or wood from a store of another player (see
	/// creature_town_theft)
	StealAndPutInTown,
	StealFoodFromStore,
	StealWoodFromStore,
};

/// What tells the planner that the creature did an action too recently to choose it again
enum class Recency : uint8_t
{
	/// Nothing
	None,
	/// Fewer whole seconds than two have gone by since it last did the action
	TwoSeconds,
	/// It last showed off to its end 200 game turns ago or less
	ShowedOff,
};

/// The kinds of one-shot miracle that a creature casts for its desires
enum class OneShotKind : uint8_t
{
	Aggressive,
	Compassionate,
	Playful,
	RestoreHealth,
};

/// A one-shot miracle as the creature sees it: the first miracle of its seed, and the kinds of that miracle
struct OneShot
{
	uint32_t magicType {0};
	bool aggressive {false};
	bool compassionate {false};
	bool playful {false};
	bool restoresHealth {false};

	[[nodiscard]] bool Is(OneShotKind kind) const;
};

struct Executor
{
	/// The action's name in the game's table
	std::string_view action;
	Target target {Target::None};
	/// What it uses to do the action, which it takes up first: the thing it throws, or the tree it plants or gives
	Target use {Target::None};
	/// The thing it is done to is never what it holds
	bool notWhatItHolds {false};
	Build build {Build::Emote};
	/// The animation played, for the builds that play one
	size_t animation {0};
	/// The row of the dance table, for a dance
	uint32_t danceType {0};
	creature_mind::Activity activity {creature_mind::Activity::Planned};
	/// The miracle that the agenda casts, when the creature table gives the action no miracle. The creature then never
	/// chooses the action itself, but a script can make it do the action.
	std::optional<uint32_t> castMagic;
	/// The kind of one-shot miracle that the action casts, for the actions that cast one
	OneShotKind oneShot {OneShotKind::Aggressive};
	/// The action takes the creature away from its home: a young creature does not do it
	bool leavesHome {false};
	/// What stops the creature choosing the action again too soon
	Recency recency {Recency::None};
};

/// From this stage of growing up the creature is old enough to leave its home
constexpr uint32_t k_LeaveHomeStage = 3;
/// Some joint actions with a friend start only when a desire of the creature is stronger than this, or its sadness is
/// less
constexpr float k_SlightDesire = 0.1f;

/// Every action that can be carried out
[[nodiscard]] std::span<const Executor> All();
[[nodiscard]] const Executor* For(std::string_view action);

/// What the agenda needs to know of where the creature is
struct Situation
{
	/// Where the player looks from
	std::optional<glm::vec2> camera;
	/// Where the creature drinks, none when it finds no place to drink
	std::optional<creature_drink::Place> drink;
	/// Fishing at the nearest fish farm: its shoal (the farm itself without one), where the creature would stand to fish
	/// for itself (the shoal, or the nearest place it can stand near enough to it; none for none), how near it goes,
	/// whether it already has food in its hand, and its height. None with no farm near.
	struct Fishing
	{
		glm::vec2 shoal {0.0f};
		std::optional<glm::vec2> standAt;
		float arriveWithin {0.0f};
		bool holdingFood {false};
		float height {0.0f};
	};
	std::optional<Fishing> fishing;
	/// Where its home is, none for a creature without one
	std::optional<glm::vec2> home;
	/// The food pile of the storage pit that the action is done to, by its entity's number, and where it is. None when
	/// the pit has no food pile, or the action is not done to a storage pit.
	struct FoodPile
	{
		uint32_t pile {0};
		glm::vec2 point {0.0f};
	};
	std::optional<FoodPile> storeFood;
	/// The thing the action uses, by its entity's number, as a burning thing to set something alight with; and whether
	/// its hand holds something already
	std::optional<uint32_t> instrument;
	bool handFull {false};
	/// What the desire of the plan is about (the town that the creature helps, say), by its entity's number. None when
	/// the plan is about nothing.
	std::optional<uint32_t> activity;
	/// Whether what its hand holds is a tree, or a magic mushroom
	bool holdingTree {false};
	bool holdingMushroom {false};
	/// Whether what its hand holds is a rock
	bool holdingRock {false};
	/// Whether it can eat what its hand holds as it is, with no pick-up first
	bool holdingEatable {false};
	/// The one-shot miracle that its hand holds, and the one-shot miracle that the action uses; none when it is not one
	std::optional<OneShot> heldOneShot;
	std::optional<OneShot> usedOneShot;
	/// The creature itself, by its entity's number, and where it stands
	uint32_t self {0};
	/// The stage of growing up of the creature
	uint32_t stage {k_LeaveHomeStage};
	glm::vec2 position {0.0f};
	/// Where the thing it uses is, and its radius on the ground
	glm::vec2 instrumentPoint {0.0f};
	float instrumentRadius {0.0f};
	/// Where the nearest of its player's hands is, none when its player has no hand here
	std::optional<glm::vec3> playerHand;
	/// Where the camera is, whether the creature has a player (whose camera it can look to), whether a script controls
	/// it, its radius on the ground, and a chance from 0 to 1 for what varies
	std::optional<glm::vec3> eye;
	bool hasPlayer {false};
	bool controlledByScript {false};
	/// Whether it has enough life to start a fight
	bool healthyEnoughToFight {false};
	float radius {0.0f};
	/// Its height, and the height and radius on the ground of the thing the action is done to
	float height {0.0f};
	float thingHeight {0.0f};
	float thingRadius {0.0f};
	/// Whether the thing the action is done to is alive: a villager, an animal, a bird or a creature
	bool thingLiving {false};
	/// How far the camera is from it
	float eyeDistance {0.0f};
	/// Draws from the random numbers of the game as the agenda is made: the extra time of a look that does not go near
	/// the thing (0 to 1.2 seconds), of a smile (0 to 4 seconds) and of a sit (0 to 5 seconds)
	float lookExtraSeconds {0.0f};
	float smileExtraSeconds {0.0f};
	float sitExtraSeconds {0.0f};
	/// A clear place by the thing to sleep there, none when there is none (or it wasn't looked for)
	std::optional<glm::vec2> clearArea;
	/// The places that the creature uses when it hangs around at home, and the length of a game turn in seconds
	creature_mind::HangAroundPlaces hangAround;
	float turnSeconds {0.0f};
	/// A dance with villagers: the clear place for it (none when there is none or it wasn't looked for), the fewest
	/// dancers its row of the dance table asks for, and the beats of its length
	std::optional<glm::vec2> danceArea;
	uint32_t fewestDancers {0};
	uint32_t danceBeats {0};
	/// Whether it is away from its player's hand, and whether that hand moves (its attitude to its player is below)
	bool awayFromHand {false};
	bool handMoving {false};
	/// Whether the camera of its player looks at it
	bool lookedAt {false};
	/// How far its home is, in metres, none without a home; and whether its player has a temple
	std::optional<float> homeDistance;
	bool hasTemple {false};
	/// The heart of its player's temple, on the ground, and the heart's radius on the ground; none without a temple
	struct TempleHeart
	{
		/// The temple, by its entity's number
		uint32_t temple {0};
		glm::vec3 point {0.0f};
		float radius {0.0f};
	};
	std::optional<TempleHeart> templeHeart;
	/// Where it stands, with its height
	glm::vec3 standing {0.0f};
	/// No other creature is near enough to frighten it (see creature_mind::NothingFrighteningNear)
	bool nothingFrighteningNear {true};
	/// Whether the sky shows night, and the place where the creature looks for the moon
	bool night {false};
	glm::vec3 moon {0.0f};
	/// Whether its alignment is evil (below 0)
	bool evil {false};
	/// Whether it wears a leash
	bool leashed {false};
	/// How far its player's hand nearest it is, none without a hand; and what it feels about its player, -1 to 1
	std::optional<float> handDistance;
	float attitudeToPlayer {0.0f};
	/// Whether what its hand holds is a thing it can stroke
	bool holdingStrokeable {false};
	/// Game turns a second
	float turnsPerSecond {10.0f};
	/// For the town help actions: whether what its hand holds is a cow or a villager; where the worship site that the
	/// action is done to keeps its food; and the home of the villager that the action is done to, by its entity's number
	bool holdingCow {false};
	bool holdingVillager {false};
	std::optional<glm::vec2> siteFood;
	std::optional<uint32_t> villagerHome;
	/// For the actions that use the creature's map of the land: the place that the creature goes to or looks at, and
	/// the sea that it looks at or throws into. None when the map gives no such place.
	std::optional<glm::vec2> explorePlace;
	std::optional<glm::vec2> exploreSea;
	/// For the sacrifice: whether what its hand holds is alive, and where the altar of the first worship site of its
	/// temple stands, on the ground. The altar is none when the temple has no worship site.
	bool holdingLiving {false};
	std::optional<glm::vec3> sacrificeAltar;
	/// For the theft that takes a thing to a town: the storage pit of the nearest town of its player, by its entity's
	/// number. None when the player has no town, or that town has no storage pit.
	std::optional<uint32_t> ownTownPit;
	/// For the joint actions with a friend: how strong its desires to drink, to have a poo and to sleep are, and how
	/// sad it is (the value of the source of sadness)
	float water {0.0f};
	float pooDesire {0.0f};
	float tiredness {0.0f};
	float sadness {0.0f};
	/// The nearest town, when the creature believes something about it: whether it wants to attack it more than to
	/// help it. None without a town, or without a belief about the nearest town.
	std::optional<bool> attackNearestTown;
	/// The friend of a joint action: where it stands, how tall it is, where its body is, and its radius on the ground
	struct Friend
	{
		glm::vec2 point {0.0f};
		float height {0.0f};
		glm::vec3 position {0.0f};
		float radius {0.0f};
	};
	std::optional<Friend> friendly;
	/// The town of a joint action with a town, none without a town that has a storage pit
	std::optional<creature_mind::FriendsTown> friendsTown;
	/// The distance that each of the two kissers stands from the point halfway between them
	std::optional<float> kissingDistance;
	/// Whether the creature can see the friend
	bool seesFriend {true};
	/// The field nearest the creature that it believes in, for a meal with a friend
	std::optional<FoodPile> friendsField;
	/// Where the head of the creature is, and where its home is on the ground
	std::optional<glm::vec3> head;
	std::optional<glm::vec3> homePlace;
};
/// What a casting action casts, from its row of the game's table and the creature
struct CastInfo
{
	uint32_t magicType {0};
	/// The gesture drawn before casting, 0 for none
	uint32_t gesture {0};
	/// The creature's height
	float height {0.0f};
	/// Whether what it is cast at is living
	bool targetLiving {false};
	/// For a fireball, and for water on itself: the middle of the clear area near the creature that it goes to first,
	/// none when there is none
	std::optional<glm::vec2> clearArea;
	/// Where the thing is at the time the creature makes the agenda, for a miracle that goes to a place
	std::optional<glm::vec3> place;
	/// To amuse another creature: the spells on creatures that it knows and can pay for, in the order of their types
	std::vector<creature_mind::AmusingSpell> amusingSpells;
};
/// Whether an action casts a miracle
[[nodiscard]] bool IsCast(const Executor& executor);
/// Whether an action of the game's table, by its name there, needs a thing to use as well as the thing it acts on
[[nodiscard]] bool NeedsThingToUse(std::string_view action);

/// Whether an action can be carried out now, in this situation, before any thing is chosen
[[nodiscard]] bool Possible(const Executor& executor, const Situation& situation);
/// Whether the agenda of an action can be made in this situation. The planner chooses only possible actions. A script
/// can force an action that is not possible. Then the creature does the action when the situation gives what the agenda
/// needs. For example, a creature with no temple sleeps at home, at the place where it was made.
[[nodiscard]] bool Buildable(const Executor& executor, const Situation& situation);
/// Whether the creature may choose an action for itself in this situation: the conditions that the game's table puts on
/// some actions (by day or by night, near or far from home, with the player's hand near). A script can make it do the
/// action without them.
[[nodiscard]] bool Allowed(const Executor& executor, const Situation& situation);
/// Whether the creature did an action too recently to choose it again: the turns since it last did it, the game turn of
/// the last time it showed off to its end (0 for never) and the game turn now
[[nodiscard]] bool DoneTooRecently(const Executor& executor, uint32_t turnsSinceDone, uint32_t lastShowOffTurn, uint32_t turn,
                                   uint32_t turnsPerSecond);
/// The agenda for an action on a thing (by its entity number, and where it is), if it can be done
[[nodiscard]] std::optional<std::vector<creature_mind::Step>> Agenda(const Executor& executor, std::optional<uint32_t> object,
                                                                     glm::vec2 objectPoint, const Situation& situation,
                                                                     const creature_mind::Random& random,
                                                                     const std::optional<CastInfo>& cast = std::nullopt);

/// Whether a creature can poo by a thing. The thing must not be alive and must not be a field or a fish farm. It must
/// also be taller than k_PooedOnTallerThan.
[[nodiscard]] bool CanPooBy(bool alive, bool fieldOrFishFarm, float height);

} // namespace openblack::creature_plan_actions
