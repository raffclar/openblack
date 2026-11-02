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
#include <optional>
#include <span>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/AllMeshes.h"
#include "Enums.h"

namespace openblack
{
#pragma pack(push, 1)
// The layout checks below take offsetof of the tables, which aren't standard layout: compilers that warn of it allow it
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

struct GTribeInfo
{
	Tribe tribeType;
	WorshipSiteInfo worshipSiteInfo;
	float percentFemaleInTown;
};

struct GObjectInfo
{
	ObjectType type;
	AlignmentType alignmentType;
	std::array<char, 0x30> debugString;
	SoundCollisionType collideSound;
	ImmersionEffectType immersion;
	HelpText helpStartEnum;
	HelpText helpEndEnum;
	HelpSystemMessageSet helpMessage;
	HelpSystemCondition helpCondition;
	HelpSystemMessageSet helpInHand;
	HelpSystemCondition handCondition;
	float foodValue;
	uint32_t woodValue;
	FoodType foodType;
	float defenceEffectBurn;
	float defenceEffectCrush;
	float defenceEffectHit;
	float defenceEffectHeal;
	float defenceEffectFlyAway;
	float defenceEffectAlignmentModification;
	float defenceEffectBeliefModification;
	float defenceMultiplierBurn;
	float defenceMultiplierCrush;
	float defenceMultiplierHit;
	float defenceMultiplierHeal;
	float defenceMultiplierFlyAway;
	float defenceMultiplierAlignmentModification;
	float defenceMultiplierBeliefModification;
	float weight;
	float heatCapacity;
	float combustionTemperature;
	float burningPriority;
	uint32_t canCreatureUseForBuilding;
	uint32_t canCreatureInteractWithMe;
	uint32_t canCreatureAttackMe;
	uint32_t canCreaturePlayWithMe;
	uint32_t canCreatureInspectMe;
	uint32_t canCreatureGiveMeToLiving;
	uint32_t canCreatureBringMeBackToTheCitadel;
	VillagerStates villagerInteractState;
	VillagerStates villagerInteractStateForBuilding;
	float villagerInteractDesire;
	float sacrificeValue;
	float impressiveValue;
	float aggressorValue;
	float villagerImpressiveValue;
	float artifactMultiplier;
	float drawImportance;
	float computerAttackDesire;
};

struct GMiscInfo: GObjectInfo
{
	MeshId normal;
};

struct GMobileInfo: GObjectInfo
{
	uint32_t dummy;
};

struct SpeedGroup
{
	SpeedState speedDefault;
	SpeedState speedFleeing;
	SpeedState speed2;
	SpeedState speed3;
	SpeedState speed4;
	SpeedState speed5;
};

struct GMobileWallHugInfo: GMobileInfo
{
	SpeedGroup speedGroup;
	CollideType collideType;
};

struct IsReacting
{
	uint32_t isFleeingFromObject;
	uint32_t isLookingAtObject;
	uint32_t isFollowingObject;
	uint32_t isFleeingFromSpell;
	uint32_t isLookingAtSpell;
	uint32_t isFollowingSpell;
	uint32_t isReactingToCreature;
	uint32_t isReactingToFood;
	uint32_t isReactingToMagicTree;
	uint32_t isReactingToFlyingObject;
	uint32_t isReactingToAbodeBurning;
	uint32_t isReactingToBall;
	uint32_t isReactingToWood;
	uint32_t isReactingToMagicShield;
	uint32_t isReactingToGift;
	uint32_t isReactingToNewBuilding;
	uint32_t isReactingToHandPickUp;
	uint32_t isReactingToHandUsingTotem;
	uint32_t isReactingToObjectCrushed;
	uint32_t isReactingToFight;
	uint32_t isReactingToTeleport;
	uint32_t isReactingToNiceSpell;
	uint32_t isReactingToHandPuttingStuffInStoragePit;
	uint32_t isReactingToDeath;
	uint32_t isReactingToDroppedByHand;
	uint32_t isReactingToFainting;
	uint32_t isReactingToConfused;
	uint32_t isReactingToFallingTree;
	uint32_t isFleeingFromPredator;
	uint32_t isReactingInCrowd;
	uint32_t isReactingToBreeder;
	uint32_t isReactingToTownCelebration;
	uint32_t isReactingToVillagerInHand;
	uint32_t isReactingToBurningThingInHand;
	uint32_t isReactingToMagicWaterPuttingOutFire;
	uint32_t isReactingToMagicShieldStruck;
	uint32_t isReactingToMagicShieldDestroyed;
	uint32_t isReactingToImpressiveSpell;
	uint32_t isReactingToScaffold;
	uint32_t isReactingToMissionary;
	uint32_t isReactingToFightWon;
};

struct GLivingInfo: GMobileWallHugInfo
{
	LivingType creatureType;
	LivingStates moveState;
	float life;
	float strength;
	float defence;
	uint32_t startAge;
	uint32_t grownUpAge;
	uint32_t oldAge;
	uint32_t retirementAge;
	IsReacting isReacting;
	uint32_t isShepherdable;
	uint32_t minFlockingValue;
	uint32_t maxFlockingValue;
};

struct AgeToScale
{
	std::array<float, 20> values;
};

struct GAnimalInfo: GLivingInfo
{
	AnimalInfo animalInfo;
	SexType sex;
	MeshId high;
	MeshId std;
	MeshId low;
	AnimId defaultAnim;
	uint32_t hunger;
	uint32_t thirst;
	uint32_t sleep;
	uint32_t needToBreed;
	float flockDistance;
	TurnangleStates turnAngle;
	TurnangleStates viewAngle;
	uint32_t playerCanPickUp;
	uint32_t needsTown;
	FoodType needsFoodTypes;
	uint32_t flocksCanMerge;
	float stalkingDistance;
	float attackDistance;
	float huntingDistance;
	uint32_t siSightDistance;
	uint32_t farSightDistance;
	uint32_t nearSightDistance;
	uint32_t chaseTime;
	uint32_t environment;
	uint32_t domainInnerRadius;
	uint32_t domainRadius;
	uint32_t maxFlockSize;
	uint32_t stayTime;
	float altitudeMin;
	float altitudeMax;
	float altitudeVariance;
	float altitudeMovementChange;
	float altitudeNormal;
	AgeToScale ageToScale;
};

struct GMobileObjectInfo: GMobileInfo
{
	MobileObjectInfo mobileType;
	MeshId meshId;
	float startScale;
	float finalScale;
};

struct GPotInfo: GMobileObjectInfo
{
	PotType potType;
	ResourceType resourceType;
	uint32_t maxAmountInPot;
	uint32_t scaleEvery;
	PotInfo nextPotForResource;
	Reaction associatedReaction;
	uint32_t canBecomeAPhysicsObject;
	uint32_t amountPickedUpInitially;
	uint32_t amountPickedUpPerTurn;
	uint32_t amountPickedUpPerTurnEnd;
	uint32_t maxAmountCanBePickedUp;
	float multiPickUpRampTime;
};

struct GFootballPositionInfo
{
	glm::vec2 offset;
	VillagerStates startState;
	VillagerStates state;
	uint32_t side;
};

struct GBallInfo: GMobileObjectInfo
{
	uint32_t defaultSpeed;
	CollideType collideType;
	uint32_t strength;
	float defence;
	float kickXZVelocity;
	float kickYVelocity;
	float shootXZVelocity;
	float shootYVelocity;
	float dribbleXZVelocity;
	float dribbleYVelocity;
	float minDistanceToFindPlayer;
};

struct GSoundInfo
{
	float weatherPercentageForMaxFade;
	float weatherPercentageForMaxWeatherVolume;
	float radiusForMaxAtmosVolume;
	float radiusForMinAtmosVolume;
	float fadeOutStart;
	float normalAtmosFadeStartHeight;
	float normalAtmosFadeEndHeight;
	float atmosphereHeight;
	float atmosphereMaxVolHeight;
	float spaceHeight; // AtmosphereNoVolHeight
	float atmosVolumeStep;
	float townTriggerDistance;
	float townTriggerOffDistance;
	float noBlocksForFullAtmos;
	float distanceFromNearestAtmosForFadeStart;
};

struct GMagicFireBallInfo: GObjectInfo
{
	float initialTemperature;
	float deletionTemperature;
	float catchIncreaseFactor;
};

struct GDanceInfo
{
	uint32_t duration;
	uint32_t totalNumBeats;
	std::array<char, 0x40> debugText;
	std::array<char, 0x40> fileName;
	uint32_t startsAutomatically;
	glm::vec2 areaRequired;
	uint32_t minimumDancers;
	uint32_t maximumDancers;
	float impressiveness;
};

struct GSpecialVillagerInfo
{
	std::array<char, 0x30> name;
	uint32_t age;
	SexType sex;
	VillagerJob job;
	uint32_t married;
	Tribe tribe;
	AnimalInfo pet;
	uint32_t faceNumber;
};

struct CreatureDevelopmentPhaseEntry
{
	std::array<char, 0x30> name;
	/// At this distance from home, a creature at this stage wants to go home most
	float goHomeDistance;
	float field0x34;
	float field0x38;
	/// When the creature moves to a phase, it starts the desires that this phase and each phase before it start, and
	/// stops the desires that they stop, phase by phase. An empty slot holds no desire.
	std::array<uint32_t, 10> desiresOn;
	std::array<uint32_t, 4> desiresOff;
};
static_assert(offsetof(CreatureDevelopmentPhaseEntry, goHomeDistance) == 0x30);
static_assert(offsetof(CreatureDevelopmentPhaseEntry, desiresOn) == 0x3c);
static_assert(offsetof(CreatureDevelopmentPhaseEntry, desiresOff) == 0x64);
static_assert(sizeof(CreatureDevelopmentPhaseEntry) == 0x74);

struct CreatureInitialDesireInfo
{
	/// The kinds of source that feed the desire, in their order. A kind past the last kind is an empty slot.
	std::array<uint32_t, 8> sources;
	/// The desire is about something in itself, as a town to help or a creature to befriend
	uint32_t aboutSomething;
	uint32_t field0x24;
	/// The creature can learn from feedback about the desire
	uint32_t learnable;
	/// An action that the player made the creature do for the desire teaches approval when the creature forgets it
	uint32_t approvesOrders;
	uint32_t field0x30;
	/// A desire of the body
	uint32_t bodyDesire;
	/// A desire that the player can help with
	uint32_t canBeHelped;
	/// The value that feedback keeps the desire's weight near, and the range of the desire's decay
	float initialMax;
	float decayMin;
	float decayMax;
	/// The change that a plan for this desire adds to the alignment of the creature on each game turn
	float alignmentChange;
	float field0x4c;
	/// When the creature sees the hand do something that shows this desire, the desire grows this many times faster
	float seenPlayerActionSpeedUp;
	/// How strongly feedback changes how fast the desire grows
	float lessonDivisor;
	/// How much distance counts against a goal of the desire
	float distanceWeight;
	uint32_t field0x5c;
	float field0x60;
	float field0x64;
	/// The weight the desire starts with, and whether feedback changes it
	float initialWeight;
	uint32_t learnsWeight;
	/// The name of the desire, which the statistics show
	std::array<char, 0x40> name;
	/// The texts of the desire for the creature's thoughts: the desire, trying to satisfy it and satisfying it
	std::array<char, 0x40> desireText;
	/// The short name of the desire, as "impress", which the creature's web page uses
	std::array<char, 0x40> shortName;
	std::array<char, 0x40> tryingText;
	std::array<char, 0x40> doingText;
};
static_assert(offsetof(CreatureInitialDesireInfo, sources) == 0x0);
static_assert(offsetof(CreatureInitialDesireInfo, aboutSomething) == 0x20);
static_assert(offsetof(CreatureInitialDesireInfo, learnable) == 0x28);
static_assert(offsetof(CreatureInitialDesireInfo, approvesOrders) == 0x2c);
static_assert(offsetof(CreatureInitialDesireInfo, bodyDesire) == 0x34);
static_assert(offsetof(CreatureInitialDesireInfo, canBeHelped) == 0x38);
static_assert(offsetof(CreatureInitialDesireInfo, initialMax) == 0x3c);
static_assert(offsetof(CreatureInitialDesireInfo, decayMin) == 0x40);
static_assert(offsetof(CreatureInitialDesireInfo, decayMax) == 0x44);
static_assert(offsetof(CreatureInitialDesireInfo, alignmentChange) == 0x48);
static_assert(offsetof(CreatureInitialDesireInfo, seenPlayerActionSpeedUp) == 0x50);
static_assert(offsetof(CreatureInitialDesireInfo, lessonDivisor) == 0x54);
static_assert(offsetof(CreatureInitialDesireInfo, distanceWeight) == 0x58);
static_assert(offsetof(CreatureInitialDesireInfo, initialWeight) == 0x68);
static_assert(offsetof(CreatureInitialDesireInfo, learnsWeight) == 0x6c);
static_assert(offsetof(CreatureInitialDesireInfo, name) == 0x70);
static_assert(offsetof(CreatureInitialDesireInfo, desireText) == 0xb0);
static_assert(offsetof(CreatureInitialDesireInfo, shortName) == 0xf0);
static_assert(offsetof(CreatureInitialDesireInfo, tryingText) == 0x130);
static_assert(offsetof(CreatureInitialDesireInfo, doingText) == 0x170);
static_assert(sizeof(CreatureInitialDesireInfo) == 0x1b0);

struct CreatureActionKnownAboutEntry
{
	std::array<char, 0x40> name;
	/// The desire that a miracle serves when a creature casts it from a one-shot globe
	uint32_t desire;
	/// How many times a creature must see a miracle before it knows it
	uint32_t timesToSee;
	/// How many seconds a creature must watch a skill, from when it first sees it
	float watchSeconds;
	uint32_t field0x4c;
	/// The first development phase in which the creature can learn it
	uint32_t minPhase;
	/// A computer player can teach it to its creature
	uint32_t teachable;
};
static_assert(offsetof(CreatureActionKnownAboutEntry, name) == 0x0);
static_assert(offsetof(CreatureActionKnownAboutEntry, desire) == 0x40);
static_assert(offsetof(CreatureActionKnownAboutEntry, timesToSee) == 0x44);
static_assert(offsetof(CreatureActionKnownAboutEntry, watchSeconds) == 0x48);
static_assert(offsetof(CreatureActionKnownAboutEntry, minPhase) == 0x50);
static_assert(offsetof(CreatureActionKnownAboutEntry, teachable) == 0x54);
static_assert(sizeof(CreatureActionKnownAboutEntry) == 0x58);

struct CreatureDesireAttributeEntry
{
	/// The attributes of things that the creature's opinion of acting on them for the desire depends on. A slot past
	/// the last attribute is empty.
	std::array<uint32_t, 10> attributes;
};
static_assert(offsetof(CreatureDesireAttributeEntry, attributes) == 0x0);
static_assert(sizeof(CreatureDesireAttributeEntry) == 0x28);

struct GMultiMapFixedInfo: GObjectInfo
{
	GroundInfo groundInfo;
	uint32_t woodRequiredPerBuild;
	uint32_t timeToBuild;
	uint32_t scaffoldsRequired;
	uint32_t maxVillagerNeededToBuild;
	float desireToBeBuilt;
	float desireToBeRepaired;
	float influence;
};

struct GCitadelPartInfo: GMultiMapFixedInfo
{
	CitadelTypes citadelType;
	MeshId meshType;
	float startLife;
	uint32_t startStrength;
	float startDefence;
};

struct GWorshipSiteInfo: GCitadelPartInfo
{
	float radiusFromCitadel;
	MeshId baseMesh;
	PotInfo potForResourceFood;
	PotInfo potForResourceWood;
	float chantsPerVillager;
	float prayerSiteDistance;
	uint32_t maxDancersVisible;
	float chantsToFillBattery;
	float eachVillagerAddToFillBattery;
	float chantsToReserveForMaintaining; ///< read as a float; the file holds an int, so it reads as ~7e-43 (kept)
	float artifactPowerupMultiplier;
};
static_assert(offsetof(GWorshipSiteInfo, chantsPerVillager) == 0x134);
static_assert(offsetof(GWorshipSiteInfo, chantsToFillBattery) == 0x140);
static_assert(offsetof(GWorshipSiteInfo, eachVillagerAddToFillBattery) == 0x144);
static_assert(offsetof(GWorshipSiteInfo, chantsToReserveForMaintaining) == 0x148);

struct GAbodeInfo: GMultiMapFixedInfo
{
	AbodeType abodeType;
	AbodeNumber abodeNumber;
	std::array<char, 0x30> debugString;
	Tribe tribeType;
	MeshId meshId;
	uint32_t canBePhysicallyDamaged;
	float startLife;
	uint32_t startStrength;
	float startDefence;
	uint32_t startInfluence;
	uint32_t maxVillagersInAbode;
	uint32_t maxChildrenInAbode;
	uint32_t startVillagersInAbode;
	uint32_t startChildrenInAbode;
	uint32_t startFood;
	uint32_t startFoodRAnd;
	uint32_t startWood;
	uint32_t startWoodRAnd;
	uint32_t howLongRuinLastsFor;
	PotInfo potForResourceFood;
	PotInfo potForResourceWood;
	float percentTooCrowded;
	MobileObjectInfo producesMobileObject;
	float maxNumMobileObjectsToProduce;
	float timeEachMobileObjectTakesToProduce;
	float emptyAbodeLifeReducer;
	int populationWhenNeeded;
	float thresholdForStopBeingFunctional;
	HelpText toolTipsForBuild;
	HelpText didYouKnow;
	DykCategory dykCategory;

	static AbodeInfo Find(const std::string& name);
	static AbodeInfo Find(Tribe tribe, AbodeNumber abodeNumber);
	/// The first row of the tribe, or of no tribe, with the abode number; none when no row has it. The game finds the
	/// buildings that a scaffold offers in this way.
	[[nodiscard]] static std::optional<AbodeInfo> FindFirst(std::span<const GAbodeInfo> rows, Tribe tribe,
	                                                        AbodeNumber abodeNumber);
};

struct GSingleMapFixedInfo: GObjectInfo
{
	MeshId normal;
};

struct GMapShieldInfo: GSingleMapFixedInfo
{
	float dummy;
};

struct GRewardInfo: GMobileObjectInfo
{
	SpellSeedType seed;
	GestureType gestureType;
	MagicType powerUp;
	HelpText helpTextEnum;
	float beliefValue;
	MobileStaticInfo mobileStatic;
	uint32_t scaffold;
};

struct GContainerInfo
{
	ContainerInfoType containerType;
};

struct GCitadelInfo: GContainerInfo
{
	CitadelHeartInfoType heartInfo;
	glm::ivec2 pensStart;
	uint32_t pensMaxSpiral;
	glm::ivec2 worshipSiteStart;
	uint32_t maxNo;
	uint32_t maxDistanceOfTownFromCitadelOfPygmyTown;
	uint32_t pygmyTownMaxSpiral;
	uint32_t housesInPygmyTown;
	uint32_t importanceOfBeingFarAwayFromOtherTowns;
	int prayerSiteAngle;
	float prayerSiteDistance;
	float virtualInfluenceMaxDistance;
	uint32_t virtualInfluenceMaxGameTicks;
	float virtualInfluenceChantsToDouble;
};

struct CreatureDevelopmentDurationEntry
{
	/// For a species, by its row in the creature tables, the time a creature stays in each development phase. Only
	/// the phase change conditions read it, and nothing reads them.
	std::array<int32_t, 14> durations;
};
static_assert(offsetof(CreatureDevelopmentDurationEntry, durations) == 0x0);
static_assert(sizeof(CreatureDevelopmentDurationEntry) == 0x38);

struct GTownInfo: GContainerInfo
{
	TownInfo townType;
	uint32_t size;
	uint32_t minHousesToSetup;
	uint32_t maxHousesToSetup;
	uint32_t maxSizeInCells;
	uint32_t distance;
	uint32_t split;
	TurnangleStates aroundCitadelStartAngle;
	uint32_t aroundCitadelStepAngle;
	uint32_t aroundCitadelStartDistance;
	uint32_t aroundCitadelEndDistance;
	AbodeInfo townCentre;
	AbodeInfo storagePit;
	uint32_t percentFemale;
	uint32_t processAbodeEvery;
	uint32_t newBuildingEvery;
	std::array<uint32_t, static_cast<size_t>(VillagerJob::_COUNT)> profession;
	float happyFoodMultiplier;
	float influence;
	float numberOfDaysFoodInStoreToBeHappy;
	float happyWoodPerBuildingSite;
	uint32_t playtimeEvery;
	float maxDistanceWallBeBuilt;
	float beliefNeededForWonder;
	float proportionDeadForGraveyard;
	float playerInteactDecay;
	float howImpressedWithFlyingThing;
	float impressedWithFlyingThingRemovedAsSeen;
	float impressedWithFlyingThingAddPerGameTurn;
	float baseBeliefDesireValue;
	float totalWoodNeededToStartBuilding;
	float firstTimeDamageDoneAddition;
	uint32_t totalScaffoldNeededToStartBuilding;
	float builtPercentBeforeWhichYouCanAddScaffolds;
	float beliefInNeutralPlayer;
	float maxAlignmentChangePerGameTurnForDesires;
	std::array<float, 5> storyInfluence;
	float maxForTimeWillWorkUntil;
	float bedTimeMod;
	float foodWantedMultiplier;
	float minimumWoodForDesire;
	float maximumWoodForDesire;
	float numOfBuildingsForDesiredWood;
	float relaxationMod;
	float startingBeliefForCreatedTown;
	uint32_t emptyTownAllowedThisManyVillagers;
	float multiplierForAddingResourceToTown;
	float multiplierForNonOwnerAddingResource;
	uint32_t maxGameturnsForBeliefAfterRemovingFromStoragePit;
	float footballMatchDuration;
	float beliefForFooballMatch;
	float thresholdToStartRepairing;
	uint32_t gameTurnsAfterEmergencyVillagersReact;
	uint32_t numSecondsToIgnoreCreature;
	uint32_t numSecondsBeforeTownForgetsAboutCreature;
	uint32_t maxDistanceForVillagersToGoToTheWorshipsite;
	float minLifeForVillagersToGoToTheWorshipsite;
	float consideredEvilIfMakingDisciplesAndDesireIsLessThan;
	float consideredGoodIfMakingDisciplesAndDesireIsGreaterThan;
	uint32_t numGameTurnsBetweenGettingBeliefForArtifacts;
	float populationForUnmodifiedBelief;
	float multiplierForBeliefFromVillagersDancingAroundArtifact;
	float giveResourceAligmnetChangeMultiplier;
	float takeResourceAligmnetChangeMultiplier;
	float maxDistanceFromCongreationPosThatPeopleChillOut;
	float maxDistanceFromHouseThatPeopleChillOut;
	float maxDistanceThatVillagersWillGoToWorship;
	float amountOverCapacityThatDiscipleBreedersBreedUntil;
	uint32_t populationUnderWhichHelpSpritesWarn;
	float oneOffBeliefForCreature;
	float divisorForAverageDesires;
	float thresholdForAverageDesiresHelpSprites;
	float alignmentChangePerDisciple;
	float maxDistanceForTownForest;
	uint32_t shuffleVillagersEvery;
	float beliefSpeedScaleMultiPlayer;
	std::array<float, 6> beliefSpeedScaleStory;
	float fightWonImpressiveValue;
};

struct GJobInfo
{
	JobInfo job;
	uint32_t restAtHomeAfterWork;
	uint32_t subVisitsAtWhileWork;
	uint32_t timeAtEachSubVisit;
	uint32_t maxGameTurnsToSpendAtJob;
	float minResourceToGet;
	JobActivity spring;
	JobActivity summer;
	JobActivity autumn;
	JobActivity winter;
	uint32_t bestResourceCollectedPerSubVisit;
	uint32_t inJobSpiralArea;
	uint32_t nutureValue;
	float lookForJobLocationMaxDistance;
	uint32_t maxNoLookForsPerTurn;
	EffectInfo nurtureEffect;
	EffectInfo harvestEffect;
	TownDesireInfo satisfyWhichTownDesire;
};

struct GMagicInfo
{
	MagicType magicType;
	ImmersionEffectType immersion;
	uint32_t stopImmersion;
	float perceivedPower;
	ParticleType particleType;
	ImpressiveType impressiveType;
	SpellSeedType spellSeedType;
	GestureType gestureType;
	PowerUpType powerupType;
	CastRuleType castRuleType;
	uint32_t isSpellSeedDrawnInHand;
	uint32_t isSpellRecharged;
	uint32_t isCreatureCastFromAbove;
	uint32_t oneOffSpellIsPlayful;
	uint32_t oneOffSpellIsAggressive;
	uint32_t oneOffSpellIsCompassionate;
	uint32_t oneOffSpellIsToRestoreHealth;
	ParticleType particleTypeInHand;
};
static_assert(sizeof(GMagicInfo) == 0x48);
static_assert(offsetof(GMagicInfo, isSpellRecharged) == 0x2c);
static_assert(offsetof(GMagicInfo, isCreatureCastFromAbove) == 0x30);

struct GMagicGeneralInfo: GMagicInfo
{
};

struct GMagicResourceInfo: GMagicInfo
{
	ResourceType resourceType;
	uint32_t resourceAmountFirstEvent;
	uint32_t resourceAmountPerEvent;
	uint32_t costPerUnit;
	/// A new pile of the food speeds up the people who take from it (it poisons nothing)
	uint32_t speedUp;
};

struct GPBallInfo: GMobileObjectInfo
{
	float kickXZVelocity;
	float kickYVelocity;
};

struct GShotInfo: GMobileObjectInfo
{
	AttackInfo hitEffect;
	int strength;
	int acceleration; // (no longer used)
	int heightAcceleration;
	int velocity; // no longer used
	int finalHeightVelocity;
	SpeedState speed;
};

struct GAlignmentInfo
{
	float animalNice;
	float animalNasty;
	float creature;
	float priest;
	float skeleton;
	float villager;
	float building;
	float plant;
	float field;
	float feature;
	float mobileObject;
	float land;
	float script;
	float unimportant;
};

struct GClimateInfo
{
	std::array<char, 0x30> debugString;
	float rainMinSpring;
	float rainMinSummer;
	float rainMinAutumn;
	float rainMinWinter;
	float rainMaxSpring;
	float rainMaxSummer;
	float rainMaxAutumn;
	float rainMaxWinter;
	float tempMinSpring;
	float tempMinSummer;
	float tempMinAutumn;
	float tempMinWinter;
	float tempMaxSpring;
	float tempMaxSummer;
	float tempMaxAutumn;
	float tempMaxWinter;
	float windMinSpring;
	float windMinSummer;
	float windMinAutumn;
	float windMinWinter;
	float windMaxSpring;
	float windMaxSummer;
	float windMaxAutumn;
	float windMaxWinter;
};

struct GClimateRainInfo
{
	float january;
	float february;
	float march;
	float april;
	float may;
	float june;
	float july;
	float august;
	float september;
	float october;
	float november;
	float december;
	float midnight;
	float oneOClock;
	float twoOClock;
	float threeOClock;
	float fourOClock;
	float fiveOClock;
	float sixOClock;
	float sevenOClock;
	float heightOClock;
	float nineOClock;
	float tenOClock;
	float elevenOClock;
	float midday;
	float thirdtenOClock;
	float fourtenOClock;
	float fivetenOClock;
	float sixtenOClock;
	float seventenOClock;
	float heighttenOClock;
	float ninetenOClock;
	float twentyOClock;
	float twentyoneOClock;
	float twentytwoOClock;
	float twentitreeOClock;
	float natureRainDesire;
};
struct GCreaturePenInfo: GCitadelPartInfo
{
	PenInfo info;
	float startPlacementDistance;
	float endPlacementDistance;
	uint32_t placementAngle;
};

struct GScriptOpposingCreature
{
	/// For a species of the player's creature, the species of each god's creature against it, as the scripts number
	/// them
	std::array<uint32_t, 3> byGod;
};
static_assert(offsetof(GScriptOpposingCreature, byGod) == 0x0);
static_assert(sizeof(GScriptOpposingCreature) == 0xc);

struct GTotemStatueInfo: GMultiMapFixedInfo
{
	MeshId plinth;
};

struct CreatureDesireForType
{
	/// For each species, by its row in the creature tables, the seconds the desire takes to grow by 1 at the start
	std::array<float, 17> increaseSecondsBySpecies;
};
static_assert(offsetof(CreatureDesireForType, increaseSecondsBySpecies) == 0x0);
static_assert(sizeof(CreatureDesireForType) == 0x44);

struct GVortexInfo
{
	ParticleType particleTypePreLandscape;
	ParticleType particleTypePostLandscape;
	ParticleType particleTypeObjectMover;
	ParticleType particleTypeLightMap;
	VortexStateType initialState;
	uint32_t fadeWhenDeleted;
	float baseScale;
	VortexStatType maxToCreateTotalObjects;
	VortexStatType maxToCreateResourceFood;
	VortexStatType maxToCreateResourceWood;
	VortexStatType maxToCreateVillager;
	VortexStatType maxToCreateOneShot;
};

struct GScaffoldInfo: GMobileObjectInfo
{
	float phantomBuildingRotationPerGameTurn;
	uint32_t gameTurnsAfterPlacingCanStillPickUp;
	float maxDistanceForImpressingTowns;
	float proportionOfTownToImpress;
	uint16_t maxNumberForCombining;
};

struct GScriptHighlightInfo: GSingleMapFixedInfo
{
	MeshId active;
	ParticleType particleTypeGlints;
	ParticleType particleTypeActive;
};

struct GSpeedThreshold
{
	SpeedState speedMaxWalk;
	SpeedState speedMaxRun;
};

struct GCreatureInfo: GLivingInfo
{
	/// The species' row in the creature tables, the Giant Ape's first
	uint32_t row;
	float field0x1e8;
	/// How big a new creature of the species is, and how fat, 0 to 1
	float startScale;
	float startFatness;
	/// How a new creature's body starts: full of energy, and neither warm nor cold
	float startEnergy;
	float startWarmth;
	/// The temperature the species is comfortable at; it warms or cools only well away from it
	float comfortTemperature;
	/// The creature ages by one every this many seconds of game time
	uint32_t secondsPerAgeTick;
	/// It grows fast for most of this many minutes of age, then slowly
	float growUpMinutes;
	float field0x208;
	/// Walking while its energy is below this tires it faster
	float lowEnergyThreshold;
	/// How much moving tires it each game turn, before its youth makes that faster
	float exhaustionRate;
	/// It goes from not thirsty to fully thirsty in this many seconds
	float secondsToDehydrate;
	/// Its strength is multiplied by this each game turn
	float strengthDecay;
	/// Carrying something as heavy as itself makes it stronger by half in this many minutes
	float carryStrengthMinutes;
	/// The energy it uses each game turn, less the bigger it is
	float energyDrain;
	/// While hungry, the fat it burns each game turn
	float fatBurn;
	/// How much of what it eats beyond full turns to fat
	float overeatFatFactor;
	/// Asleep, the life it regains and the exhaustion it sheds each game turn
	float sleepHeal;
	float sleepRecover;
	std::array<uint8_t, 0xc> field0x234;
	/// How fast the creature goes, each a fraction of its top speed: slowly (when exhausted), walking and running
	float slowSpeed;
	float walkSpeed;
	float runSpeed;
	/// How fast its head speeds up as it turns to look, in radians a second each second, before its size slows it
	float headTurnRate;
	std::array<uint8_t, 0x14> field0x250;
	/// The least any of its desires can be
	float desireFloor;
	std::array<uint8_t, 0x4> field0x268;
	/// The least and the most seconds that one of its desires can take to grow by 1
	float minDesireIncreaseSeconds;
	float maxDesireIncreaseSeconds;
	std::array<uint8_t, 0xc> field0x274;
	/// When the creature sees the hand do something, the desire that the action shows grows this many times faster
	float seenPlayerActionSpeedUp;
	/// A creature casting a miracle pays for it with its energy: it has this many chants for each of its size and strength
	/// above its energy's floor, below which it can't pay
	float chantsPerEnergy;
	float spellEnergyFloor;
	std::array<uint8_t, 0x4> field0x28c;
	/// The approval that an action the player made the creature do teaches, when the creature forgets the action
	float orderApproval;
	std::array<uint8_t, 0x38> field0x294;
	/// A creature looks another creature over again only once more than this many seconds have gone by since it last did
	float inspectCreatureSeconds;
	std::array<uint8_t, 0x8> field0x2d0;
	/// Running away, the creature goes at least this far from what it flees, and up to 40 further
	float runAwayDistance;
	/// The look of a creature that walks to a place (see creature_look::WhileGoingInfo): the sweep's angle from ahead
	/// and its step, in radians, its nearest and furthest distance and the distance step, and the range in which it
	/// looks only at the place
	float lookSweepHalfAngle;
	float lookSweepAngleStep;
	float lookSweepNearest;
	float lookSweepFurthest;
	float lookSweepDistanceStep;
	/// Running away, it looks back at what it runs from for this many seconds, then looks about for this many
	float runLookBackSeconds;
	float runLookAheadSeconds;
	float lookAtPlaceRange;
	/// The help tells of a problem with its body at most once in this many seconds
	float helpBodyProblemSeconds;
	float field0x300;
	/// The help prompts on how to look after it at most once in this many seconds
	float helpTutorialSeconds;
	/// The help says it is low on energy, hurt, tired or thirsty past these, each with a "very" step
	float helpLowEnergy;
	float helpVeryLowEnergy;
	float helpLowLife;
	float helpVeryLowLife;
	float helpTired;
	float helpVeryTired;
	float helpThirsty;
	float helpVeryThirsty;
	std::array<uint8_t, 0xc> field0x328;
	/// The mesh of its species' icon that stands on its player's town totems (the game's creature info +0x344)
	MeshId totemIcon;
	/// Each turn the change waiting for its alignment, held between -1 and 1, moves it by that share of this
	float alignmentChangePerTurn;
	float field0x33c;
	/// The personality the species is born with, each 0 to 1: kindness, aggression, lethargy, friendliness,
	/// communicativeness, and a sixth trait that no desire reads
	std::array<float, 6> innateTraits;
	/// How much things on the land, mountains and water catch its eye as it looks about
	float lookAtObjectsWeight;
	float lookAtMountainsWeight;
	float lookAtWaterWeight;
	/// How long it sleeps for its size, once rested
	float sleepLength;
	/// A meal's food value over this, and over its size, is the energy it gains
	float foodToEnergy;
	float field0x36c;
	/// The poo a meal builds up for each unit of energy it gains
	float pooPerEnergy;
	/// Multiplies the alignment change that the plans of the creature make on each game turn
	float alignmentChangeScale;
	/// How a creature's size turns into how much it impresses villagers: added to its size, which is then held between 1
	/// and 4, and multiplied by this
	float impressiveSizeAddition;
	float impressiveSizeMultiplier;
	/// How much its size counts against its strength in turning chants into energy
	float spellSizeFactor;
};
static_assert(sizeof(GCreatureInfo) == 0x384);
static_assert(offsetof(GCreatureInfo, chantsPerEnergy) == 0x284);
static_assert(offsetof(GCreatureInfo, minDesireIncreaseSeconds) == 0x26c);
static_assert(offsetof(GCreatureInfo, maxDesireIncreaseSeconds) == 0x270);
static_assert(offsetof(GCreatureInfo, seenPlayerActionSpeedUp) == 0x280);
static_assert(offsetof(GCreatureInfo, desireFloor) == 0x264);
static_assert(offsetof(GCreatureInfo, spellSizeFactor) == 0x380);
static_assert(offsetof(GCreatureInfo, totemIcon) == 0x334);
static_assert(offsetof(GCreatureInfo, helpBodyProblemSeconds) == 0x2fc);
static_assert(offsetof(GCreatureInfo, inspectCreatureSeconds) == 0x2cc);
static_assert(offsetof(GCreatureInfo, lookSweepHalfAngle) == 0x2dc);
static_assert(offsetof(GCreatureInfo, lookAtPlaceRange) == 0x2f8);
static_assert(offsetof(GCreatureInfo, lookAtObjectsWeight) == 0x358);
static_assert(offsetof(GCreatureInfo, innateTraits) == 0x340);
static_assert(offsetof(GCreatureInfo, helpVeryThirsty) == 0x324);
static_assert(offsetof(GCreatureInfo, startEnergy) == 0x1f4);
static_assert(offsetof(GCreatureInfo, comfortTemperature) == 0x1fc);
static_assert(offsetof(GCreatureInfo, growUpMinutes) == 0x204);
static_assert(offsetof(GCreatureInfo, energyDrain) == 0x220);
static_assert(offsetof(GCreatureInfo, sleepRecover) == 0x230);
static_assert(offsetof(GCreatureInfo, sleepLength) == 0x364);
static_assert(offsetof(GCreatureInfo, pooPerEnergy) == 0x370);
static_assert(offsetof(GCreatureInfo, slowSpeed) == 0x240);
static_assert(offsetof(GCreatureInfo, runAwayDistance) == 0x2d8);
static_assert(offsetof(GCreatureInfo, alignmentChangePerTurn) == 0x338);
static_assert(offsetof(GCreatureInfo, strength) == 0x11c);
static_assert(offsetof(GCreatureInfo, defenceMultiplierBurn) == 0x80);
static_assert(offsetof(GCreatureInfo, secondsPerAgeTick) == 0x200);
static_assert(offsetof(GCreatureInfo, headTurnRate) == 0x24c);
static_assert(offsetof(GCreatureInfo, field0x36c) == 0x36c);
static_assert(offsetof(GCreatureInfo, alignmentChangeScale) == 0x374);
static_assert(offsetof(GCreatureInfo, impressiveSizeAddition) == 0x378);
static_assert(offsetof(GCreatureInfo, impressiveSizeMultiplier) == 0x37c);

struct GMagicRadiusSpellInfo: GMagicInfo
{
	float minRadius;
	float maxRadius;
	float radiusForNormalCost;
};

struct GMagicStormAndTornadoInfo: GMagicRadiusSpellInfo
{
	float maxWindSpeed;
	float rainAmount;
};

/// The texts of one kind of the advisors' remarks, ended by a 0
struct GHelpSpritesGuidance
{
	std::array<uint32_t, 34> texts;
};
static_assert(sizeof(GHelpSpritesGuidance) == 0x88);

struct GFeatureInfo: GMultiMapFixedInfo
{
	MeshId meshId;

	static FeatureInfo Find(const std::string& name);
};

struct GBigForestInfo: GFeatureInfo
{
	CarriedTreeType carriedType;
};

struct GForestInfo: GContainerInfo
{
	uint32_t defaultNoTrees;
	HelpText helpStartEnum;
	SoundCollisionType collideSound;
	ImmersionEffectType immersion;
	HelpText helpEndEnum;
	HelpSystemMessageSet helpMessage;
	HelpSystemCondition helpCondition;
};

struct GAnimatedStaticInfo: GFeatureInfo
{
	uint32_t dummy;
	AnimId defaultAnim;

	static AnimatedStaticInfo Find(const std::string& name);
};

struct GWorshipSiteUpgradeInfo: GFeatureInfo
{
	float scale;
};

struct GWallSectionInfo: GFeatureInfo
{
	float nearEnoughToImprove;
	uint32_t gameTurnsBeforeImprove;
	float heightIncreasePerImprove;
	float distanceToConsiderSeperateWall;
};

struct GArrowInfo: GMobileObjectInfo
{
	uint32_t field0x104;
	uint32_t field0x108;
	uint32_t field0x10c;
	uint32_t field0x110;
	uint32_t field0x114;
	uint32_t field0x118;
	uint32_t field0x11c;
};

struct GFieldInfo: GMultiMapFixedInfo
{
	glm::vec2 size;
	uint32_t maxNoFarmersPerField;
	glm::uvec2 gateNumberMissing;
	MobileStaticInfo fence;
	uint32_t deleteIfSame;
	MobileObjectInfo cropPlanted;
	uint32_t maxNumFarmers;
	EffectInfo nurtureEffect;
	float rainEffectMultiplier;
};

struct GBeliefInfo
{
	float townDesireBeliefThresholds;
	float claimedTownBeliefMultiplier;
	float lostATownBeliefInPlayerMultiplier;
	float minimumThreshold;
	float defaultBoredomOfMe;
	float beliefLeftWhenHelpSpritesWarn;
};

struct GShowNeedsInfo: GObjectInfo
{
	MeshId mesh;
	AnimId animId;
	float maxNeedValue;
	float showNeedGreater;
	float maxHieght;
};

struct GTerrainMaterialInfo
{
	SoundSurfaceType surfaceSound;
	int immersion;
	float surfaceFriction;
	std::array<char, 0x30> debugString;
	HelpText helpStartEnum;
	HelpText helpEndEnum;
	glm::uvec3 tornadoDustColorRGB;
	std::array<TreeInfo, 4> magicTreeTypes;
};

struct GFlowersInfo: GFeatureInfo
{
	float growthSpeed;
};

struct HelpSystemInfo
{
	float maxDistanceForFOVobject;
	float wideScreenTime;
	uint32_t readDefaultAdjustGTTime;
	uint32_t readDefaultWordGTTime;
};

struct GMagicForestInfo: GMagicInfo
{
	uint32_t finalNoTrees;
	float startLife;
	float growSpeed;
	float decaySpeed;
	float woodValueMultiplier;
};

struct GVillagerStateTableInfo
{
	/// The animation a villager plays in the state: -4 hides it, other negatives keep the clip it has
	int32_t animation;
	/// The town desire that a villager in the state works for, -1 for none, and how much it counts
	int townDesire;
	float townDesireWeight;
	uint32_t isFinalState;
	/// A villager storing its state to come back to stores the one it stored before instead of this one
	int keepsPreviousState;
	/// A moving state: its clip advances with the ground covered, and its life drain is its own rather than its final
	/// state's
	uint32_t movesWithGround;
	uint32_t isScriptState;
	uint32_t isScriptInterruptableState;
	/// The state a villager goes back to once it stops reacting, if it was in this one
	int resumeState;
	/// Which of the villager's speeds it moves at in this state: 1 for fleeing
	uint32_t speedIndex;
	std::array<char, 0x80> name;
	/// Bit 0 set: a villager in the state is free for other work, and for a partner
	int availableFlags;
	uint32_t field0xac;
	uint32_t field0xb0;
	/// When a villager in the state is called to make dinner, it stays in the state if this is not 0, else it goes
	/// home
	uint32_t staysForDinnerCall;
	/// A state of reacting to something
	int isReactionState;
	uint32_t field0xbc;
	/// A villager leaving a home state for this one stays inside
	int staysAtHomeOnExit;
	/// A villager going into the state may pause for a second first
	int canPauseForASecond;
	/// How much a villager whose state this is cares for food it finds lying about
	float foodInterest;
	/// How much a villager whose state this is cares for wood it finds lying about
	float woodInterest;
	/// A hungry villager in the state goes to find food
	uint32_t hungerInterrupts;
	/// A starving villager in the state goes to find food
	uint32_t starvingInterrupts;
	/// A wounded villager in the state stays rather than going home
	uint32_t woundedStays;
	/// What a villager in the state carries whatever else it holds, 0 for no change
	int carriedObject;
	/// The skill (CreatureActionKnownAbout) that a creature can learn by looking at a villager in the state; None for
	/// none. A curious creature finds a villager that does a skill the creature does not know interesting.
	uint32_t teachesSkill;
	/// A villager in the state has its needs looked at: hunger, old age, wounds and growing up
	uint32_t doesChecks;
	/// A wounded villager in the state goes home
	uint32_t woundedGoesHome;
	/// A villager whose state this is may start reacting to things
	uint32_t availableForReaction;
	/// Going into the state cuts short the clip of leaving the state before
	uint32_t skipsOutOfClip;
	/// A villager that lands after a throw returns to the state it had before, as one that a script controls does
	uint32_t resumesAfterLanding;
	/// The life a villager in the state loses each turn
	float lifeDrain;
	/// The help text telling what a villager in the state is doing, as its details show it
	uint32_t helpText;
	/// The text the Help key gives about a villager in the state, 0 for none
	uint32_t queryText;
};
static_assert(offsetof(GVillagerStateTableInfo, townDesire) == 0x4);
static_assert(offsetof(GVillagerStateTableInfo, townDesireWeight) == 0x8);
static_assert(offsetof(GVillagerStateTableInfo, availableFlags) == 0xa8);
static_assert(offsetof(GVillagerStateTableInfo, staysForDinnerCall) == 0xb4);
static_assert(offsetof(GVillagerStateTableInfo, resumesAfterLanding) == 0xf4);
static_assert(sizeof(GVillagerStateTableInfo) == 0x104);

struct GVillagerInfo: GLivingInfo
{
	Tribe tribeType;
	SexType sex;
	VillagerNumber villagerNumber;
	MeshId superMesh;
	MeshId childMeshHigh;
	MeshId childMeshMedium;
	MeshId childMeshLow;
	MeshId highDetail;
	MeshId stdDetail;
	MeshId lowDetail;
	JobInfo primaryJob;
	JobInfo secondaryJob;
	float jobEfficency;
	uint32_t startHavingSexAge;
	uint32_t stopHavingSexAge;
	int wantSexSpring;
	int wantSexSummer;
	int wantSexAutumn; // always -1 ?
	int wantSexWinter;
	uint32_t timePregnantFor;
	uint32_t lookForJobDelay;
	uint32_t waitAtHome;
	uint32_t restAtHomeTime;
	float restAtHomeRestoresLifeBy;
	float pregnancyChance;
	uint32_t inheritence;
	uint32_t boyGirlChance;
	float aveageHeightOfAdult;
	uint32_t maxFoodCarried;
	uint32_t maxWoodCarried;
	uint32_t minWoodToShowGraphic;
	uint32_t minFoodToShowGraphic;
	uint32_t maxTraderFoodCarried;
	uint32_t maxTraderWoodCarried;
	float woodUsedPerBuildCycle;
	float repairAmount;
	uint32_t jobSpeed;
	float percentLazy;
	float chantLifeRate;
	uint32_t dyingTimeWithoutGraveyard;
	uint32_t dyingTimeWithGraveyard;
	uint32_t timeForFootballChance;
	uint32_t footballWinGoalTime;
	uint32_t footballLostGoalTime;
	float lookAheadDistance;
	uint32_t scaredStiffTime;
	uint32_t waitForDinner;
	uint32_t housewifeGossipTime;
	uint32_t housewifeWaits;
	float foodNurishmentMultiplier;
	float gameTurnReducesFoodInBellyBy;
	float hungryForFood;
	float starvingForFood;
	float happyWithFood;
	float fullWithFood;
	float hungerToLifeMultiplier;
	float starvingToLifeMultiplier;
	uint32_t foodReqiredForDinner;
	uint32_t processChecksEvery;
	uint32_t havingSexTime;
	uint32_t dancingSpeed;
	AgeToScale ageToScale;
	std::array<AnimId, 3> lAttractAnim;
	std::array<AnimId, 3> lNeedAnim;
	float maxDistCarryFoodPit;
	float maxDistCarryWoodPit;
	uint32_t allowFaceSubstitution;
	float damageThresholdToGoHome;
	float damageThresholdToSleepUntil;
	uint32_t numGameTurnsAfterAggressionInterestedInShield;
	float pauseForASecondChance;
	float baseForTownNeedsSpeedMod;
	float divisorForTownNeedsSpeedMod;
	float speedModWhenFullLoadOfWood;
	float speedModWhenFullLoadOfFood;
	float lifeWhenWalksWounded;
	float lifeWhenCrawlsWounded;
	uint32_t amountOfWoodPerBuilderWanted;
	uint32_t keepMeshWhenChangeTown;
	float ownDesireThreshold;
	float maxDistanceFromArtifactDanceCanBe;
	uint16_t initialChillOutTime;
	uint16_t subsequentChillOutTime;
	uint16_t timeToWaitForArtifactDance;
	uint16_t drowningTime;
	float foodPowerupIncrease;
	float missionaryImpressiveValue;

	static VillagerInfo Find(Tribe tribe, VillagerNumber villagerNumber);
	/// The first row of the table with the tribe and the kind of work, or none
	[[nodiscard]] static std::optional<VillagerInfo> FindIn(std::span<const GVillagerInfo> rows, Tribe tribe,
	                                                        VillagerNumber villagerNumber);
};

struct CreatureDesireActionEntry
{
	/// The creature actions that can satisfy the desire, in the order the planner reads them. The list ends at the
	/// first empty slot.
	std::array<uint32_t, 30> actions;
};
static_assert(offsetof(CreatureDesireActionEntry, actions) == 0x0);
static_assert(sizeof(CreatureDesireActionEntry) == 0x78);

struct GMagicTeleportInfo: GMagicInfo
{
	float costPerKilometer;
};

struct GPlaytimeInfo
{
	ObjectType associatedObject;
	float priority;
	AbodeNumber associatedStructure;
	AbodeNumber associatedAbodeNumber;
	DanceInfo associatedDance;
};

struct CreatureMagicActionKnownAboutEntry: CreatureActionKnownAboutEntry
{
	/// The creature action that the miracle lets the creature take
	uint32_t action;
	/// The share of the energy that the miracle costs a creature that maintains it
	float energyShare;
};
static_assert(offsetof(CreatureMagicActionKnownAboutEntry, action) == 0x58);
static_assert(offsetof(CreatureMagicActionKnownAboutEntry, energyShare) == 0x5c);
static_assert(sizeof(CreatureMagicActionKnownAboutEntry) == 0x60);

struct CreatureSourceBoundsInfo
{
	float minThreshold;
	float maxThreshold;
	float thresholdStep;
};

struct GPlayerInfo
{
	float maxAlignmentChangePerGameTurn;
	float maxScriptAlignmentChange;
	float treePullPutAlignmentChange;
	float applyEffectAlignmentChangeAddition;
	std::array<float, static_cast<size_t>(DeathReason::_COUNT)> dealthReason;
	float computerPlayerBeliefChangeDecay;
	float averageTownPopulationForCredits;
};

struct ReactionInfo
{
	uint32_t priority;
	uint32_t numGameTurnsForNormalThingsToReact;
	uint32_t numGameTurnsForNormalThingsBeforeReactingAgain;
	uint32_t numGameTurnsForCreatureToReact;
	uint32_t numGameTurnsForCreatureBeforeReactingAgain;
	uint32_t whetherItImpresses;
	uint32_t whetherReactionFinishesIfInitiatorInHand;
	float maxReactionDistance;
	float howImportantIsDistance;
	uint32_t whetherReactionGrows;
	float reactionGrowthPerGameTurn;
	uint32_t stealthRandomChance;
	float minDistanceToRunAwayFromObject;
	float maxDistanceToRunAwayFromObject;
	float defaultReactionImpressiveMultiplier;
	float additionToTownBoredomMultipliers;
	TownDesireInfo correspondingTownDesire;
	TownDesireInfo correspondingTownDesireForAlignment;
	float alignmentModifier;
	GuidanceAlignment alignmentForSFX;
};

struct GMagicHealInfo: GMagicInfo
{
	float dummyVar;
	uint32_t maxToHeal;
};

struct GMagicFlockGroundInfo: GMagicInfo
{
	uint32_t numberToCreate;
	float alignmentSwitch;
	float distanceToTravel;
	float huntingRadius;
};

struct GAnimalStateTableInfo
{
	uint32_t field0x0;
	int field0x4;
	float field0x8;
	/// A state an animal is in for itself rather than on its way into another, as moving somewhere is
	uint32_t isFinalState;
	uint32_t field0x10;
	uint32_t isScriptState;
	uint32_t isScriptInterruptableState;
	/// The state that an animal goes into when a reaction ends, if the animal was in this state before the reaction
	uint32_t resumeState;
	uint32_t field0x20;
	std::array<char, 0x80> name;
	/// An animal in the state checks its needs
	uint32_t seesToNeeds;
};
static_assert(offsetof(GAnimalStateTableInfo, isScriptState) == 0x14);
static_assert(offsetof(GAnimalStateTableInfo, isScriptInterruptableState) == 0x18);
static_assert(offsetof(GAnimalStateTableInfo, seesToNeeds) == 0xa4);
static_assert(sizeof(GAnimalStateTableInfo) == 0xa8);

struct GLeashSelectorInfo: GObjectInfo
{
	uint32_t dummy;
};

struct CreatureInitialSourceInfo
{
	/// For each species, by its row in the creature tables, the starting value of the source (the first table) or
	/// its starting threshold (the second table)
	std::array<float, 17> bySpecies;
};
static_assert(offsetof(CreatureInitialSourceInfo, bySpecies) == 0x0);
static_assert(sizeof(CreatureInitialSourceInfo) == 0x44);

struct GMagicWaterInfo: GMagicInfo
{
};

struct GMagicFoodInfo: GMagicResourceInfo
{
};

struct GMagicWoodInfo: GMagicResourceInfo
{
};

struct GMobileStaticInfo: GFeatureInfo
{
	float maxScale;
	MobileStaticInfo mobileType;
};

struct GHelpSystemTooltipsInfo
{
	float priority;
	float displayTime;
	float displayTimeAfterFocus;
};

struct GWeatherInfo
{
	std::array<char, 0x30> debugString;
	float fadeInOutTime;
	float lastsFor;
	float strength;
	int temperature;
	int wetness;
	int snowFall;
	uint32_t overCast;
	glm::ivec2 wind;
};

struct GPrayerIconInfo: GSingleMapFixedInfo
{
	uint32_t dummy;
};

struct GInfluenceInfo
{
	float percentageFullInfluence;
	float percentageDistanceForDecreasingGradient;
	float valueOfSmall;
};

struct GTreeInfo: GSingleMapFixedInfo
{
	MeshId growing;
	MeshId burning;
	uint32_t strength;
	float defence;
	float startLife;
	uint32_t growsAfterNumGameTurns;
	float growthAmount;
	uint32_t burnCheckAroundEvery;
	uint32_t randomBurnChance;
	float minSize;
	float maxSize;
	float rainingAcceleratorMultiplier;
	float waterSpellAcceleratorMultiplier;
	CarriedTreeType carriedType;
	uint16_t maxNumTreesCanProduce;
};

/// A species' balance row. When the tables load, it overrides the matching fields of the species' creature row, so
/// these are the values the game plays with (the creature row's own copies are only defaults).
struct DifferentCreatureInfo
{
	float startScale;
	float growUpMinutes;
	float energyDrain;
	float startFatness;
	float fatBurn;
	float overeatFatFactor;
	float strength;
	float strengthDecay;
	float carryStrengthMinutes;
	float slowSpeed;
	float walkSpeed;
	float runSpeed;
	float headTurnRate;
	float exhaustionRate;
	float sleepRecover;
	float defenceMultiplierBurn;
	float defenceMultiplierCrush;
	float defenceMultiplierHit;
	float sleepHeal;
	uint32_t secondsPerAgeTick;
	float pooPerEnergy;
	/// Overrides the creature row's field0x36c
	float field0x54;
	/// Overrides the alignment change scale of the creature row
	float alignmentChangeScale;
};
static_assert(sizeof(DifferentCreatureInfo) == 0x5c);

/// Copies a species' balance row over the fields of its creature row that it overrides
void ApplyCreatureBalance(GCreatureInfo& creature, const DifferentCreatureInfo& balance);

struct GEffectInfo
{
	float effectBurn;
	float effectCrush;
	float effectHit;
	float effectHeal;
	float effectFlyAway;
	float effectAlignmentModification;
	float effectBeliefModification;
	float radius;
	MeshId testMesh;
};

struct GMagicEffectInfo: GEffectInfo
{
	std::array<char, 0x30> debugString;
	// seconds, read as floats; -1 = no time limit. MAGIC_TYPE 0 holds the int 10.
	float timerWhenOneShot;
	float timerWhenPlayerCasting;
	float timerWhenCreatureCasting;
	float timerWhenComputerPlayerCasting;
	float initialChants;
	float costToCreate;
	float costPerEvent;
	float costPerGameTurn;
	float costPerShieldCollide;
	uint32_t divideCostsByTribalPower;
	uint32_t createReactionOnCast;
	uint32_t createReactionOnEvent;
	Reaction reactionType;
	std::array<CreatureDesires, 2> perceivedPlayerDesire;
	TownDesireInfo townDesireBeingHelped;
	float agressiveRangeMin;
	float agressiveRangeMax;
	float calculatedCostForOneTurn;
	float costForOneAppliedEffect;
	float costInGameTurnsToCreateForOneVillager;
	float tempNoVillagers;
	float costForTempVillagers;
	float costInRealTimeToCreate;
	float impressiveValue;
	float cpImpressiveBalance;
	std::array<uint32_t, static_cast<int>(Tribe::_COUNT)> useTribalPowerMultiplier;
	uint32_t isAggressiveSpellWhichIsUsedInCreatureFightArena;
	uint32_t isDefensiveSpellWhichIsUsedInCreatureFightArena;
	HelpText helpStartEnum;
	HelpText helpEndEnum;
	HelpText creatureNearlyLearntEnum;
	HelpText creatureLearntEnum;
	HelpSystemMessageSet helpMessage;
	HelpSystemCondition helpCondition;
	HelpText toolTipsEnum;
	float aggressiveAttackValue;
	float computerCastDuration;
};
static_assert(offsetof(GMagicEffectInfo, debugString) == 0x24);
static_assert(offsetof(GMagicEffectInfo, initialChants) == 0x64);
static_assert(offsetof(GMagicEffectInfo, costPerEvent) == 0x6c);
static_assert(offsetof(GMagicEffectInfo, costPerGameTurn) == 0x70);
static_assert(offsetof(GMagicEffectInfo, divideCostsByTribalPower) == 0x78);
static_assert(offsetof(GMagicEffectInfo, agressiveRangeMin) == 0x94);
static_assert(offsetof(GMagicEffectInfo, useTribalPowerMultiplier) == 0xbc);

struct GTownDesireInfo
{
	PrayerIconInfo associatedPrayerSite;
	float showsAfterPercent;
	float desireTriggersVillagerAction;
	float desireTriggersVillagerEmergencyAction;
	std::array<VillagerBasicInfo, 3> associatedVillagerBirth;
	MeshId worshipSiteMesh;
	int worshipSiteSlot;
	float worshipSiteScale;
	float desireToBeliefScale;
	float desireAffectsBeliefAfter;
	float desireBuildWonderReducer;
	float desireValueGreaterCausesDecayInBelief;
	float desireToBeliefThresholdDecay;
	float desireAffectsAlignmentAfter;
	float howImportantDesireIsToAlignment;
	uint32_t maxTimeForAlignmentChange;
	std::array<float, static_cast<int>(Tribe::_COUNT)> tribeMultiplier;
	HelpText helpStartEnum;
	HelpText helpEndEnum;
	HelpText helpStatEnum;
	HelpSystemMessageSet helpMessage;
	HelpSystemCondition helpCondition;
};

struct GMagicFlockFlyingInfo: GMagicInfo
{
	uint32_t numberToCreate;
	float alignmentSwitch;
	float distanceToTravel;
};

struct GSpotVisualInfo
{
	std::array<char, 0x30> debugString;
	ParticleType particleType;
	uint32_t life;
	Reaction reactionType;
	uint32_t singleZSort;
	uint32_t targetOwnerObject;
};

struct GPFootballInfo: GMultiMapFixedInfo
{
	MeshId meshId;
};

struct GSpellSeedInfo: GObjectInfo
{
	// Named by how the game uses them
	GestureType selectionGesture; ///< 0xF0: stage 0 of the selection, 1 SPIRAL (player) / 2 INVERSE_SPIRAL (creature)
	GestureType gesture;          ///< 0xF4: the miracle's gesture (stage 1)
	GestureType gestureStage2;    ///< 0xF8: 0 in every entry
	GestureType sizingGesture;    ///< 0xFC: 4 CIRCLE for STORM, SHIELD, PHYSICAL_SHIELD
	SpellCastType castType;       ///< 0x100: whether the spell is cast in the hand
	uint32_t isKeptInHand;        ///< 0x104: whether the spell stays in the hand once cast
	uint32_t castOnObject;        ///< 0x108: 1 = cast on an object
	uint32_t unknown0x10C;        ///< 0 for NATURE and TELEPORT, else 1 (unused by the game)
	uint32_t seedFollowsSpell;    ///< 0x110: the seed is bound to the live spell
	/// 0x114: the magic of no power-up (-1), then of power-ups 0, 1 and 2
	std::array<MagicType, 4> magicTypes;
	/// 0x124: the gesture that powers the seed up to power-ups 0, 1 and 2 (0 = none)
	std::array<GestureType, 3> powerUpGestures;
	MeshId mesh;                       ///< 0x130
	float scale;                       ///< 0x134: the seed's scale
	float unknown0x138;                ///< equals scale except for the flocks (unused by the game)
	float holdLoweringMultiplier;      ///< 0x13C
	float holdRadius;                  ///< 0x140: times the scale
	float holdYRotate;                 ///< 0x144
	HoldType holdType;                 ///< 0x148: MAGIC (2) is forced until the seed is ready
	uint32_t attachInHandEffectToBone; ///< 0x14C: attaches the in-hand particle effect to a bone
	/// 0x150: how far above (below, mostly) the middle of a globe or icon the seed's model is, times its scale
	float meshHeight;
	/// 0x154: how far above the middle the holder particle effect plays, times its scale
	float holderHeight;
	uint32_t deleteSeedOnceCast; ///< 0x158
	float unknown0x15C;          ///< 0.1 in every entry (unused by the game)
	uint32_t unknown0x160;       ///< 1 in every entry (unused by the game)
	ParticleType holderParticle; ///< 0x164: the effect on the worship icon's holder
	uint32_t useMesh;            ///< 0x168: the seed's model is drawn in a globe or above an icon
	uint32_t exists;             ///< 0x16C: 1 = a real spell, 0 = unused slot
	uint32_t iconIndex;          ///< 0x170: icon slot (inferred)
	HelpText tooltip;            ///< 0x174
	uint32_t unknown0x178;       ///< (unused by the game)
	uint32_t unknown0x17C;       ///< 1 for FIRE, LIGHTNING_BOLT, HEAL, WEAK, STRONG: the miracles cast in fights by gesture
};
static_assert(offsetof(GSpellSeedInfo, selectionGesture) == 0xf0);
static_assert(offsetof(GSpellSeedInfo, magicTypes) == 0x114);
static_assert(offsetof(GSpellSeedInfo, powerUpGestures) == 0x124);
static_assert(offsetof(GSpellSeedInfo, exists) == 0x16c);
static_assert(offsetof(GSpellSeedInfo, iconIndex) == 0x170);

struct GCitadelHeartInfo: GCitadelPartInfo
{
	int startGoodness;
	uint32_t startFollowers;
	uint32_t maxFlockCount;
	std::array<float, 5> storyInfluence;
	float transferedDamageMultiplier;
};

struct GMagicCreatureSpellInfo: GMagicInfo
{
	CreatureReceiveSpellType creatureReceiveSpellType;
	std::array<char, 0x30> text;
	float startTransitionDuration;
	float finishTransitionDuration;
	float totalDuration;
	float maxDirnChangeWhenCtrCasting;
};

struct GRewardProgress
{
	MagicType magicType;
	std::array<uint32_t, 6> onLand;
};

struct GRewardProgressGood: GRewardProgress
{
};

struct GRewardProgressEvil: GRewardProgress
{
};

struct GFurnitureInfo: GObjectInfo
{
	MeshId mesh;
};

struct GSpookyVoiceInfo
{
	uint32_t soundNumber;
	std::array<char, 30> name;
};

struct GFieldTypeInfo: GMultiMapFixedInfo
{
	float ageGrowth;
	float ageRecolt;
	float timesToSow;
	float foodValueTakenWithHand;
	float totalFoodInField;
	uint32_t maxFarmerInFarm;
	float effectSunWhenGrowing;
	float effectSunWhenRipening;
	float effectRainWhenGrowing;
	float effectRainWhenRipening;
	MobileStaticInfo fence;
	float ratioBeforeRipe;
	float effectOfWaterSpell;
};

struct GSpellSystemInfo
{
	GestureType selectionSystemGestureR;
	GestureType selectionSystemGestureNonCreature;
	GestureType selectionSystemGestureCreature;
	float selectionSystemTimeOut;
	float selectionSystemRepeatTimeOut;
	float delayBeforeSeedActive;
	GestureType creatureSpecialMoveGesture;
	float leashSelectionSystemTimeOut;
	GestureType leashSelectionStart;
	std::array<GestureType, 4> leashSelectionGestures;
	GestureType creatureZoomTo;
	GestureType creatureEndGive;
};

struct GFishFarmInfo: GMultiMapFixedInfo
{
	uint32_t maxNoFishermanPerFishFarm;
	uint32_t numGameTurnsAfterWhichFoodIsIncreased;
};

struct CreatureDesireDependency
{
	/// For each other desire, how feedback on this desire changes how fast that one grows: above 0 the same way,
	/// below 0 the other way, 0 not at all
	std::array<float, 40> dependencies;
};
static_assert(offsetof(CreatureDesireDependency, dependencies) == 0x0);
static_assert(sizeof(CreatureDesireDependency) == 0xa0);

struct GPFootballPositionInfo: GFootballPositionInfo
{
};

struct CreatureMimicInfo
{
	std::array<char, 0x80> name;
	/// How readily the creature copies it
	float chance;
	/// Only on the learning leash
	uint32_t needsLearningLeash;
	/// The action the creature notices it with and the actions it may copy it with
	uint32_t noticeAction;
	std::array<uint32_t, 6> copyActions;
	/// The desire the creature may then want, how long each stage lasts in steps, and whether it copies the desire
	uint32_t desire;
	uint32_t stageSteps;
	uint32_t copiesDesire;
};
static_assert(offsetof(CreatureMimicInfo, chance) == 0x80);
static_assert(offsetof(CreatureMimicInfo, needsLearningLeash) == 0x84);
static_assert(offsetof(CreatureMimicInfo, noticeAction) == 0x88);
static_assert(offsetof(CreatureMimicInfo, copyActions) == 0x8c);
static_assert(offsetof(CreatureMimicInfo, desire) == 0xa4);
static_assert(offsetof(CreatureMimicInfo, stageSteps) == 0xa8);
static_assert(offsetof(CreatureMimicInfo, copiesDesire) == 0xac);
static_assert(sizeof(CreatureMimicInfo) == 0xb0);

struct GPrayerSiteInfo: GFeatureInfo
{
	float radius;
};

struct HelpSpiritInfo: GLivingInfo
{
	MeshId meshId;
};

struct CreatureDesireSourceTable
{
	/// The desire that the source feeds
	uint32_t desire;
	/// The source is cleared when the desire is satisfied
	uint32_t clearedWhenSatisfied;
	/// What the source's value is multiplied by as it feeds the desire
	float multiplier;
	/// How far a new creature's starting value and threshold of the source are spread at random either way
	float valueSpread;
	float thresholdSpread;
	std::array<char, 0x40> field0x14;
	std::array<char, 0x40> field0x54;
};
static_assert(offsetof(CreatureDesireSourceTable, desire) == 0x0);
static_assert(offsetof(CreatureDesireSourceTable, clearedWhenSatisfied) == 0x4);
static_assert(offsetof(CreatureDesireSourceTable, multiplier) == 0x8);
static_assert(offsetof(CreatureDesireSourceTable, valueSpread) == 0xc);
static_assert(offsetof(CreatureDesireSourceTable, thresholdSpread) == 0x10);
static_assert(sizeof(CreatureDesireSourceTable) == 0x94);

struct GSpellIconInfo: GMultiMapFixedInfo
{
	MeshId meshId;
	float radiusFromCitadel;
	float gatheringChantAddPerGameTurn;
};

struct GMagicShieldInfo: GMagicRadiusSpellInfo
{
	float chantCostPerImpactMomentum;
	float shieldHeight;
	float raiseWithScale;
	float bobMagnitude;
};

struct GGroundInfo: GObjectInfo
{
	CountryList country;
	MeshId editorMesh;
};

namespace v100 // original release 1.0
{
struct CreatureActionInfo
{
	/// What doing the action does to the creature's body once it is done: strength gained, energy used and exhaustion
	/// added, the last two less the bigger it is
	float strengthGain;
	float energyCost;
	float exhaustionCost;
	/// How impressive the action is to villagers who see it
	uint32_t impressiveType;
	/// How much doing the action impresses villagers watching the creature
	float impressiveness;
	std::array<char, 0x20> name;
	uint32_t field0x34;
	uint32_t field0x38;
	uint32_t field0x3c;
	uint32_t field0x40;
	uint32_t field0x44;
	uint32_t field0x48;
	uint32_t field0x4c;
	uint32_t field0x50;
	uint32_t field0x54;
	uint32_t field0x58;
	uint32_t field0x5c;
	uint32_t field0x60;
	uint32_t field0x64;
	uint32_t field0x68;
	uint32_t field0x6c;
	uint32_t field0x70;
	uint32_t field0x74;
	uint32_t field0x78;
	uint32_t field0x7c;
	uint32_t field0x80;
	uint32_t field0x84;
	uint32_t field0x88;
	uint32_t field0x8c;
	uint32_t field0x90;
	/// The action acts on a thing
	uint32_t actsOnThing;
	/// The desire the action satisfies
	uint32_t desire;
	/// The two ordinary skills the creature must know to do it, 6 for none
	std::array<uint32_t, 2> skillsNeeded;
	/// The miracle the action casts, 0 for none, which the creature must know to do it
	uint32_t magicType;
	/// The action never acts on the thing that the creature holds
	uint32_t notOnWhatItHolds;
	uint32_t field0xac;
	uint32_t field0xb0;
	/// The desire that the action shows to another creature that sees it: the other creature takes it as the strongest
	/// desire of the creature that does it
	uint32_t shownDesire;
	uint32_t field0xb8;
	uint32_t field0xbc;
	/// The desire it satisfies is multiplied by this once it is done
	float desireMultiplier;
	/// The times the creature must complete the action for its full share of skill in it
	float fullSkillTimes;
	/// The desire multiplier applies however the action started
	uint32_t alwaysApplies;
	float field0xcc;
	/// How many seconds after it finishes the action can still be learnt from
	float learningWindowSeconds;
	/// The opinion of the action that a new creature starts with, from -1 to 1
	float startingOpinion;
	uint32_t field0xd8;
	/// A friend that does the action far enough away is worth following
	uint32_t worthFollowing;
	/// Stroked while holding something during the action, the creature learns to eat what it holds
	uint32_t eatWhenStroked;
	uint32_t field0xe4;
	/// How much a picture of the action is worth for the creature's web page, from 0 to 1
	float pictureWorth;
	/// When the action ends well on another creature, how much nicer the creature finds the other creature, from -1 to 1
	float creatureNicenessChange;
	/// The creature can learn from the action at all
	uint32_t learnable;
	/// Done to its end, the action gives the creature full life
	uint32_t fullRecovery;
	uint32_t field0xf8;
};
static_assert(offsetof(CreatureActionInfo, impressiveType) == 0xc);
static_assert(offsetof(CreatureActionInfo, actsOnThing) == 0x94);
static_assert(offsetof(CreatureActionInfo, notOnWhatItHolds) == 0xa8);
static_assert(offsetof(CreatureActionInfo, alwaysApplies) == 0xc8);
static_assert(offsetof(CreatureActionInfo, learningWindowSeconds) == 0xd0);
static_assert(offsetof(CreatureActionInfo, eatWhenStroked) == 0xe0);
static_assert(offsetof(CreatureActionInfo, learnable) == 0xf0);
static_assert(offsetof(CreatureActionInfo, fullRecovery) == 0xf4);
static_assert(offsetof(CreatureActionInfo, fullSkillTimes) == 0xc4);
static_assert(offsetof(CreatureActionInfo, worthFollowing) == 0xdc);
static_assert(offsetof(CreatureActionInfo, desire) == 0x98);
static_assert(offsetof(CreatureActionInfo, startingOpinion) == 0xd4);
static_assert(offsetof(CreatureActionInfo, skillsNeeded) == 0x9c);
static_assert(offsetof(CreatureActionInfo, desireMultiplier) == 0xc0);
static_assert(offsetof(CreatureActionInfo, magicType) == 0xa4);
static_assert(offsetof(CreatureActionInfo, shownDesire) == 0xb4);
static_assert(offsetof(CreatureActionInfo, pictureWorth) == 0xe8);
static_assert(offsetof(CreatureActionInfo, creatureNicenessChange) == 0xec);

struct InfoConstants
{
	std::array<GMagicGeneralInfo, 10> magicGeneral;
	std::array<GMagicHealInfo, 2> magicHeal;
	std::array<GMagicTeleportInfo, 1> magicTeleport;
	std::array<GMagicForestInfo, 1> magicForest;
	std::array<GMagicFoodInfo, 2> magicFood;
	std::array<GMagicStormAndTornadoInfo, 3> magicStormAndTornado;
	std::array<GMagicShieldInfo, 2> magicShield;
	std::array<GMagicWoodInfo, 1> magicWood;
	std::array<GMagicWaterInfo, 2> magicWater;
	std::array<GMagicFlockFlyingInfo, 1> magicFlockFlying;
	std::array<GMagicFlockGroundInfo, 1> magicFlockGround;
	std::array<GMagicCreatureSpellInfo, 0x10> magicCreatureSpell;
	std::array<GMagicEffectInfo, 42> magicEffect;
	std::array<GSpellSeedInfo, 30> spellSeed;
	std::array<GAnimalInfo, static_cast<size_t>(AnimalInfo::_COUNT)> animal;
	std::array<GCreatureInfo, 17> creature;
	std::array<DifferentCreatureInfo, 17> differentCreature;
	std::array<CreatureDesireForType, 40> creatureDesireForType;
	std::array<CreatureDevelopmentPhaseEntry, 14> creatureDevelopmentPhaseEntry;
	std::array<CreatureDevelopmentDurationEntry, 17> creatureDevelopmentDurationEntry;
	GCitadelInfo citadel;
	GCitadelHeartInfo citadelHeart;
	std::array<GCreaturePenInfo, 5> creaturePen;
	std::array<GWorshipSiteInfo, 9> worshipSite;
	std::array<GSpellIconInfo, 2> spellIcon;
	std::array<GAbodeInfo, static_cast<size_t>(AbodeInfo::_COUNT)> abode;
	std::array<GVillagerInfo, static_cast<size_t>(VillagerInfo::_COUNT)> villager;
	std::array<GSpecialVillagerInfo, 0x30> specialVillager;
	std::array<GTreeInfo, static_cast<size_t>(TreeInfo::_COUNT)> tree;
	std::array<GSingleMapFixedInfo, 4> singleMapFixed;
	std::array<GScriptHighlightInfo, 4> scriptHighlight;
	std::array<GMapShieldInfo, 2> mapShield;
	GBallInfo ball;
	GTownInfo town;
	std::array<GJobInfo, static_cast<size_t>(JobInfo::_COUNT)> job;
	std::array<GFeatureInfo, static_cast<size_t>(FeatureInfo::_COUNT)> feature;
	std::array<GFlowersInfo, 2> flowers;
	std::array<GAnimatedStaticInfo, static_cast<size_t>(AnimatedStaticInfo::_COUNT)> animatedStatic;
	std::array<GMobileObjectInfo, static_cast<size_t>(MobileObjectInfo::_COUNT)> mobileObject;
	GScaffoldInfo scaffold;
	std::array<GMobileStaticInfo, static_cast<size_t>(MobileStaticInfo::_COUNT)> mobileStatic;
	std::array<GPotInfo, static_cast<size_t>(PotInfo::_COUNT)> pot;
	GPrayerIconInfo prayerIcon;
	GPrayerSiteInfo prayerSite;
	std::array<GShowNeedsInfo, 4> showNeeds;
	std::array<GTownDesireInfo, 17> townDesire;
	GWorshipSiteUpgradeInfo worshipSiteUpgrade;
	std::array<HelpSpiritInfo, 3> helpSpirit;
	GArrowInfo arrow;
	std::array<GSpotVisualInfo, 50> spotVisual;
	std::array<GEffectInfo, 12> effect;
	std::array<GFieldInfo, 2> field;
	std::array<GFieldTypeInfo, static_cast<size_t>(FieldTypeInfo::_COUNT)> fieldType;
	GFishFarmInfo fishFarm;
	std::array<GFootballPositionInfo, 10> footballPosition;
	std::array<GPlaytimeInfo, 5> playtime;
	GPlayerInfo player;
	GSoundInfo sound;
	GBeliefInfo belief;
	std::array<GHelpSpritesGuidance, 22> helpSpritesGuidance;
	GInfluenceInfo influence;
	HelpSystemInfo helpSystem;
	std::array<GAlignmentInfo, 7> alignment;
	std::array<ReactionInfo, 41> reaction;
	std::array<CreatureActionInfo, 328> creatureAction;
	std::array<CreatureDesireActionEntry, 40> creatureDesireAction1;
	std::array<CreatureDesireActionEntry, 17> creatureDesireAction2;
	std::array<CreatureDesireActionEntry, 40> creatureDesireAction3;
	std::array<CreatureDesireDependency, 40> creatureDesireDependency;
	std::array<CreatureInitialDesireInfo, 40> creatureInitialDesire;
	std::array<CreatureDesireSourceTable, 61> desireSourceTable;
	std::array<CreatureInitialSourceInfo, 61> creatureInitialSource1;
	std::array<CreatureInitialSourceInfo, 61> creatureInitialSource2;
	std::array<CreatureSourceBoundsInfo, 61> creatureSourceBounds;
	std::array<CreatureDesireAttributeEntry, 40> creatureDesireAttributeEntry;
	std::array<CreatureActionKnownAboutEntry, 6> creatireActionKnownAboutEntry;
	std::array<CreatureMagicActionKnownAboutEntry, 42> creatureMagicActionKnownAboutEntry;
	std::array<CreatureMimicInfo, 46> creatureMimic;
	std::array<GTerrainMaterialInfo, 43> terrainMaterial;
	std::array<GTribeInfo, static_cast<size_t>(Tribe::_COUNT)> tribe;
	std::array<GSpeedThreshold, 10> speedThreshold;
	GPBallInfo pBall;
	GPFootballInfo pFootball;
	std::array<GPFootballPositionInfo, 10> pFootballPosition;
	std::array<GDanceInfo, 25> dance;
	std::array<GVillagerStateTableInfo, 0xFF> villagerStateTable;
	std::array<GAnimalStateTableInfo, 53> animalStateTable;
	std::array<GTotemStatueInfo, 9> totemStatue;
	std::array<GBigForestInfo, static_cast<size_t>(BigForestInfo::_COUNT)> bigForest;
	std::array<GFurnitureInfo, 31> furniture;
	std::array<GWeatherInfo, 7> weather;
	std::array<GClimateInfo, 7> climate;
	GSpellSystemInfo spellSystem;
	std::array<GVortexInfo, 3> vortex;
	std::array<GMagicFireBallInfo, 3> magicFireBall;
	std::array<GRewardInfo, 61> reward;
	GLeashSelectorInfo leashSelector;
	std::array<GRewardProgressGood, 30> rewardProgressGood;
	std::array<GRewardProgressEvil, 30> rewardProgressEvil;
	std::array<GSpookyVoiceInfo, 5> spookyVoice;
	std::array<GScriptOpposingCreature, 17> scriptOpposingCreature;
	std::array<GHelpSystemTooltipsInfo, 170> toolTips;
};
static_assert(std::is_trivial_v<InfoConstants>, "GInfo must be trivial to be read properly");
static_assert(sizeof(InfoConstants) == 0x8dc66);
} // namespace v100

namespace v120 // official patch 1.20
{
struct CreatureActionInfo: v100::CreatureActionInfo
{
	uint32_t field0xfc;
};

struct InfoConstants
{
	std::array<GMagicGeneralInfo, 10> magicGeneral;
	std::array<GMagicHealInfo, 2> magicHeal;
	std::array<GMagicTeleportInfo, 1> magicTeleport;
	std::array<GMagicForestInfo, 1> magicForest;
	std::array<GMagicFoodInfo, 2> magicFood;
	std::array<GMagicStormAndTornadoInfo, 3> magicStormAndTornado;
	std::array<GMagicShieldInfo, 2> magicShield;
	std::array<GMagicWoodInfo, 1> magicWood;
	std::array<GMagicWaterInfo, 2> magicWater;
	std::array<GMagicFlockFlyingInfo, 1> magicFlockFlying;
	std::array<GMagicFlockGroundInfo, 1> magicFlockGround;
	std::array<GMagicCreatureSpellInfo, 0x10> magicCreatureSpell;
	std::array<GMagicEffectInfo, 42> magicEffect;
	std::array<GSpellSeedInfo, 30> spellSeed;
	std::array<GAnimalInfo, static_cast<size_t>(AnimalInfo::_COUNT)> animal;
	std::array<GCreatureInfo, 17> creature;
	std::array<DifferentCreatureInfo, 17> differentCreature;
	std::array<CreatureDesireForType, 40> creatureDesireForType;
	std::array<CreatureDevelopmentPhaseEntry, 14> creatureDevelopmentPhaseEntry;
	std::array<CreatureDevelopmentDurationEntry, 17> creatureDevelopmentDurationEntry;
	GCitadelInfo citadel;
	GCitadelHeartInfo citadelHeart;
	std::array<GCreaturePenInfo, 5> creaturePen;
	std::array<GWorshipSiteInfo, 9> worshipSite;
	std::array<GSpellIconInfo, 2> spellIcon;
	std::array<GAbodeInfo, static_cast<size_t>(AbodeInfo::_COUNT)> abode;
	std::array<GVillagerInfo, static_cast<size_t>(VillagerInfo::_COUNT)> villager;
	std::array<GSpecialVillagerInfo, 0x30> specialVillager;
	std::array<GTreeInfo, static_cast<size_t>(TreeInfo::_COUNT)> tree;
	std::array<GSingleMapFixedInfo, 4> singleMapFixed;
	std::array<GScriptHighlightInfo, 4> scriptHighlight;
	std::array<GMapShieldInfo, 2> mapShield;
	GBallInfo ball;
	GTownInfo town;
	std::array<GJobInfo, static_cast<size_t>(JobInfo::_COUNT)> job;
	std::array<GFeatureInfo, static_cast<size_t>(FeatureInfo::_COUNT)> feature;
	std::array<GFlowersInfo, 2> flowers;
	std::array<GAnimatedStaticInfo, static_cast<size_t>(AnimatedStaticInfo::_COUNT)> animatedStatic;
	std::array<GMobileObjectInfo, static_cast<size_t>(MobileObjectInfo::_COUNT)> mobileObject;
	GScaffoldInfo scaffold;
	std::array<GMobileStaticInfo, static_cast<size_t>(MobileStaticInfo::_COUNT)> mobileStatic;
	std::array<GPotInfo, static_cast<size_t>(PotInfo::_COUNT)> pot;
	GPrayerIconInfo prayerIcon;
	GPrayerSiteInfo prayerSite;
	std::array<GShowNeedsInfo, 4> showNeeds;
	std::array<GTownDesireInfo, 17> townDesire;
	GWorshipSiteUpgradeInfo worshipSiteUpgrade;
	std::array<HelpSpiritInfo, 3> helpSpirit;
	GArrowInfo arrow;
	std::array<GSpotVisualInfo, 50> spotVisual;
	std::array<GEffectInfo, 12> effect;
	std::array<GFieldInfo, 2> field;
	std::array<GFieldTypeInfo, static_cast<size_t>(FieldTypeInfo::_COUNT)> fieldType;
	GFishFarmInfo fishFarm;
	std::array<GFootballPositionInfo, 10> footballPosition;
	std::array<GPlaytimeInfo, 5> playtime;
	GPlayerInfo player;
	GSoundInfo sound;
	GBeliefInfo belief;
	std::array<GHelpSpritesGuidance, 22> helpSpritesGuidance;
	GInfluenceInfo influence;
	HelpSystemInfo helpSystem;
	std::array<GAlignmentInfo, 7> alignment;
	std::array<ReactionInfo, 41> reaction;
	std::array<CreatureActionInfo, 328> creatureAction;
	std::array<CreatureDesireActionEntry, 40> creatureDesireAction1;
	std::array<CreatureDesireActionEntry, 17> creatureDesireAction2;
	std::array<CreatureDesireActionEntry, 40> creatureDesireAction3;
	std::array<CreatureDesireDependency, 40> creatureDesireDependency;
	std::array<CreatureInitialDesireInfo, 40> creatureInitialDesire;
	std::array<CreatureDesireSourceTable, 61> desireSourceTable;
	std::array<CreatureInitialSourceInfo, 61> creatureInitialSource1;
	std::array<CreatureInitialSourceInfo, 61> creatureInitialSource2;
	std::array<CreatureSourceBoundsInfo, 61> creatureSourceBounds;
	std::array<CreatureDesireAttributeEntry, 40> creatureDesireAttributeEntry;
	std::array<CreatureActionKnownAboutEntry, 6> creatireActionKnownAboutEntry;
	std::array<CreatureMagicActionKnownAboutEntry, 42> creatureMagicActionKnownAboutEntry;
	std::array<CreatureMimicInfo, 46> creatureMimic;
	std::array<GTerrainMaterialInfo, 43> terrainMaterial;
	std::array<GTribeInfo, static_cast<size_t>(Tribe::_COUNT)> tribe;
	std::array<GSpeedThreshold, 10> speedThreshold;
	GPBallInfo pBall;
	GPFootballInfo pFootball;
	std::array<GPFootballPositionInfo, 10> pFootballPosition;
	std::array<GDanceInfo, 25> dance;
	std::array<GVillagerStateTableInfo, 0xFF> villagerStateTable;
	std::array<GAnimalStateTableInfo, 53> animalStateTable;
	std::array<GTotemStatueInfo, 9> totemStatue;
	std::array<GBigForestInfo, static_cast<size_t>(BigForestInfo::_COUNT)> bigForest;
	std::array<GFurnitureInfo, 31> furniture;
	std::array<GWeatherInfo, 7> weather;
	std::array<GClimateInfo, 7> climate;
	GSpellSystemInfo spellSystem;
	std::array<GVortexInfo, 3> vortex;
	std::array<GMagicFireBallInfo, 3> magicFireBall;
	std::array<GRewardInfo, 61> reward;
	GLeashSelectorInfo leashSelector;
	std::array<GRewardProgressGood, 30> rewardProgressGood;
	std::array<GRewardProgressEvil, 30> rewardProgressEvil;
	std::array<GSpookyVoiceInfo, 5> spookyVoice;
	std::array<GScriptOpposingCreature, 17> scriptOpposingCreature;
	std::array<GHelpSystemTooltipsInfo, 170> toolTips;
};

static_assert(std::is_trivial_v<InfoConstants>, "GInfo must be trivial to be read properly");
static_assert(sizeof(InfoConstants) == 0x8E186);
} // namespace v120

using CreatureActionInfo = v120::CreatureActionInfo;
using InfoConstants = v120::InfoConstants;

void UpdateInfo(CreatureActionInfo& info, const v100::CreatureActionInfo& old);
void UpdateInfo(InfoConstants& info, const v100::InfoConstants& old);
/// Every species' balance row overrides its creature row, as the game does once its tables are loaded
void ApplyCreatureBalance(InfoConstants& info);

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
#pragma pack(pop)

} // namespace openblack
