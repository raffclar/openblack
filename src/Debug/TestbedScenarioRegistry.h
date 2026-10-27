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

#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/BonusCreatures.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureMarks.h"
#include "Creature/CreaturePhysiology.h"
#include "Creature/CreatureTattoo.h"
#include "ECS/CreatureAttitudeRules.h"
#include "ECS/StatisticsRules.h"
#include "Enums.h"
#include "Particles/ParticleDrawPath.h"
#include "TestbedCrowd.h"

namespace openblack::ecs::components
{
struct PlayerStatistics;
}

/// The testbed's scenarios: ready-made set ups of the flat testbed that show one facet of the creatures at a time, such
/// as a thirsty creature finding water or a lineup of bodies. Each is plain data: the land, time of day and weather,
/// where the camera looks, the creatures with their bodies, needs and desires, the objects about them, and a timeline of
/// commands. The debug window's runner applies them through the game's systems; everything here is pure, so the
/// registry and the maths of applying it are tested on their own.
namespace openblack::testbed_scenarios
{

/// Which side of the creatures a scenario shows, for finding it in the list
enum class Facet : uint8_t
{
	/// What it does with nothing to do: fidgeting, sitting, wandering off
	Idle,
	/// Its faces, gestures and the emotes that show its desires
	Expressions,
	/// What it looks at
	Senses,
	/// Its body's needs and how it sees to them: thirst, hunger, sleep, poo, sick, fainting, cold
	Needs,
	/// Growing up and the shapes its body takes
	Growth,
	/// Its skins, hair, tattoos, wounds and eyes
	Appearance,
	/// Its shadow and its reflection
	Light,
	/// Walking, running, turning and finding its way
	Movement,
	Footprints,
	Audio,
	/// Picking things up, looking them over, putting them down, throwing them, eating them, knocking them down,
	/// pointing, and what curiosity, play and anger have it do with them
	Objects,
	/// The player's hand on it: stroking, slapping, and its status panel
	Hand,
	/// Being led on the leashes, tied up and kept at home
	Leash,
	/// Fighting another creature, being knocked out and coming round
	Combat,
	/// What it learns: from strokes and slaps, from mind files, as it grows up, and by copying the player
	Mind,
	/// The particle effects of the miracles and spot visuals
	Particles,
	/// Set ups for trying the in-game editor on: picking, moving and placing things, and its cameras
	Editor,
	/// The miracles: dispensers, casting them and what they do, the creature spells among them
	Miracles,
	/// Crowds of hundreds to thousands of creatures or villagers, all fully simulated, for measuring how the game's
	/// costs grow with their number
	Benchmark,
	/// Creature Mode, the camera locked onto a creature, and the Creature Cave
	CreatureMode,
	/// Things thrown, dropped and knocked: how they fly, what they break and how they come to rest, and what a frame
	/// costs meanwhile
	Physics,
	/// The land's nature: trees and their roots, fireflies
	Nature,
	/// The land's animals: its birds and the doves or bats about the temples
	Animals,
	/// The sky: the moon and its phases
	Sky,
	/// The player's two advisors, the good one and the evil one
	Advisors,
	/// The story's set pieces, such as the game's end as the player's temple falls
	Story,
	/// The tutorial's lessons: the hand's demonstrations and the help texts that go with them
	Tutorial,
	/// The interface over the game: the words at the top of the screen while it is paused, slower or quicker
	Interface,
	/// The maps and the rules of a network game
	Multiplayer,

	_Count
};
constexpr size_t k_FacetCount = static_cast<size_t>(Facet::_Count);
[[nodiscard]] std::string_view Name(Facet facet);

/// The weather laid over the whole island, the climates breeding no storms of their own; clear is a mild day
enum class Weather : uint8_t
{
	Clear,
	Rain,
	Thunderstorm,
	Snow,
	Blizzard,
};
[[nodiscard]] std::string_view Name(Weather weather);

/// The land that a scenario starts on
enum class Land : uint8_t
{
	/// The flat testbed
	Testbed,
	/// The network map for 32 gods, with a citadel and a village in each slot. Its land and its things come from the
	/// map, so the scenario puts nothing of the testbed on it.
	GodsRingMap,
};

struct Environment
{
	Land land {Land::Testbed};
	/// The hour of the day, as scripts give it, and whether the clock then runs on or stands still
	float hour {12.0f};
	bool clockRuns {true};
	Weather weather {Weather::Clear};
	/// With a thunderstorm, the shortest and longest wait between its forked bolts, in seconds; the weather window's 2 to
	/// 8 when not given
	std::optional<glm::vec2> boltSeconds;
	/// Game turns of the creatures' bodies that pass each game turn
	float bodyTimeScale {1.0f};
	/// Whether creatures faint when exhausted, starved or out of life
	bool fainting {true};
	/// Smiley faces for footprints as on the first of April, or by the date when not given
	std::optional<bool> aprilFools;
	/// The grid of every miracle's dispenser the testbed lays out in front of the camera stays, or is cleared away
	bool dispenserGrid {true};
	/// The footpaths are drawn as lines, as the debug menu's Footpaths box draws them
	bool drawFootpaths {false};
	/// Push scrolling is on, and works in the testbed's window as in full screen
	bool pushScrolling {false};
	/// The symbol that the world shows for the first player in place of the symbol of the player's profile
	std::optional<uint32_t> profileSymbol;
	/// The player's prayer power, for watching a maintained miracle run out of it; the testbed's plenty when not given
	std::optional<float> prayer;
	/// The player's alignment, -1 evil to 1 good, as the scenario starts; neutral when not given
	std::optional<float> playerAlignment;
	/// Where the cursor, and so the hand, is put, as a share of the window from its top left, until the mouse moves
	std::optional<glm::vec2> cursor;
	/// Where the player's temple stands, from the middle of the map, x east and y north in units of the land; with it
	/// standing their influence border shows, as on a land. The testbed has none when not given.
	std::optional<glm::vec2> temple;
	/// The Creature Breeder's bonus creatures taken as unlocked while the scenario runs, as the debug option takes them;
	/// the option is put back when it stops
	std::optional<creature_swap::Unlocked> unlockedBonusCreatures;
	/// The land's number, as a land's script sets it, while the scenario runs, such as 1 for the first land of the
	/// story. The runner puts the testbed's number back when the scenario stops.
	std::optional<int32_t> landNumber;
	/// The snow lies this deep in every cell of the island as the scenario starts, 0 to 255. A testbed set-up step.
	std::optional<float> snowDepth;
	/// The players other than the first that are in the game while the scenario runs, such as a rival god for whom a
	/// script makes a creature. The runner removes them when the scenario stops.
	std::vector<PlayerNames> players;
	/// The game is a skirmish while the scenario runs: a god with no temple is out, and the last god in wins. The runner
	/// puts the testbed back when the scenario stops.
	bool skirmish {false};
};

/// Where the camera looks as the scenario starts
enum class Shot : uint8_t
{
	/// Where the testbed puts it: above and behind the middle of the map, looking north
	Testbed,
	/// Above and to the south of everything in the scenario, all of it in view
	Overview,
	/// Behind and above one creature, keeping up with it
	Follow,
	/// In front of one creature's face, looking at its head and eyes
	Head,
	/// From the framing's eye to the point it looks at, as low or high as they say
	Placed,
};
[[nodiscard]] std::string_view Name(Shot shot);

struct Framing
{
	Shot shot {Shot::Overview};
	/// The creature followed or looked at, by its place in the scenario's creatures
	size_t creature {0};
	/// Further points kept in view by the overview, from the middle of the map, such as the lake; it keeps the creatures,
	/// the objects and wherever the commands send the creatures in view by itself
	std::vector<glm::vec2> include;
	/// Further away for more than 1, closer for less
	float distance {1.0f};
	/// A placed shot's eye and the point it looks at: x east and z north from the middle of the map, y above the land
	/// under each
	glm::vec3 eye {0.0f};
	glm::vec3 look {0.0f};
};

/// A creature's body as the scenario wants it; what is not given is as the creature's species starts
struct NeedOverrides
{
	std::optional<float> energy;
	std::optional<float> exhaustion;
	std::optional<float> dehydration;
	std::optional<float> poo;
	std::optional<float> life;
	std::optional<float> warmth;
	/// Hours of game time it has lived
	std::optional<uint32_t> age;

	[[nodiscard]] bool Empty() const;
};

/// A desire set to a fraction of its maximum, and activated if the creature's stage of growing up hadn't brought it
struct DesireOverride
{
	creature_desires::Desire desire;
	float fraction {1.0f};
};

struct CreatureSetup
{
	/// What the readout calls it, such as "fat"; the species' name when empty
	std::string_view label;
	CreatureType species {CreatureType::Tiger};
	/// Where it stands, from the middle of the map, x east and y north in units of the land
	glm::vec2 offset {0.0f};
	/// Its facing as the spawner gives it: 0 faces south, towards the testbed's camera, and 180 faces north
	float facingDegrees {0.0f};
	PlayerNames owner {PlayerNames::PLAYER_ONE};
	/// Whether it is the one creature its owner can lead on the leash. When not given, a player's first creature is
	/// and the others aren't.
	std::optional<bool> leashable;
	/// Its alignment, fatness and strength, the species' start when not given, and its size, 1 when not given: a
	/// creature well grown, about 15 units tall, rather than as small as a new one starts
	std::optional<float> alignment;
	std::optional<float> fatness;
	std::optional<float> strength;
	std::optional<float> size;
	/// How far it has grown up, from 0; fully grown up when not given
	std::optional<uint32_t> phase;
	NeedOverrides needs;
	std::vector<DesireOverride> desires;
	/// The needs and desires are set again every frame, so they stay as given however time goes
	bool hold {false};
	/// The mind leaves the body alone, to be posed by the commands
	bool pauseMind {false};
	/// A paused mind starts to think this many seconds after the scenario starts, so that the creature stays where it is
	/// until then
	std::optional<float> thinksAfterSeconds;
	std::vector<creature_tattoo::Slot> tattoos;
	/// Wounds and burns, and drops of blood, on its skin
	std::vector<creature_marks::Mark> wounds;
	std::vector<creature_marks::Mark> blood;
	/// A mind file it takes up as it starts: "game:NAME" for one of the game's Scripts/CreatureMind files, or
	/// "chosen:NAME" for the one last opened with the spawner's file dialog, falling back to the game's file NAME
	std::string_view mindFile;
	/// The skills it knows as it starts, as a script teaches them
	std::vector<CreatureActionKnownAbout> skills;
};

/// Something put on the land for the creatures: an object, a tree, a feature such as a pillar of rock, a villager, a pot
/// or pile of food or wood, a building or field of the scenario's town, which is made with the first of them, or an
/// animal, or a fish farm (which joins the nearest town)
struct ObjectSetup
{
	std::variant<MobileObjectInfo, TreeInfo, FeatureInfo, VillagerInfo, PotInfo, AbodeInfo, FieldTypeInfo, AnimalInfo,
	             MobileStaticInfo, AnimatedStaticInfo, FishFarmInfo, FlowersInfo>
	    type;
	glm::vec2 offset {0.0f};
	float scale {1.0f};
	float yawDegrees {0.0f};
	/// How much a pot holds, which for food is what it is worth to eat: a meal for a grown up creature by default
	int32_t amount {800};
	/// A villager walks to this point, from the middle of the map, once the delay is up, and then back and forth between
	/// it and where it started at the repeat
	std::optional<glm::vec2> walkTo;
	float walkAfterSeconds {1.0f};
	std::optional<float> walkRepeatSeconds;
	/// What a walking villager is to do once there: deciding afresh unless given. A villager walking to decide afresh takes
	/// up no reaction on its way, as in the game
	VillagerStates walkFinal {VillagerStates::DecideWhatToDo};
	/// A villager belongs to the scenario's town of the player, rather than to none
	bool joinTown {false};
	/// A villager is made at this age, rather than as a grown up of 30
	std::optional<uint32_t> villagerAge;
	/// A villager of the town lives in the scenario's object of this index, a building of the town
	std::optional<size_t> livesIn;
	/// A villager lives in the scenario's building of this index. The building must come before the villager in the
	/// list.
	std::optional<size_t> homeIn;
	/// A villager goes to make love with the scenario's object of this index once the walk delay is up, as a breeder
	/// sends one: both walk to meet and kiss there
	std::optional<size_t> mateWith;
	/// A villager is put down on the nearest teleport stone after this many seconds, as the hand drops one, and jumps at
	/// once to the stone nearest where it was walking
	std::optional<float> dropOnStoneSeconds;
	/// A villager sets off to worship at this point once the walk delay is up, going through its player's teleport
	/// stones when the point is further than villagers walk to worship
	std::optional<glm::vec2> worshipAt;
	/// A villager's, animal's or building's life, 0 (dead) to 1, full when not given; a building is hurt down to it
	std::optional<float> life;
	/// A villager carries this much wood and food, which slows it, none when not given
	int16_t woodHeld {0};
	int16_t foodHeld {0};
	/// A tree belongs to the land's forest of this number, none for 0; and grows to this size, its scale when not given
	uint32_t forest {0};
	std::optional<float> fullSize;
	/// A villager or animal that has eaten poison; a pot whose food is poisoned
	bool poisoned {false};
	/// Made indestructible, as a script makes one
	bool indestructible {false};
	/// An animal lives as a land's script makes it: at a random age, in a herd or flock of its own, and free to move.
	/// Only the grazers and the land's birds can live. Other kinds, and all animals without this flag, stay at their
	/// position. They show the clip of an animal that a script holds still.
	bool living {false};
	/// A villager is held by a script where it is put, standing still, as a land's script holds one it makes. An animal
	/// is made as a land's script makes one, and the script controls it.
	bool inScript {false};
	/// A villager is under the control of a script, as a script controls a villager that it moves: it takes up no
	/// reaction until the script releases it
	bool scriptControlled {false};
	/// A villager is the named person of this row in the game's table of named people, whose name the Show Villager
	/// Names key shows
	std::optional<uint8_t> nameRow;
	/// A villager is made a disciple of this work, as a land's script makes one
	VillagerDisciple disciple {VillagerDisciple::None};
	/// A script has said it can't be picked up, and can't be moved, as Land 1's script says of the fisherman's wife
	/// while she waits for the player
	bool cannotBePickedUp {false};
	bool immovable {false};
	/// A building of the town holds this much food, rather than the scenario town's usual stock
	std::optional<uint32_t> storedFood;
	/// A building of the town holds this much wood, rather than the scenario town's usual stock
	std::optional<uint32_t> storedWood;
	/// A building is only planned by the scenario's town, as a land's script plans one, rather than standing built
	bool planned {false};
	/// A planned building is the plan of one that had been finished and went from the land, which its town wants
	/// rebuilt as a repair
	bool wasBuilt {false};
	/// A building stands only this much built, a site open for its town's builders to finish it
	std::optional<float> built;
	/// A building starts with this part of the time to its next loss of life from neglect already gone
	float neglect {0.0f};
	/// The crop of a field is fully sown and ripe, with all the food of its type. If false, the field is not sown.
	bool ripeCrop {false};
	/// A firefly hides exactly where the thing stands, as a land's script places one
	bool firefly {false};
	/// The thing is set alight as it is made, burning as at the start of a fire
	bool alight {false};
	/// The thing makes a reaction of this kind as it is made, as the game makes one about a thing (a thing coming to rest
	/// makes one to look at it), by the scenario's player
	std::optional<Reaction> reaction;
	/// A tree lies dead, as a land's script makes a dead tree, rather than standing and growing
	bool dead {false};
	/// The lean of a dead tree or a mobile static about x and z, in degrees, as a land's script gives it
	glm::vec2 leanDegrees {0.0f};
};

/// A particle effect played on the land
struct ParticleSetup
{
	ParticleType type {ParticleType::Smoke};
	/// An effect file played in place of the type's, by its name, for the effects no particle type names
	std::string_view file;
	particles::draw::DrawPath path {particles::draw::DrawPath::Sorted};
	/// The scenario's creatures are given to it to act on, as the heal miracle is given the people it heals
	bool targetsCreatures {false};
	glm::vec2 offset {0.0f};
	/// Above the land
	float height {0.0f};
	float magnitude {1.0f};
	/// The player whose colour it takes where it takes one
	int player {0};
	/// Seconds after which it closes down and starts again, for effects that end; none to run until the scenario stops
	float restartSeconds {0.0f};
};

/// A vortex between the lands opened as a script opens one, and later told to fade out
struct VortexSetup
{
	VortexType type {VortexType::In};
	glm::vec2 offset {0.0f};
	/// Seconds into the scenario it is made, and after that it is told to fade out; none to stay
	float delaySeconds {0.0f};
	std::optional<float> fadeOutAfterSeconds;
	/// For a vortex into the land: the crossing file's lines, written just before it is made, as a vortex out of the last
	/// land would have written them (none leaves the file as it is); and where the animals that come out gather, as one
	/// flock keeping within the distance and the radius, as a land's script tells it
	std::vector<std::string_view> crossingLines;
	std::optional<glm::vec2> gatheringOffset;
	float flockDistance {0.0f};
	float flockRadius {0.0f};
};

/// A reward chest given to the player as a challenge's script gives one: dropped from the sky or put on the land
struct RewardSetup
{
	RewardObjectInfo type {RewardObjectInfo::RewardObjectToyBall};
	glm::vec2 offset {0.0f};
	bool fromSky {true};
	/// It is given in the scenario's town, which gets what is inside, rather than in no town
	bool inTown {false};
	/// Seconds into the scenario it is given
	float delaySeconds {0.0f};
	/// A testbed set-up step: its player opens it as soon as it is given, as a tap does. Only for a chest put on the land.
	bool opened {false};
};

/// A miracle dispenser, or a one-shot bubble on its own, put down for the scenario
struct DispenserSetup
{
	MagicType type {MagicType::Fireball};
	glm::vec2 offset {0.0f};
	/// Only the bubble, floating this high above the land, without its dispenser
	std::optional<float> bubbleHeight;
	/// A testbed set-up step: the dispenser starts with its period already counted and floats its globe at once, rather
	/// than after its period as a dispenser put down in the game does
	bool charged {true};
	/// The dispenser comes as a challenge's reward comes: the script makes it, gives it its miracle and turns it on, so
	/// it floats its first globe at once. It belongs to the scenario's town, the player's. Not with charged.
	bool reward {false};
	/// With reward: the dispenser belongs to no town
	bool noTown {false};
	/// The life it is left with by a blow when it is made, 1 for none
	float life {1.0f};
	/// With reward: its first globe goes at once, as if the hand took it, so the dispenser starts to count
	bool globeTaken {false};
};

/// How a script casts a miracle at a point or a thing: by the neutral player, from the hand's place, as big as the radius,
/// lasting the seconds and spun by the curl
struct ScriptSpell
{
	float radius {0.0f};
	float seconds {0.0f};
	float curl {0.0f};
};

/// A storm a land's setup script places in the island's global climate, as CREATE_WEATHER_STORM does, with the script's
/// own lists of numbers (see script_weather::ParseLandStorm), made as the scenario starts
struct LandStormSetup
{
	/// Where it starts and where it heads, from the middle of the map, on the land
	glm::vec2 offset {0.0f};
	glm::vec2 destination {0.0f};
	/// Seconds of its life already gone, and its speed (0 stays put)
	float age {0.0f};
	float speed {0.0f};
	int32_t cloudCount {8};
	std::string_view shape;
	std::string_view clouds;
	std::string_view weather;
};

/// A miracle cast in the scenario, as if from a hand above the land
struct MiracleCast
{
	enum class Target : uint8_t
	{
		/// At the point
		Point,
		/// On one of the scenario's creatures, by its place
		Creature,
	};
	MagicType type {MagicType::Fireball};
	Target target {Target::Point};
	/// The point cast at, from the middle of the map
	glm::vec2 point {0.0f};
	size_t creature {0};
	/// Where the hand casting it is, from the middle of the map, and how high above the land
	glm::vec2 handOffset {0.0f};
	float handHeight {15.0f};
	/// The hand's movement as it lets go, which throws a fireball
	glm::vec3 throwVelocity {0.0f};
	/// Seconds after the scenario starts that it is cast
	float delaySeconds {1.0f};
	/// A miracle held in the hand (lightning, water, food, wood) runs this long, then stops; none to run its course
	std::optional<float> holdSeconds;
	/// Seconds after which it is cast again, none for once
	std::optional<float> repeatSeconds;
	/// Cast through the hand as a player does, rather than straight at its point: a seed of it is put in the hand, the
	/// action button pressed with the hand over its point (or creature), and let go after its hold, the hand moving by
	/// its throw velocity meanwhile
	bool byHand {false};
	/// By hand, the seed comes from the player's worship, charged from their prayer power and ready after a moment, rather
	/// than from a bubble
	bool fromWorship {false};
	/// By hand, a circle drawn round the point first, of this radius, for the storms and shields; none draws none
	std::optional<float> circleRadius;
	/// Cast straight at its point by this player, rather than the player's
	PlayerNames player {PlayerNames::PLAYER_ONE};
	/// By hand, the hand sweeps on at its throw velocity for this long after letting go, as a player sweeps a flock out
	std::optional<float> sweepSeconds;
	/// The caster's alignment, set as it is cast (a flying flock is bats for an evil caster, doves for a good one)
	std::optional<float> casterAlignment;
	/// By hand, the seed is held this many seconds before the button goes down, rather than just long enough to be ready
	std::optional<float> holdBeforePress;
	/// Cast as a script casts it, from the hand's place, rather than as a seed in the hand would
	std::optional<ScriptSpell> script;
	/// A shield is broken this many seconds after it is cast, as by a blow its caster can't pay for: its people react to
	/// its fall and it closes down. A testbed set-up step.
	std::optional<float> breakSeconds;
};

/// Something a creature is told to do, in turn with the scenario's other commands
/// A value a scenario hands a land's script, as another script's call hands it: a number, a marker made at a point from
/// the middle of the map so high above the land, or one of the scenario's objects or creatures, by its place
struct ScriptArgument
{
	enum class Kind : uint8_t
	{
		Number,
		Marker,
		Object,
		Creature,
		/// A position as three numbers: the point from the middle of the map, at the height above the land. Only a
		/// native call takes it.
		Position,
		/// A whole number (number, made whole towards zero), as a script gives a constant such as a resource type
		Integer,
	};
	Kind kind {Kind::Number};
	float number {0.0f};
	glm::vec2 point {0.0f};
	float height {0.0f};
	size_t index {0};
};

/// The value of a command that plays custom music that stops the music
constexpr size_t k_StopCustomMusic = std::numeric_limits<size_t>::max();

struct Command
{
	enum class Kind : uint8_t
	{
		/// Going to the point, by walking or running; following the other creature; running away from the point;
		/// turning to face the point, or the camera; stopping
		WalkTo,
		RunTo,
		Follow,
		FleeFrom,
		TurnToFace,
		FaceCamera,
		Stop,
		/// Playing the animation: an action, a gesture on top of the body, or a face
		PlayAction,
		PlayGesture,
		PullFace,
		/// Drawing a gesture in the air with the creature's hand, by the gesture's number (value)
		CreatureDrawGesture,
		/// Pulling the face its mind would for a feeling or what it does (value, by the face cues)
		ShowFeeling,
		SitDown,
		StandUp,
		/// Seeing to a need now: sleeping, waking, eating the nearest food, drinking at the nearest water, having a
		/// poo, a fart, being sick, fainting
		Sleep,
		Wake,
		Eat,
		Drink,
		Poo,
		Fart,
		Puke,
		Faint,
		/// As if the player's hand stroked or slapped it
		Stroke,
		Slap,
		/// The hour of the day jumps, for every creature
		SetHour,
		/// Walking up to the scenario's object and picking it up; then, with what it holds, putting it down, tossing it
		/// away, eating it, or looking it over with one of its four ways of doing so (stroking, shaking,
		/// smelling, examining); throwing it at the point
		PickUp,
		PutDown,
		Discard,
		EatHeld,
		Examine,
		ThrowAt,
		/// Walking up to the scenario's object and knocking it down; pointing at the point
		KnockDown,
		PointAt,
		/// The player's hand stroking a part of its body, or slapping it at a height and hard or gently, held to it
		/// until the hand lets go, which tells its mind how it was treated
		HandStroke,
		HandSlap,
		HandLetGo,
		/// Putting a leash on it, which it is first taught; tying the leash to the scenario's object, untying it to the
		/// hand, taking it off; keeping it within a radius of where it stands
		PutOnLeash,
		TieLeash,
		UntieLeash,
		TakeOffLeash,
		ConfineToHome,
		/// Making it the one creature its owner can lead, which the owner's other creature stops being; the player's
		/// hand tapping it to put the picked leash on, as the Action button does; the player pressing a leash shortcut
		/// (value: 0 the leash key L, 1 the previous leash V, 2 the next leash B), which acts on the player's creature
		MakeLeashable,
		HandTapLeash,
		LeashKey,
		/// The player shaking the leash off: a scribble drawn with the empty hand through the same gesture recogniser
		/// the cursor goes through, which takes off a leash held in the hand
		LeashShake,
		/// With the leash held, the player's Action button giving an order: to go to the point, or to act on the
		/// scenario's object
		LeashOrderAt,
		LeashOrderOn,
		/// Hanging the player's three temple leashes in a row about the point, five metres up, as a temple hangs them;
		/// the player tapping one of them (value: 0 aggression, 1 learning, 2 compassion)
		HangLeashPosts,
		TapLeashPost,
		/// The player's empty hand tapping the scenario's object, a building that the hand cannot take. The leash takes
		/// the tap as an order when it can. If the leash does not take the tap, the hand knocks on the building.
		LeashTapBuilding,
		/// Fighting the other creature; then, in the fight, a blow high, in the middle or low charged for a while, a
		/// block, a step forward, back, right or left, the special move, and fighting by itself or not, as the player's
		/// clicks and the debug tools give them
		StartFight,
		FightBlow,
		FightBlock,
		FightStep,
		FightSpecial,
		FightAuto,
		/// In the fight, a miracle (value, by its magic type) added to the end of the moves, as drawing its gesture does
		FightSpell,
		/// The player double clicks the place of the creature, which is in its arena: the camera shows the fight at once
		FightView,
		/// The pointer moves to the middle of the body of the opponent of the creature on the screen, as the mouse moves
		/// it. A press of a button there goes to the fight only while the camera shows the fight.
		PointerOnOpponent,
		/// The pointer moves onto the body of the creature in the Creature Cave on the screen, as the mouse moves it
		PointerOnCaveCreature,
		/// The fight autopilot starts to fight for the player with the real mouse, with a policy (value: 0 the lesson, 1
		/// aggressive, 2 random, 3 a sequence). It needs the inspector.
		FightAutopilot,
		/// The fight transcript of the creature shows the stages of the guide's fight lesson in their order: a blow that
		/// makes the opponent reel, more than two blows in the queue, a block and a step
		FightLessonCheck,
		/// Being knocked out, as a fight's loser is, and brought round again
		KnockOut,
		BringRound,
		/// The debug tools stop the fight of the creature with no knock-out: each fighter finishes, then is happy or sad
		FightAbort,
		/// Tying the leash it wears to the other creature, which has it fight that creature
		TieLeashToCreature,
		/// A desire (by value) is set to a fraction (amount) of its maximum; the creature grows up to a stage (by value)
		SetDesire,
		SetPhase,
		/// From now on, each thing it does to something is judged once done, by the mind's trainer: stroked if the
		/// thing is of a kind (value, the game's belief type, such as 6 for a villager), else slapped
		RewardIf,
		/// It watches a skill (value, by its row of the game's table) or a miracle near it; the player does one of the
		/// deeds creatures copy (value) at the point
		SeeSkill,
		SeeMiracle,
		PlayerDid,
		/// The player does one of the deeds creatures copy (value) to the scenario's object
		PlayerDidOn,
		/// C pressed: Creature Mode locks onto the player's creature, or lets go of it; a double click on the creature;
		/// the cursor keys (value: left, right, up or down) held with Shift, or with Ctrl, for some seconds (amount);
		/// Ctrl and Shift pressed together for a clear view; the camera given back
		CreatureKey,
		DoubleClick,
		CameraKeys,
		ClearCameraView,
		LeaveCreatureMode,
		/// F5 pressed: the Creature Cave opens, on a page (value); a symbol (value) tattooed on a place (body part), or
		/// the place's symbols taken off
		OpenCreatureCave,
		ApplyTattoo,
		RemoveTattoo,
		/// The tattoo editor opened on the player's creature, as clicking it in the Creature Cave does
		OpenTattooEditor,
		/// The player's hand is given a seed (value, by the game's seed number) as if from a bubble; a gesture (value, by
		/// the game's gesture number) is drawn with the hand across the middle of the screen, through the same recogniser
		/// the cursor goes through. Neither needs a creature.
		HoldSeed,
		DrawGesture,
		/// The player's hand is given a seed (value) summoned from their worship, charged from their prayer power, which
		/// can be powered up
		SummonSeed,
		/// The creature knows a miracle (value, by its magic type) as if it had learnt it; it is told to cast a miracle
		/// (value) at the scenario's object, or at another creature (atCreature), going about it as by itself
		KnowMiracle,
		CastMiracle,
		/// The player presses a key's action (value, the bindable action's flag) for one frame, as the options screen's
		/// presses are made. Needs no creature
		PressKey,
		/// The player's hand takes hold of a fireball in flight, the hand straight over it: an empty hand taps it (catching
		/// another player's), a hand with a seed presses the action button on it (a fire seed takes it in)
		HandTakeFireBall,
		/// The player's mouse, through the same input the real one goes through: the pointer put at a point on the
		/// screen (point, as fractions of its width and height from the top left); a button (value: 1 left, 2 middle,
		/// 3 right) pressed or let go; the mouse moved by point, as fractions of the screen, over some seconds
		/// (amount); the wheel turned some notches (value) away from the player, or towards with ctrl
		PointerTo,
		PointerPress,
		PointerRelease,
		PointerSweep,
		WheelTurn,
		/// The player's alignment jumps, which the hand shows
		SetAlignment,
		/// A script's open (value 1) or close (value 0) of the scenario's object (a gate, the gate stone plinth); the
		/// player's hand laying the scenario's object, a gate stone, in the plinth that is the scenario's object of this
		/// place (value). Neither needs a creature.
		SetOpenClose,
		LayGateStone,
		/// A script's cinema bars slide in (value 1) or out (value 0)
		WideScreen,
		/// A script sets the interface level (value), as the script command that sets the interface interaction does:
		/// the camera's features, its keys and the hand's reach. Needs no creature
		SetInterfaceLevel,
		/// Its history, as the Creature Cave's trophies show it: how it leans in fights (amount, -1 defensive to 1
		/// aggressive) as if it had fought; how many times it has seen a miracle (value, by its magic type; amount, the
		/// times), which it then knows about
		SetFightLean,
		SetMiracleSightings,
		/// The player's hand knocks on the scenario's object, a building, as the Action button pressed on it does, from
		/// where the hand is
		HandTapObject,
		/// One of the player's advisors (value: 0 the good one, 1 the evil one) is told to do something, as the scripts
		/// tell it (advisor). Needs no creature
		Advisor,
		/// The creature walks to its home: its temple's pen while its player has a temple
		WalkHome,
		/// One of the help's message sets (value) runs, as the help system runs it. Needs no creature
		RunHelpMessage,
		/// Every kind of the advisors' guidance counts as heard as long ago as can be, so the next remark due is said
		/// at once rather than after its usual wait. Needs no creature
		EndGuidanceWaits,
		/// The scenario's temple (value: its place in the scenario's temples) starts being destroyed, as when its heart
		/// loses the last of its life. Needs no creature
		DestroyTemple,
		/// A script tells the creature to play an animation (value) some times (amount, rounded), as "play 47 loop 1"
		/// does: a static action's start, loop and end, or an individual action
		ScriptPlay,
		/// A help text (value, by the scripts' number) shown in the dialogue box, as a script's RUN_TEXT shows one. Needs
		/// no creature
		RunText,
		/// A hand demonstration (handDemo, by its file's name) played through the player's hand and camera, as a
		/// tutorial's script plays one. Needs no creature
		PlayHandDemo,
		/// The camera is put back where the scenario's shot puts it, as when the scenario starts (after a miracle's camera
		/// flight, say). Needs no creature
		Reframe,
		/// The player's hand takes the scenario's object at once, as if picked up from where it stands (needs no
		/// creature); the hand gives what it holds to the creature, as the Action button pressed over it does
		HandTake,
		HandGive,
		/// A script swaps the minds of the creature and another (value, by its place in the scenario's creatures), as
		/// "swap creature from A to B" does
		SwapMinds,
		/// A challenge's script records it as SNAPSHOT does: the challenge (value), its title (title, by its help text's
		/// name), how far it has got (amount, 1 done), a quest (ctrl) or not; the camera is first put 50 above and 70
		/// along z from a point (point, from the middle of the map), looking at it, and the picture is of that view.
		/// Needs no creature
		RecordChallenge,
		/// The player is put in a room of the temple (value, by the temple's rooms: 0 the challenge room). Needs no
		/// creature. In the temple, the value of no room (TempleRoom::Unknown) walks the camera out through the main
		/// room's way out
		EnterTemple,
		/// The explorers' boat film plays as a script's PLAY_JC_SPECIAL 6 plays it on Land 1, moved to the testbed: its
		/// dry dock at the point (from the middle of the map) on the land there, and its sailing starting in the lake's
		/// north-east corner. Needs no creature
		PlayBoatFilm,
		/// A land's script (script, by its name) starts as a script's "run script" starts it, given its arguments in
		/// order (scriptArguments); the scripts' globals named are set first (scriptGlobals). Needs no creature
		RunScript,
		/// The player's hand releases the thing that it holds, as a release of the Action button does. With no velocity, the
		/// hand puts the thing on the land below it. Else, the hand throws the thing at the velocity. No creature is necessary.
		HandRelease,
		/// The player's hand lets go of a handful of wood (value, how much) slowly over a point of the land (point, from
		/// the middle of the map), pouring it there as a slow let-go does. Needs no creature
		HandPour,
		/// A script tells the creature to do an action of the game's table (action, by its name) to the scenario's object,
		/// with another of the scenario's objects (with) when given, as a script's creature action command does. With
		/// atCreature, the action is about that creature of the scenario.
		DoAction,
		/// The creature and the other creature (value) become friends, as a script's "force friends" command does
		MakeFriends,
		/// The creature wants only one desire (value) from now on, as a script's "only desire" command makes it
		OnlyDesire,
		/// The scenario's object goes from the world at once, as when something other than the creature removes it.
		/// Needs no creature
		RemoveObject,
		/// The player reads a "did you know" tip (value, by the scripts' number) of a category (amount, rounded) on a
		/// sign, as when the hand taps the sign. The library lists the tip. Needs no creature
		ReadTip,
		/// The temple's camera looks at a scroll of the room the player is in (value: its place among the room's
		/// scrolls), as when the player presses the scroll. Needs no creature
		LookAtTempleScroll,
		/// The creature catches fire, and burns as at the start of a fire. With a value of 1, the scenario's object catches
		/// fire in place of the creature.
		SetAlight,
		/// The player's hand takes hold of the totem of the scenario's object, a town centre, and then lets go. Needs no
		/// creature
		HandGripTotem,
		/// The pointer moves onto the middle of the foot of the scenario's town's worship site altar on the screen, as the
		/// mouse moves it. Needs no creature
		PointerOnAltar,
		/// The pointer moves onto the rope of the leash of the creature, tied to a thing, a quarter of the way from the
		/// thing, as the mouse moves it
		PointerOnRope,
		/// The pointer moves onto the scenario's object (object) at a height (amount) over its place on the screen, as
		/// the mouse moves it. Needs no creature
		PointerOnObject,
		/// The creature fizzes to a share (amount: 0 in sight, 1 out of sight) over some milliseconds (value), as a
		/// script's fade does. It stays at that share.
		SetFizz,
		/// The game writes the help record of the player to helpstats.dat in their folder in the profile folder, and
		/// makes the folder if it is missing. Then the game reads the file again, and the log tells if the record is the
		/// same. Needs no creature
		ProfileFolderCheck,
		/// The player has a picture of their own for a design (value), as from a Symbol file in their profile's folder:
		/// grey rings. Needs no creature
		OwnTattooPicture,
		/// Opens the game's menu on its Players page, then the tattoo editor over it, as the page's Edit Tattoo button
		/// does. Needs no creature
		OpenTattooMenu,
		/// Opens the game's menu on its Controls page, picks the row that the value gives and double clicks it, so that the
		/// box asks for the row's new key or mouse input. Needs no creature
		OpenControlsBox,
		/// Opens the game's menu on the page that the value gives (the main menu 0, Options 1, Players 2, Advanced 3,
		/// Controls 4). With ctrl, the menu acts as if the game had no sound. Needs no creature
		OpenMenuPage,
		/// Presses a key in the game's menu: the value is the SDL key code, with Shift when ctrl is set. Needs no creature
		PressMenuKey,
		/// The custom music player plays the track of the custom music folder that the value gives, from 0, and the music
		/// moods look at the music on this turn. The value k_StopCustomMusic stops the music. The game reads the folder again
		/// first. Needs no creature
		PlayCustomMusic,
		/// The custom music controls at the top right of the screen show their buttons (value 1), and also the play
		/// list (value 2), as the music button and the play list button show them. Value 0 hides them. Needs no
		/// creature
		ShowCustomMusicControls,
		/// The game saves into the slot (value), as the save game room saves into it. Needs no creature
		SaveGame,
		/// The game loads the creatures of the slot (value) into the land, as a load of the slot does. Needs no creature
		LoadGame,
		/// A script tells the creature to take notice of reactions (value 1) or to take no notice (value 0), as
		/// a script's "enable ... reaction" and "disable ... reaction" do
		ScriptReaction,
		/// A script puts the creature at the point at once (from the middle of the map), as SET_POSITION does. Its walk,
		/// body action, face and head look stop there.
		ScriptPlace,
		/// The game stops every task of a land's script (script, by its name), as a script's "stop script" does. The log
		/// tells how many tasks stopped and if the scenario's object is still in a script. Needs no creature
		StopScript,
		/// The pointer moves to the middle of the body of the creature on the screen, as the mouse moves it
		PointerOnCreature,
		/// The music of the music type that the value gives starts as a script's "start music" starts it. The value 0
		/// stops it as "stop music" does. Needs no creature
		StartScriptMusic,
		/// The game writes the web page of the creature into the player's profile folder, as it does each 600 turns of
		/// the creature's life
		WriteWebPage,
		/// A bookmark (value, 0 to 6) is put at a point on the land (point, from the middle of the map), with the camera
		/// origin 80 metres back and 50 metres up from it. Then the camera flies to it, as the bookmark's number key does.
		/// Needs no creature
		FlyToBookmark,
		/// The player presses the Tab key: the camera flies to the next town of the player. Needs no creature
		TabToTown,
		/// The player keeps the creature in a bookmark (value, 0 to 6), as Ctrl and the number key do with the hand over
		/// it. Then the camera follows the bookmark, as Shift and the number key do.
		BookmarkCreature,
		/// The falling spell's film plays as a script's SET_AVI_SEQUENCE 2 starts it, with a copy of the player's first
		/// creature over the film. Needs no creature of its own
		PlayFallingSpellFilm,
		/// The intro film plays full screen, as the game starts it at the start of a new game. Needs no creature
		PlayIntroFilm,
		/// The creature gets a glow on its left hand, its belly, its right hand and above its body, each of the power
		/// that the value gives, as the creed powers give their glows
		GiveHandGlows,
		/// A script gives the creature a creed power: the value names the place of the glow (0 the left hand, 1 the
		/// belly, 2 the right hand, 3 above the body) and the amount gives its power. The scale is 5, as each shipped
		/// script gives it.
		GiveCreedPower,
		/// A script attaches a sound to the creature, as ATTACH_SOUND_TAG does: the value gives the sound. Each start
		/// plays it one time, and the next turn starts it again when it is finished.
		AttachScriptSound,
		/// A script attaches a sound to the scenario's object, as ATTACH_SOUND_TAG does: the value gives the sound.
		/// Needs no creature.
		AttachObjectScriptSound,
		/// A script says a help text's line (value, by the scripts' number) on the first voice, as
		/// GAME_PLAY_SAY_SOUND_EFFECT does: from the point (from the middle of the map) on the land with ctrl, else heard
		/// everywhere. Needs no creature.
		SayLine,
		/// A sample of the guidance bank (value) with the volume and the pitch that the guidance's voices give: heard
		/// everywhere, or with ctrl from the point (from the middle of the map) on the land, heard as far as radius with
		/// the distances of a placed guidance voice. Needs no creature.
		PlayGuidanceVoice,
		/// The creature stops at once where it is, as a new body action or a placement stops it. The Stop command stops
		/// it as the game stops a walk.
		StopAtOnce,
		/// The creature turns to face the point (from the middle of the map) as the game's turn stages tell it to. While it
		/// walks to its stop, the turn does not start, and the command asks again each frame until the turn starts.
		TurnToFaceWhenStill,
		/// A script releases the scenario's object (object) into the game, as RELEASE_FROM_SCRIPT does. A villager
		/// that the script controls then decides what to do, and reacts again. Needs no creature.
		ScriptRelease,
		/// Each icon of the player's worship sites holds a part of the charge of its miracle (amount, 0 to 1), as if the
		/// worshippers charged it that far. Needs no creature.
		FillSiteIcons,
		/// The game saves into the quick save slot, as the quick save key does: a new picture of the screen and the
		/// slot's own name. Needs no creature
		QuickSave,
		/// The game continues as a plain start of the program does: the save on exit of the profile (slot 20) loads,
		/// and the menu shows over the paused game. Needs no creature.
		ContinueGame,
		/// The logo effect of the first start plays, with its sound, as before the first game of the program. Needs no
		/// creature.
		PlayLogoEffect,
		/// The player's worship site icon of a seed (value, a SpellSeedType) puts its miracle in the hand. The seed keeps
		/// a part of its charge (amount, 0 to 1), and the icon charges again for the hand, as when the hand takes a seed
		/// of an icon that charges. The battery of the site gets the full cost of the miracle. Needs no creature.
		ChargeIconWithSeedHeld,
		/// The scenario's object, a building going up, is finished at once, as when its builders put on the last of
		/// it. Needs no creature.
		FinishBuilding,
		/// A script native (script, by its name) runs at once with its arguments in order (scriptArguments). A position
		/// is three numbers. Needs no creature.
		CallNative,
		/// The scenario's object, an animal, runs away from the creature at once, as an animal runs away from a fire
		AnimalFlees,
		/// The testbed holds the search for the creature's route (value 1), so that it waits for its route, or lets the
		/// search go on (value 0)
		HoldRouteSearch,
		/// The game pauses (value 1) or continues (value 0), as the pause key does. Needs no creature.
		PauseGame,
		/// The game's speed changes to the amount, as the speed keys or a script change it: 1 is normal. Needs no
		/// creature.
		SetGameSpeed,
		/// A plan is forced on the creature, as an order from its player: the action by its name (action), done to the
		/// creature itself. The action's own agenda starts.
		ForceAction,
		/// The player's teleport stone nearest to the point goes at once, with its pool, as when its miracle ends. Needs
		/// no creature.
		RemoveTeleportStone,
		/// A script makes a dance (value, a row of the dance table) about the scenario's object (object), as
		/// DANCE_CREATE does. Each other villager of the scenario's objects joins the dance. With ctrl, the hand was
		/// near the dance for long enough, so the dance wants its camera. Needs no creature.
		MakeDance,
		/// The multiplayer front end opens over the temple's future room with the demonstration lobby, and goes to a
		/// box or a page (value, by testbed_multiplayer::Stage). Needs no creature.
		OpenMultiplayerBox,
		/// Each worship site of the player that is not built yet is built at once, with its altar, as when its builders
		/// put on the last of it. Needs no creature.
		BuildWorshipSites,
		/// The player presses a key (value, the key's scancode) on the keyboard and lets it go. The key goes through the
		/// queue of the window's events, as the real keyboard's keys do. Needs no creature.
		PressPlainKey,
		/// Another god's hand, as a network game reports it: the player (player) gets hands of its own, and its hand
		/// takes the scenario's object (object), or holds a miracle seed (heldSeed) in place of the object. Then the
		/// player reports its hand at the point (point), so high above the land (radius), and its camera 30 m above the
		/// hand. This computer does not move these hands. Needs no creature.
		OtherGodHand,
		/// A press of the Join Online Game button of the browser page: the game opens the multiplayer front end at the
		/// next frame that it may open in, also while a cut scene has the cinema bars in. Needs no creature.
		JoinOnline,
		/// The end box: the question to leave a network game opens (value 0), as Exit Online Game opens it, or the open
		/// end box opens the box of one of its tabs (value 1 Multiplayer, 2 Statistics), as a click on the tab does.
		/// Value 3 opens the main menu as it shows in a network game. Needs no creature.
		EndBox,
		/// The god of the player (player) brings a creature as it does at the start of a network game. The mind file
		/// (mindFile) of the creature mind folder of the game takes the place of the creature file of the god's player.
		/// The creature comes at the creature home of the god's temple. Needs no creature.
		BringCreature,
	};
	Kind kind {Kind::Stop};
	/// Which creature, by its place in the scenario's creatures
	size_t creature {0};
	/// The player whose hand or keys do it
	PlayerNames player {PlayerNames::PLAYER_ONE};
	/// Seconds after the last command before this one, counted once its creature is free when it waits for that
	float delaySeconds {0.0f};
	/// Waits until its creature has stopped moving and its body plays nothing, such as having arrived; for a command
	/// to the player's hand, until the hand has drawn the whole of the gesture it was drawing
	bool waitUntilFree {false};
	/// The point gone to, fled from or faced, from the middle of the map
	glm::vec2 point {0.0f};
	/// The velocity at which the player's hand throws the thing that it releases
	glm::vec3 velocity {0.0f};
	/// The animation played, the creature followed or fought, the band of a blow (high, middle, low), the step
	/// (forward, back, right, left), or whether it fights by itself (1) or not
	size_t value {0};
	/// The hour jumped to
	float hour {12.0f};
	/// The object picked up, knocked down or tied to, by its place in the scenario's objects
	size_t object {0};
	/// The miracle seed that another god's hand holds in place of the object
	std::optional<SpellSeedType> heldSeed;
	/// The action a script tells it to do, by its name in the game's table, and the object it does it with
	std::string_view action;
	std::optional<size_t> with;
	/// A globe that the scenario puts down, by its place among the scenario's globes. The creature picks up this globe
	/// in place of the object, or does the action with this globe. With onGlobe set, it does the action to this globe.
	/// The hand takes this globe in place of the object.
	std::optional<size_t> globe;
	bool onGlobe {false};
	/// A script's action is done to the scenario's first temple in place of the object
	bool onTemple {false};
	/// The leash put on
	LeashType leash {LeashType::Rope};
	/// The part of the body stroked, by its place in the hand's parts (the head first)
	size_t bodyPart {0};
	/// How high a slap lands, as a share of the creature's height, whether it is gentle, and which way it sweeps
	float slapHeight {0.85f};
	bool gentle {false};
	bool sweepsRight {false};
	/// How far from where it stands it is kept
	float radius {0.0f};
	/// How long a blow's click is held, charging it
	float chargeMs {0.0f};
	/// A desire's fraction of its maximum, or the seconds the camera's keys are held
	float amount {1.0f};
	/// The camera's keys are held with Ctrl rather than Shift
	bool ctrl {false};
	/// The creature a miracle is cast at, or that a script's action is done to in place of the object, by its place in
	/// the scenario's creatures
	std::optional<size_t> atCreature;
	/// The creature that a script's action uses, in place of the object (with), by its place in the scenario's creatures
	std::optional<size_t> usesCreature;
	/// The player's alignment jumped to, from -1 (evil) to 1 (good). For a recorded challenge, the alignment of the
	/// challenge, which moves the player's alignment on each record after the first.
	float alignment {0.0f};
	/// What an advisor is told: to come out of its corner, or to appear in a puff of smoke; to go home, or to vanish; to
	/// cling to or fly to a point on the screen (point, from 0 to 1 across and down); to point at that point on the
	/// screen, or at a point of the land (point, from the middle of the map), out in the world or not (gentle); to look
	/// at the point of the land; to play an anim (anim) at the point on the screen at a speed (amount); to feel an
	/// emotion (anim)
	enum class AdvisorAction : uint8_t
	{
		Out,
		Appear,
		Home,
		Vanish,
		Cling,
		Fly,
		PointOnScreen,
		PointAtLand,
		LookAtLand,
		PlayAnim,
		Feel,
	};
	AdvisorAction advisor {AdvisorAction::Out};
	uint32_t anim {0};
	/// The hand demonstration played, by its file's name in the game's hand demonstrations
	std::string_view handDemo;
	/// The title of a challenge recorded, by its help text's name
	std::string_view title;
	/// A script run, its arguments, and the globals set before it runs
	std::string_view script;
	/// The name of a mind file in the creature mind folder of the game
	std::string_view mindFile;
	std::vector<ScriptArgument> scriptArguments;
	std::vector<std::pair<std::string_view, ScriptArgument>> scriptGlobals;
};
[[nodiscard]] std::string_view Name(Command::Kind kind);
/// Whether a command is the player's mouse, which needs no creature
[[nodiscard]] bool IsPointerCommand(Command::Kind kind);

/// A flock of the land's birds, made as a land script makes one: the flock, then its birds about a point, each at a
/// random age
struct BirdFlockSetup
{
	AnimalInfo kind {AnimalInfo::Dove};
	/// Where the flock is made and its home, from the middle of the map
	glm::vec2 offset {0.0f};
	uint32_t count {10};
	/// How far from its home the leader's legs take it (its kind's way for 0), and how far from the leader the others
	/// keep
	float reach {0.0f};
	float flockDistance {10.0f};
};

/// A temple of a player's, built, at a point from the middle of the map, turned by the angle as a land's script turns
/// one (radians about the vertical)
struct TempleSetup
{
	glm::vec2 offset {0.0f};
	PlayerNames owner {PlayerNames::PLAYER_ONE};
	float angle {0.0f};
	/// How much of it is built, 0 to 1, for a temple still going up; built when not given
	std::optional<float> built;
	/// Its heart's life, 0 to 1, for a temple already hurt; whole when not given
	std::optional<float> life;
};

/// The player's hand held still over the land for the whole scenario, as a player holds it: from the middle of the map,
/// and how high above the land
struct HandHold
{
	glm::vec2 offset {0.0f};
	float height {6.0f};
};

/// One of the scenario's objects thrown into the physics as the player's hand throws it, again and again if asked
struct ThrowSetup
{
	/// The object, by its place in the scenario's objects
	size_t object {0};
	/// It is lifted to this point, from the middle of the map, before it is thrown; none to throw it from where it lies
	std::optional<glm::vec2> from;
	/// How high above the land it is let go
	float height {6.0f};
	glm::vec3 velocity {0.0f};
	/// Seconds after the scenario starts that it is thrown
	float delaySeconds {1.0f};
	/// Seconds after which it is thrown again, none for once. A throw due while it still flies waits for it to land.
	std::optional<float> repeatSeconds;
};

/// A scroll or sign as a land's script puts one up, at a point from the middle of the map; it comes after the
/// scenario's objects in their order, so commands reach it as the object after the last
struct HighlightSetup
{
	/// Its row of the highlights' info table, as a script gives it (0 plain, 1 sign, 2 silver, 3 gold)
	uint32_t kind {3};
	glm::vec2 offset {0.0f};
	/// The challenge it belongs to, as the script's own challenge gives it
	uint32_t challenge {1};
	/// A script's height above the land; none to stand on what is under it
	std::optional<float> height;
	/// A sign's tip text and category
	std::optional<std::pair<uint32_t, uint32_t>> tip;
	/// Started by the script as it is made
	bool active {false};
};

/// A footpath as a land's footpath file lays one: its nodes in order, at points from the middle of the map, starting
/// on an object's own place if given; and the objects whose footpaths it is, as the file's links give them
struct FootpathSetup
{
	std::optional<size_t> startsAt;
	std::vector<glm::vec2> nodes;
	std::vector<size_t> leadsFrom;
};

struct Scenario
{
	/// Unique and never changed, for picking it from the command line or a test
	std::string_view id;
	std::string_view name;
	Facet facet {Facet::Idle};
	/// What it sets up and what to look for
	std::string_view description;
	std::string_view expected;
	Environment environment;
	Framing framing;
	std::vector<CreatureSetup> creatures;
	std::vector<ObjectSetup> objects;
	std::vector<BirdFlockSetup> birdFlocks;
	std::vector<TempleSetup> temples;
	std::vector<ParticleSetup> particles;
	std::vector<VortexSetup> vortices;
	std::vector<RewardSetup> rewards;
	std::vector<DispenserSetup> dispensers;
	std::vector<MiracleCast> miracles;
	std::vector<LandStormSetup> landStorms;
	/// The scrolls and signs scripts put up, made after the objects
	std::vector<HighlightSetup> highlights;
	/// The footpaths between the objects, laid once they are made
	std::vector<FootpathSetup> footpaths;
	/// Lines of a land's script. The runner does these lines after it lays the footpaths. Each place is a distance
	/// from the middle of the map, as for the objects.
	std::string_view landScript;
	std::vector<Command> commands;
	/// After the last command, the commands go round again from this one
	std::optional<size_t> repeatFrom;
	/// The hand held still in view, rather than left wherever the mouse is
	std::optional<HandHold> hand;
	/// A testbed set-up step until worship sets it: the player's power multiplier for a tribe, put back to 1 when the
	/// scenario stops
	std::optional<std::pair<Tribe, float>> tribalPower;
	/// A testbed set-up step: the scenario's town believes this much in the player from the start, so that it stays the
	/// player's rather than going back to the neutral player at its first turn
	std::optional<float> townBeliefInPlayer;
	/// A testbed set-up step: the debug tool gives the scenario's town these miracles, as a land file line gives them. A
	/// town without a worship site then gets its site, on the first land too.
	std::vector<MagicType> townMiracles;
	/// A testbed set-up step: the scenario's town feels so about each of the scenario's creatures from the start, as if
	/// the creature had just shown it, rather than first seeing them as it plays
	std::optional<ecs::creature_attitude::Attitude> townFeelingAboutCreatures;
	/// A testbed set-up step: the scenario's town belongs to this player from the start, rather than to the player
	std::optional<PlayerNames> townOwner;
	/// A crowd spawned a batch a frame, after the creatures and objects above; the runner measures the frames once it
	/// is all there
	std::optional<Crowd> crowd;
	/// Every miracle's position and the land's height under it are logged this often, in seconds
	std::optional<float> logMiraclesEvery;
	/// Objects thrown by the player's hand
	std::vector<ThrowSetup> throws;
	/// The weights a caught firefly's miracle is drawn by, by the miracles' names, as a land's script sets them; the
	/// testbed's land sets none
	std::vector<std::pair<std::string_view, float>> fireflyRewards;
	/// A testbed set-up step: the scenario's town has its worship site put up built, as a land's script can, and its
	/// totem asks for this share of its people at worship, which sends them there as the game does
	std::optional<float> worshipShare;
	/// The things are made after the lines of the land script run, so that a thing of the scenario's town joins the town
	/// that the script made
	bool objectsAfterLandScript {false};
	/// The debug window opened as the scenario starts, by its name; none when empty
	std::string_view debugWindow;
	/// The first player's statistics as the scenario starts, for the Game Statistics box and the temple's scrolls
	std::function<void(ecs::components::PlayerStatistics&)> playerStatistics;
	/// What the first player's creature has counted of its life
	std::optional<statistics::CreatureTallies> creatureTallies;
	/// Sharks that a script makes as the game's whales, each at a point from the middle of the map. A shark that swims
	/// on no path shows only its body under the sea.
	std::vector<glm::vec2> sharks;
};

/// A town centre's totem
void AddVillageTotemScenarios(std::vector<Scenario>& all);
/// The boxes of the multiplayer front end over the temple's future room, with the demonstration lobby
void AddMultiplayerFrontEndScenarios(std::vector<Scenario>& all);
/// The hands of the other gods of a network game, as this computer shows them, with no game server
void AddOtherGodScenarios(std::vector<Scenario>& all);
/// The Join Online Game button of the browser page
void AddJoinOnlineScenarios(std::vector<Scenario>& all);
/// The sounds of a village by day and by night: its buildings, its foresters and its animals
void AddWorldSoundScenarios(std::vector<Scenario>& all);
/// A town given miracles by the debug tool, and its worship site going up
void AddTownMiracleScenarios(std::vector<Scenario>& all);
/// The villagers around the player's temple, with their debug window open
void AddTempleVillagerScenarios(std::vector<Scenario>& all);
/// The miracles' scenarios, added to every scenario by the registry
void AddMiracleScenarios(std::vector<Scenario>& all);
/// The miracles' globes and dispensers close up, and the hand's miracle effects
void AddGlobeScenarios(std::vector<Scenario>& all);
/// The heal, the lightning bolt, the creature spells and how the living react to miracles
void AddLifeLightScenarios(std::vector<Scenario>& all);
/// The blasts, fire and water
void AddBlastFireScenarios(std::vector<Scenario>& all);
/// What the blast spares and does on a coast, the water over fields and forests and before the people watching a fire
/// put out, and the hand catching a fireball or taking one into a fire seed
void AddFirewaterScenarios(std::vector<Scenario>& all);
/// A fish farm by a town: its shoal, the hand grabbing, scooping and scaring its fish
void AddFishScenarios(std::vector<Scenario>& all);
/// Creatures casting miracles
void AddCreatureCastingScenarios(std::vector<Scenario>& all);
/// A computer player's creature at the start of the computer mind
void AddComputerPlayerScenarios(std::vector<Scenario>& all);
void AddGameEndScenarios(std::vector<Scenario>& all);
/// The joint actions of two friends, each close up
void AddFriendActionScenarios(std::vector<Scenario>& all);
/// Creature Mode's and the Creature Cave's scenarios
void AddCreatureModeScenarios(std::vector<Scenario>& all);
/// The gestures drawn with the hand: sizing and powering up miracles, scribbles, and the leash's gestures
void AddGestureScenarios(std::vector<Scenario>& all);
/// The storm miracles: rain over a village, its fields and a fire, and the powered up storm's lightning
void AddStormScenarios(std::vector<Scenario>& all);
/// The flock miracles: doves, bats and wolves swept out by hand
void AddFlockScenarios(std::vector<Scenario>& all);
/// The land's birds of every kind, seagulls over the lake, the temples' doves and bats, and many flocks at once
void AddBirdScenarios(std::vector<Scenario>& all);
/// The teleport miracle: stones, villagers jumping between them
void AddTeleportScenarios(std::vector<Scenario>& all);
/// The vortices between the lands: opening, levelling the ground and closing
void AddVortexScenarios(std::vector<Scenario>& all);
/// The reward chests the challenges give: falling, landing and being opened
void AddRewardScenarios(std::vector<Scenario>& all);
/// The tornado's scenarios: through a village and a wood, and meeting a creature
void AddTornadoScenarios(std::vector<Scenario>& all);
/// The land's nature: a tree pulled up leaving its roots, and the fireflies at nightfall
void AddNatureScenarios(std::vector<Scenario>& all);
/// The shield and forest miracles: what each shield stops and what it costs, the forest on each ground, growing and
/// withering
void AddShieldForestScenarios(std::vector<Scenario>& all);
/// The physics' scenarios: things thrown and dropped, buildings broken, rocks split, bodies coming to rest, and how
/// much a frame costs while they do
void AddPhysicsScenarios(std::vector<Scenario>& all);
/// Whether a command is the player's alone, given whether or not the scenario has creatures
[[nodiscard]] bool NeedsNoCreature(Command::Kind kind);
/// The player's hand moving over the land, dragging it and turning and zooming the camera
void AddHandNavigationScenarios(std::vector<Scenario>& all);
/// The scenarios of how the hand looks for its player's alignment
void AddHandLookScenarios(std::vector<Scenario>& all);
/// The Norse gate of the creatures' glade and the gate stone plinth: opening, closing and laying the stones
void AddGateScenarios(std::vector<Scenario>& all);
void AddKnockScenarios(std::vector<Scenario>& all);
/// Villagers walking side by side, each at the speed its life, load, town and state give
void AddVillagerSpeedScenarios(std::vector<Scenario>& all);
void AddDiscipleScenarios(std::vector<Scenario>& all);
/// The labels over the villagers' heads: the names display as it grows in and out
void AddVillagerLabelScenarios(std::vector<Scenario>& all);
/// The temple key and the realm key of the world camera
void AddTempleKeyScenarios(std::vector<Scenario>& all);
/// The scrolls and signs the land scripts put up
void AddHighlightScenarios(std::vector<Scenario>& all);
/// The moon at night, seen to the east, for trying its phases and its path with the moon debug window
void AddSkyScenarios(std::vector<Scenario>& all);
/// The advisors coming out, clinging, pointing and acting
void AddAdvisorScenarios(std::vector<Scenario>& all);
/// A town growing from the buildings it planned
void AddTownGrowthScenarios(std::vector<Scenario>& all);
/// The player's statistics counting as their town lives
void AddStatisticsScenarios(std::vector<Scenario>& all);
void AddTutorialScenarios(std::vector<Scenario>& all);
/// A creature that dances with the villagers of a town (TestbedScenarioCreatureDance.cpp)
void AddCreatureDanceScenarios(std::vector<Scenario>& all);
/// A creature learning to build and to dance by watching villagers
void AddCreatureLearningScenarios(std::vector<Scenario>& all);
/// Creatures going to the places of their map of the land: a hill and its ridge, and the beach
void AddCreatureExplorationScenarios(std::vector<Scenario>& all);
/// The network map for 32 gods, with a citadel and a village in each slot
void AddGodsRingMapScenarios(std::vector<Scenario>& all);

/// Every scenario, in the order the window lists them
[[nodiscard]] std::span<const Scenario> All();
[[nodiscard]] const Scenario* Find(std::string_view id);
/// What is wrong with a scenario's data, if anything, one line each
[[nodiscard]] std::vector<std::string> Problems(const Scenario& scenario);
/// Whether the testbed's grid of dispensers stays for a scenario: unless it asks for none, or one of its creatures or
/// things would stand on it
[[nodiscard]] bool KeepsDispenserGrid(const Scenario& scenario);

/// A point given from the middle of the map, on the land
[[nodiscard]] glm::vec2 MapPoint(glm::vec2 middle, glm::vec2 offset);

/// About how tall a creature of a size stands
[[nodiscard]] float CreatureHeight(float size);

/// Where the camera is and what it looks at
struct CameraPlacement
{
	glm::vec3 origin;
	glm::vec3 focus;
};
/// The camera above and to the south of a box on the land, by its middle and half its size east to west and south to
/// north, looking down at its middle from far enough back for all of it to be seen through the fields of view, across
/// and up and down, in radians
[[nodiscard]] CameraPlacement Overview(glm::vec3 centre, glm::vec2 halfSize, glm::vec2 fieldOfView, float distanceFactor);
/// The camera behind and above a creature, by its height, looking at its middle from the south
[[nodiscard]] CameraPlacement Follow(glm::vec3 position, float height, float distanceFactor);
/// The camera in front of a creature's face, by its height, its position and the way it faces on the land
[[nodiscard]] CameraPlacement Head(glm::vec3 position, glm::vec2 ahead, float height, float distanceFactor);
/// The box round points on the land, at least so big: its middle and half its size
struct Bounds
{
	glm::vec2 centre;
	glm::vec2 halfSize;
};
[[nodiscard]] Bounds BoundsOf(std::span<const glm::vec2> points, glm::vec2 minimumHalfSize);

/// Sets a body's needs to the overrides; the energy is at most the creature's size, as a meal can fill it
void Apply(const NeedOverrides& overrides, creature_physiology::Needs& needs, float size);
/// Sets desires to their fractions of their maxima, activating them
void Apply(std::span<const DesireOverride> overrides, creature_desires::Desires& desires);

/// Where the timeline of a scenario's commands is
struct Timeline
{
	/// The next command, and the seconds counted towards its delay: since the last was given, or since its creature
	/// was free when it waits for that
	size_t next {0};
	float seconds {0.0f};
	/// Seconds since the last command was given
	float sinceGiven {0.0f};
	/// The next command's creature has been free since the last command, so its delay counts on whatever the
	/// creature's idle mind has it do meanwhile
	bool freed {false};
	/// Every command has been given and none go round again
	bool done {false};
};
/// The timeline some seconds on: the commands due now, in order. A command waiting for its creature (or the hand) to
/// be free asks isFree; its delay only counts from when it first is.
[[nodiscard]] std::vector<size_t> Advance(Timeline& timeline, std::span<const Command> commands,
                                          std::optional<size_t> repeatFrom, float seconds,
                                          const std::function<bool(const Command& command)>& isFree);

} // namespace openblack::testbed_scenarios
