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
#include <bitset>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureMarks.h"
#include "Creature/CreatureTattoo.h"
#include "Creature/LeashKeys.h"
#include "Enums.h"

/// The commands of the players. The game does not change the world at the time of the player's input. The input makes a
/// command, and the command changes the world at the start of a later game turn. Every player, also the one player of a
/// single game, sends the commands this way, so that all the computers of a network game do the same commands at the
/// same turn.
///
/// The steps of a command:
/// 1. The input puts the command in the open block of its player.
/// 2. Once a frame, after the turns of the frame, the game sends the open block. A block that becomes full is sent at
///    once.
/// 3. At the start of each turn, before the hand's own work of the turn, the game does the commands of all the blocks
///    that it sent, in the order that it sent them. Each command has the number of the player that sent it.
///
/// Thus a command never changes the world in the turn that runs in the same frame step as its input, and the world sees
/// it at the start of the next turn after the send.
namespace openblack::player_commands
{

/// A leash shortcut key: the leash key puts on or takes off the leash, and the two other keys change the leash
struct LeashKey
{
	creature_leash::LeashKey key;
};

/// A click on a creature, which puts the leash on the player's creature
struct LeashTap
{
	entt::entity creature;
};

/// The hand let go of a creature that it held: the creature feels how the hand stroked or slapped it
struct CreatureFeedback
{
	entt::entity creature;
	/// The sum of the strokes and the slaps, in the range from -1 to 1
	float amount;
};

/// The hand takes the thing that it waited for: the thing leaves the land, and the hand holds it
struct HandTake
{
	entt::entity object;
};

/// Where a thing starts when the hand lets go of it. The computer of the player that lets go makes these values from the
/// thing as that computer last drew it in the hand: a globe turns to the camera of that player. The command sends the
/// values to all the computers, thus each computer starts the same path.
struct ThrowStart
{
	/// The spin of the thing, as the bodies count the turn
	glm::vec3 angularMomentum {0.0f};
	/// The origin of the thing's model
	glm::vec3 origin {0.0f};
	/// The angles of the thing's turn about x, y and z. The game turns about z first, then about x, then about y.
	glm::vec3 angles {0.0f};
};

/// The hand lets go of the thing that it holds. It puts the thing down, or throws it at the velocity that the hand had
/// when the player let go of the button. The thing starts from the start in the command.
struct HandLetGo
{
	entt::entity object;
	glm::vec3 velocity;
	ThrowStart start {};
};

/// The hand taps a thing
struct HandTap
{
	entt::entity object;
};

/// The hand gives the thing that it holds to a creature
struct HandGive
{
	entt::entity creature;
	entt::entity object;
	/// Whether the player who gives has a left hand. The creature takes the thing with its matching hand on every
	/// computer, so the command carries the side of the hand of the computer that sends it.
	bool leftHand {false};
};

/// An order to the player's creature on its leash
struct LeashOrder
{
	enum class Kind : uint8_t
	{
		/// The creature acts on a thing
		OnThing,
		/// The creature goes to a place on the land
		AtPlace,
		/// The leash is tied to a thing
		Tie,
		/// The leash comes back to the hand from the thing that it is tied to
		Untie,
	};
	Kind kind;
	/// The thing of an order on a thing or of a tie
	entt::entity object {entt::null};
	/// The place of an order at a place
	glm::vec3 place {0.0f};
};

/// The tap of the player's hand on a leash that hangs at the temple
struct LeashPostTap
{
	entt::entity post;
};

/// The report of where the player's hand and camera are. The input of the player moves them, and the world reads them
/// only from the last report that a turn did.
struct HandReport
{
	/// The point under the cursor that the interface picks
	glm::vec3 collidePoint;
	/// The place of the hand
	glm::vec3 handPoint;
	/// The place of the camera
	glm::vec3 camera;
	/// The point that the camera looks at
	glm::vec3 focus;

	bool operator==(const HandReport& other) const = default;
};

/// The twist of a thing that the hand let go, after the hand moved a little: the turn turns the thing's body about
/// the level axis across the motion of the hand since the let go
struct HandTwist
{
	entt::entity object;
	/// How far the hand moved since it let go of the thing
	glm::vec3 moved;
};

/// The press of the hand on a thing that it locks on to: the hand scoops from a pile, a field or a fish farm, or holds a
/// creature to stroke or slap it. The lock stays until the hand sends the end of the lock (HandUnlock).
struct HandLock
{
	entt::entity object;
};

/// The end of the lock of the hand on a thing (HandLock), when the player lets go of the button
struct HandUnlock
{
	entt::entity object;
};

/// The player let go of the button that holds a creature. The creature stops its look at the hand, and the world keeps
/// the lock until the hand sends its end (HandUnlock). The hand sends the end only when the creature is still.
struct HandLockRelease
{
	entt::entity creature;
};

/// The press of the hand that holds a thing on a target that the thing is used on: a pot goes into a store or a pile,
/// a thing goes whole to an altar, a gate stone plinth or a store
struct HandApply
{
	entt::entity target;
};

/// The hand let go of a town's totem at a share of worship: the town takes that share
struct TotemShare
{
	entt::entity totem;
	/// The share of the town's people that go to worship, from 0 to 1
	float share;
};

/// A hard slap of the hand bruised a creature: the slapping player's own creature loses some life, and the slapped
/// creature, when it is that creature, takes the wound where the slap met its skin
struct CreatureBruise
{
	entt::entity creature;
	/// Where the slap met the skin of the slapped creature, which the frame measured; none for a miss
	std::optional<creature_marks::Mark> wound;
};

/// A press of the hand on the ground of the arena while the hand directs the fight of the player's creature. The turn
/// makes the creature step towards the point, from where the creature stands and the way that it faces then.
struct FightGround
{
	entt::entity creature;
	/// The point on the ground that the hand pressed
	glm::vec3 point;
	/// The Move button makes the step in place of the queued moves; the Action button adds it after them
	bool replace;
};

/// A move of the player's fighting creature that the hand or a gesture of the player gives: a press on the opponent or
/// on the player's creature, the special move or a miracle of the fight. The turn adds the move to the queue of the
/// sending player's creature.
struct FightMove
{
	/// What the hand pressed on: the opponent or the player's own creature, or the creature of a gesture
	entt::entity target;
	/// The kind of the move (a creature_fight::Move::Kind) and its value: the animation or the miracle
	uint8_t kind;
	uint32_t value;
	/// The move goes in place of the queued moves (the Move button), or after them
	bool replace;
};

/// The player let go of the button that pressed on a body in a fight: the blow that waits last in the queue of the
/// sending player's creature gets the charge
struct FightCharge
{
	/// How long the button was held, in milliseconds
	uint32_t heldMs;
};

/// A stroke or a slap of the creature that the hand holds, which the creature takes: every computer plays the creature's
/// reaction at the same turn. The face of a stroke is not part of it: only the computer of the hand shows that face.
struct CreatureReaction
{
	entt::entity creature;
	/// The animation of the reaction and whether it plays mirrored
	uint32_t animation;
	bool mirrored;
	/// The share of the way through what the body plays that the reaction can stop it at
	double interruptsAfter;
	/// The reaction is to a slap, not to a stroke
	bool slap {false};
};

/// A click of the hand in a network game: the turn writes the click that the scripts read, so that the scripts of
/// every computer read the same click
struct HandClick
{
	/// The thing that the hand clicked, or none
	entt::entity thing;
	/// The click is the let go of a press: with no thing, the place on the land is clicked, or with no place the thing
	/// is forgotten
	bool release;
	/// The place on the land of a let go, in the fixed units of the map
	bool hasPlace;
	int32_t placeX;
	int32_t placeZ;
};

/// A gesture of the hand on the miracle in the hand: a power-up, a scribble or a shake. The turn powers the seed up,
/// stops its power-up or drops it.
struct SeedGesture
{
	enum class Kind : uint8_t
	{
		PowerUp,
		Scribble,
		Shake,
	};
	Kind kind;
	/// The level of a power-up
	int8_t level;
	/// The icon that charges the seed to the level of a power-up. The gesture sends the icon of the seed in the hand.
	/// The turn charges at this icon, not at the icon of the seed that the hand holds at the turn.
	entt::entity icon {entt::null};
};

/// The player of the command left a network game, or did not come back in time. Only the game server sends it. The
/// world defeats the player's god at the turn, and the god's slot stays closed.
struct PlayerLeft
{
};

/// The thing under the cursor of the player's hand in a network game, sent when it changes. The turn keeps it with the
/// report of the hand, so that every computer counts the same time that the hand stays over the creature.
struct HandPick
{
	/// The thing under the cursor, or none
	entt::entity object;
};

/// The player of the command is a human god of the network game from its start. Only the game server sends it, for each
/// human in the first turn. The world gives the player hands of its own, makes the player when the land has none, and
/// gives each slot with no human a computer god.
struct PlayerJoined
{
};

/// The player of the command is the god of the observers: the first observer is in the network game. Only the game
/// server sends it. The world makes the player when the land has none and gives it hands of its own. The god has no
/// citadel, so it has no influence, no miracles and no creature. All the observers are humans of this god.
struct ObserversJoined
{
};

/// The player of the command is the god of the observers, and the last observer left the network game. Only the game
/// server sends it. The god gets the next finishing place, each other human god that is still in counts one more god
/// defeated, and a computer mind plays the god. The god is not out of the game.
struct ObserversLeft
{
};

/// An observer came into the network game. Only the game server sends it, stamped with the god of the observers and the
/// number of the observer. The observer is a human of that god, with its own hand and camera. The god takes the name of
/// the observer, so the god has the name of the last observer that came in.
struct ObserverIn
{
	std::u16string name;

	[[nodiscard]] bool operator==(const ObserverIn&) const = default;
};

/// An observer left the network game: its session ended. Only the game server sends it, stamped with the god of the
/// observers and the number of the observer. The other computers stop the sparkle at the camera of the observer.
struct ObserverOut
{
};

/// The miracle selection chose a miracle: the player's icons give the miracle of the seed to the hand, or one of them
/// starts to charge it. The turn checks again that an icon may do it.
struct MiracleRequest
{
	SpellSeedType seed;
};

/// The R gesture or the R key: the player's icons give the last miracle that went into the player's hand again, or one
/// of them starts to charge it. The turn reads that miracle and checks again that an icon may do it.
struct MiracleRepeat
{
};

/// The cast of the miracle in the player's hand, as the hand of that player found it: on a thing, or at a point on the
/// land. A miracle that the hand keeps locked on sends one cast at each turn that the button stays down. The turn checks
/// again that the miracle may be cast there, then casts it with the hand as the command gives it. Thus every computer
/// casts the same miracle at the same turn.
struct HandCast
{
	/// The thing that the miracle is cast on, or none for a cast on the land
	entt::entity object {entt::null};
	/// The point on the land: the point under the hand, or the middle of the circle that the player drew
	glm::vec3 point {0.0f};
	/// The size of the miracle: the radius of the circle that the player drew, or the plain size
	float size {0.0f};
	/// The place of the hand at the cast
	glm::vec3 handPosition {0.0f};
	/// The direction that the camera looks in at the cast
	glm::vec3 cameraForward {0.0f, 0.0f, 1.0f};
	/// The smoothed velocity of the hand at the cast
	glm::vec3 handVelocity {0.0f};
	/// The spin that the movement of the hand gives the miracle
	float spin {0.0f};
};

/// The player let go of the button that kept a miracle locked on in the hand: the miracle stops, and the seed keeps what
/// the miracle has left
struct HandCastUnlock
{
	/// The seed in the hand
	entt::entity seed {entt::null};
};

/// The gesture of the leash picker chose a leash: the creature of the player changes to the leash of this type, if it
/// knows that leash
struct LeashPick
{
	LeashType type;
};

/// A scribble of the hand shakes off the thing that the hand holds. The thing falls from where the hand holds it, with
/// no speed, and the turn does not plant it again. A miracle in the hand goes, and what its seed still holds goes back to
/// the player's worship.
struct HandShakeOff
{
};

/// A scribble of the empty hand stops the charge of an icon at a worship site of the player. Of the icons that charge a
/// miracle for the hand of the player, the icon that started its charge last stops it.
struct IconChargeStop
{
};

/// The player clicked the dialogue of the help texts, or held the key that cuts a text short (the keypad's Enter), in
/// the frames since the last command. The help is of this computer only: only the computer of the player gives them to
/// its dialogue, at the turn. Thus the scripts see the click at the same turn at all frame rates.
struct DialogueClick
{
	bool click {false};
	bool skipKey {false};

	bool operator==(const DialogueClick& other) const = default;
};

/// The hand tapped the land with nothing to take and no leash order to give. At the turn, the maze puzzles take the
/// tapped place for that turn only.
struct PuzzleTap
{
	/// The tapped place in the fixed units of the map
	int32_t placeX {0};
	int32_t placeZ {0};

	bool operator==(const PuzzleTap& other) const = default;
};

/// The hand of the player went into the sea. At the turn, the shoals of the fish puzzle take the point as a scare. Thus
/// the fish of every computer dart away at the same turn.
struct FishScare
{
	/// The point in the sea where the hand went in
	glm::vec3 point {0.0f};

	bool operator==(const FishScare& other) const = default;
};

/// Tools: the creature of the player starts a fight with the nearest creature, or a fight that it is in stops. Only a
/// game server with its test commands on lets this command through.
struct CreatureFightCheat
{
};

/// The hand of the player gripped the land: the turn makes a puff of dust at the point on the ground. Thus every computer
/// makes the same dust at the same turn.
struct GripDust
{
	/// The point on the ground under the hand at the grip
	glm::vec3 point {0.0f};
};

/// The report of what the player did that the help counts, since the last report: the help events of the input of the
/// player, and what the world camera of the player shows. Only the computer of the player counts the report, at the
/// turn. Thus the help counts the same things at the same turns at all frame rates.
struct HelpReport
{
	/// One bit for each help event: bit n is event n (see help::profile)
	uint64_t events {0};
	/// A front-end box shows, for example the menu of the game. The help then counts nothing.
	bool boxShows {false};
	/// The cursor is on a point of the world. Then the point of the report of the hand (HandReport) is under the cursor.
	bool cursorOnPoint {false};
	/// The events of the view of the world camera, which the help counts each turn until the next report (see
	/// help::profile::CameraViewEvents). None when a script, the temple or a creature has the camera.
	bool cameraSeesLand {false};
	bool cameraNearAndSteep {false};
	bool cameraSeesSky {false};

	bool operator==(const HelpReport& other) const = default;
};

/// The Help key: the advisors tell the player about a thing, else about the ground at a point. The computer of the
/// player finds the thing and the point as its hand shows them. Only that computer says the help, at the turn.
struct HelpQuery
{
	/// The thing in the hand or under the hand, or none
	entt::entity thing {entt::null};
	/// The point on the ground under the hand, if the hand is over the ground
	bool hasGround {false};
	glm::vec3 ground {0.0f};
};

/// The hand moved too far from the creature that reaches for the thing that the hand gave it. The creature gives up the
/// take, unless it already has the thing, and the hand of the player forgets the gift.
struct GiveCallOff
{
	entt::entity creature {entt::null};
};

/// The tattoo editor changed the tattoos of a creature: its slots, and the block of the player's own pictures when the
/// editor made the block again. The turn puts them on the creature on every computer.
struct CreatureTattoo
{
	entt::entity creature {entt::null};
	creature_tattoo::Slots slots {};
	/// When set, the block of pictures changes too (see creature_tattoo::PicturesBlock)
	bool hasPictures {false};
	std::vector<uint8_t> pictures;

	bool operator==(const CreatureTattoo& other) const = default;
};

/// The hand let go of the rope of the leash of the player's creature, which the leash ties to a thing. The leash becomes
/// this long, and its slack length a share of it.
struct LeashLength
{
	float length {0.0f};
};

/// The report of the view of the player that the scripts and the advisors' remarks read: the view of the camera, the
/// arrival of the camera and the grip of the hand on the land. The computer of the player sends it once a turn when the
/// eye or the focus moved by more than 0.01 on an axis, or when a different field changed. The turns read it, in a game
/// alone too. Thus the scripts get the same answers at all frame rates.
struct ViewReport
{
	/// The eye and the point that the camera looks at, as the frame draws them
	glm::vec3 eye {0.0f};
	glm::vec3 focus {0.0f};
	/// The lens: the view across the width of a 4:3 screen, in radians
	float lens {0.0f};
	/// How far the near plane is ahead of the eye
	float nearClip {0.0f};
	/// The size of the window that the frame draws into, in pixels, and the shape of the camera's picture (width over
	/// height). The screen tests of the scripts and the advisors' remarks use the window of the player who reports, so
	/// that every computer and the game server get the same answer.
	uint16_t screenWidth {0};
	uint16_t screenHeight {0};
	float aspect {0.0f};
	/// How far the camera is from the fence of the camera zones, as the world camera finds it
	float inclusionDistance {0.0f};
	/// How many camera orders of the scripts the camera has got (ecs::RegistryContext::scriptCameraOrders)
	uint32_t cameraOrders {0};
	/// The camera has arrived where it goes
	bool cameraArrived {false};
	/// The hand grips the land with the select button
	bool gripsLand {false};
	/// The other button is held too
	bool otherButton {false};
	/// The mouse has a wheel: the computer's system says so, or the wheel has turned
	bool mouseWheel {false};
	/// The controls of the world camera asked for a turn of more than a hundredth in a frame of this turn
	bool cameraTurned {false};
	/// The camera icons by the hand of the world camera's last frame, with their offered bits (hand_tricons::icon). The
	/// lessons of the Gods' Playground read them as the turn of the camera at the edge of the screen.
	uint16_t cameraIcons {0};
	/// The keys held, by the game's key number (the keyboard's scan code, with 0x80 added for the extended keys)
	std::bitset<255> keysHeld;
	/// How many buttons the computer's system says the mouse has
	uint32_t mouseButtons {0};

	bool operator==(const ViewReport& other) const = default;
};

/// The player goes into their temple. When the player holds the leash of their creature, the leash comes off.
struct TempleLeashOff
{
};

/// A line of the chat that the player typed. The line goes to the players of the list, by god slot. Each computer does
/// the command at the same turn: the line shows only to the players of the list, but the creatures of the sender and of
/// the players of the list say it in their bubbles on every computer. An empty list sends it to all players. The line
/// is not a part of the synced world.
struct ChatSpeech
{
	std::vector<PlayerNames> recipients;
	std::u16string text;

	[[nodiscard]] bool operator==(const ChatSpeech&) const = default;
};

/// A line of the quick chat that the player picked: a taunt, by its number in the list of taunts. The advisors of every
/// computer say the taunt at the turn, and the line goes to the players of the list as a typed line does.
struct ChatTaunt
{
	std::vector<PlayerNames> recipients;
	uint32_t taunt {0};

	[[nodiscard]] bool operator==(const ChatTaunt&) const = default;
};

/// The quick save key. The key checks the rules of a quick save, and the turn saves the world into the quick save slot.
/// Only a single player game sends it.
struct QuickSave
{
};

/// The quick load key. The key checks the rules of a quick load. The turn asks for the load of the quick save slot, and
/// the game loads it after the draw of the frame. Only a single player game sends it.
struct QuickLoad
{
};

/// The custom music of the player started a track, or stopped. When a track starts, the creature of the player reacts
/// to what it felt before about the track. Only a single player game sends it.
struct MusicMood
{
	/// The creature of the player when the music changed
	entt::entity creature;
	/// The track that plays, 0 when no track plays
	uint32_t track {0};
	/// What the creature felt before about the track
	float mood {0.0f};
};

/// An edit of the editor in a network game. Only a player with the editor claim opens the editor there, and the game server
/// refuses the edits of every other player. Each computer does the edit at the turn, so the world stays the same on each
/// computer.
struct EditorEdit
{
	enum class Kind : uint8_t
	{
		/// Put an item of the palette on the land at the position, facing the angle
		Place,
		/// Move the thing to the position
		Move,
		/// Turn the thing about the up axis by the angle
		Turn,
		/// Remove the thing from the land
		Remove,
		/// Change the size of the thing to the scale
		Scale,
		/// Put the thing to the full pose: the position, the turn, the angles that it keeps and the scale of each axis.
		/// An undo and a redo of the editor send it, so that the thing gets back the same bits.
		Pose,
	};
	Kind kind {Kind::Move};
	/// The thing of a move, a turn or a removal. None for a placement.
	entt::entity object {entt::null};
	/// The kind of the item of the palette of a placement (editor::PlaceKind)
	uint8_t placeKind {0};
	/// The type of the item within its kind
	int32_t placeType {0};
	/// The challenge of a scroll, or the text of a tip sign
	uint32_t challenge {0};
	/// The position of a placement or a move
	glm::vec3 position {0.0f};
	/// The angle of a placement or a turn, in radians
	float radians {0.0f};
	/// The new scale of the first axis of a thing. The other axes keep their ratio to it.
	float scale {1.0f};
	/// The number that the editor of the sender gives to the edit, so that the turn tells it what the edit made. 0 when
	/// the editor needs no answer.
	uint16_t request {0};
	/// The turn of the thing of a pose
	glm::mat3 rotation {1.0f};
	/// When set, the thing of a pose keeps the angles with its turn
	bool keepsAngles {false};
	/// The angles that the thing of a pose keeps with its turn
	glm::vec3 keptAngles {0.0f};
	/// The scale of each axis of the thing of a pose
	glm::vec3 poseScale {1.0f};

	[[nodiscard]] bool operator==(const EditorEdit&) const = default;
};

/// An edit of the editor to a value of a creature (creature_edit_values::Value), such as its alignment, its size or a need
/// of its body. Only a player with the editor claim sends it in a network game, and the game server refuses it from every
/// other player.
struct EditorCreatureEdit
{
	entt::entity creature {entt::null};
	/// The value (creature_edit_values::Value)
	uint8_t value {0};
	/// The new amount of the value. Each computer keeps it in the limits of the value.
	float amount {0.0f};

	[[nodiscard]] bool operator==(const EditorCreatureEdit&) const = default;
};

/// An act of the creature tools of the editor (creature_edit_acts::Act), such as an order to the creature, a change to its
/// mind or to its leash, or a move of its fight. The editor finds the things and the places of the act, so that each
/// computer does the same act. Only a player with the editor claim sends it in a network game, and the game server refuses
/// it from every other player.
struct EditorCreatureAct
{
	/// The creature, or none for an act on every creature (creature_edit_acts::ActsOnEveryCreature)
	entt::entity creature {entt::null};
	/// The act (creature_edit_acts::Act)
	uint8_t act {0};
	/// The other thing of the act, such as the thing to pick up or the creature to fight
	entt::entity other {entt::null};
	/// The place of the act, such as the place to go to
	glm::vec3 position {0.0f};
	/// The amount of the act, such as the charge of a blow
	float amount {0.0f};
	/// The index of the act in its list, such as the skill to see
	uint16_t index {0};
	/// The small numbers of the act, such as the switch of a box
	std::array<uint8_t, 6> args {};

	[[nodiscard]] bool operator==(const EditorCreatureAct&) const = default;
};

/// An edit of the editor to a field of a villager, such as its life, its age, its tribe or one of its states. Only a player
/// with the editor claim sends it in a network game, and the game server refuses it from every other player.
struct EditorVillagerEdit
{
	enum class Field : uint8_t
	{
		/// The life, 0 to 1, in the amount
		Life,
		/// The age in years, in the number
		Age,
		/// The food, in the amount
		Food,
		/// The life stage (Villager::LifeStage), in the number
		LifeStage,
		/// The sex (Villager::Sex), in the number
		Sex,
		/// The tribe (Tribe), in the number
		Tribe,
		/// The role (VillagerNumber), in the number
		Role,
		/// The state (VillagerStates) of the slot of states in the index, in the number
		State,
	};
	entt::entity villager {entt::null};
	Field field {Field::Life};
	/// The slot of states of a state (LivingAction::Index)
	uint8_t index {0};
	/// The new number of the field
	uint32_t number {0};
	/// The new amount of the field
	float amount {0.0f};

	[[nodiscard]] bool operator==(const EditorVillagerEdit&) const = default;
};

/// A new creature of the creature tools of the editor, with the settings of the tools on the editor computer. Each computer
/// makes the creature at the same turn. Only a player with the editor claim sends it in a network game, and the game server
/// refuses it from every other player.
struct EditorCreatureSpawn
{
	/// The species of the creature (CreatureType)
	uint8_t species {0};
	/// The player that owns the creature (PlayerNames)
	uint8_t owner {0};
	/// The point on the land where the creature stands
	glm::vec3 position {0.0f};
	/// The angle that the creature faces, in radians
	float radians {0.0f};
	/// The size of the creature
	float scale {1.0f};
	float alignment {0.0f};
	float fatness {0.5f};
	float strength {0.5f};
	/// When set, the creature has the tattoos
	bool hasTattoos {false};
	creature_tattoo::Slots tattoos {};
	/// The number that the editor of the sender gives to the new creature, so that the turn tells it which creature it
	/// made. 0 when the editor needs no answer.
	uint16_t request {0};

	[[nodiscard]] bool operator==(const EditorCreatureSpawn&) const = default;
};

/// The editor makes a thing again from all that it kept of the thing: the item of the palette, the full pose and, for a
/// creature, its body. An undo of a removal, a redo of a placement and a copy of a creature send it. Only a player with
/// the editor claim sends it in a network game, and the game server refuses it from every other player.
struct EditorRemake
{
	/// The number that the editor of the sender gives to the new thing, so that the turn tells it which thing it made
	uint16_t request {0};
	/// The kind of the item of the palette (editor::PlaceKind)
	uint8_t placeKind {0};
	/// The type of the item within its kind
	int32_t placeType {0};
	/// The challenge of a scroll, or the text of a tip sign
	uint32_t challenge {0};
	glm::vec3 position {0.0f};
	glm::mat3 rotation {1.0f};
	/// When set, the thing keeps the angles with its turn
	bool keepsAngles {false};
	glm::vec3 keptAngles {0.0f};
	/// The scale of each axis
	glm::vec3 scale {1.0f};
	/// When set, the thing is a creature with the body below
	bool creature {false};
	/// The player that owns the creature (PlayerNames)
	uint8_t owner {0};
	/// The mind of the creature
	uint32_t mind {0};
	/// The size of the creature
	float size {1.0f};
	float alignment {0.0f};
	float fatness {0.5f};
	float strength {0.5f};

	[[nodiscard]] bool operator==(const EditorRemake&) const = default;
};

/// A stroke of the land brush of the editor: it raises or lowers the land, or paints its country, in a circle round a
/// point. Each computer does the stroke on its own land at the same turn. Only a player with the editor claim sends it in
/// a network game, and the game server refuses it from every other player.
struct EditorLandStroke
{
	/// The mode of the brush (editor::land_brush::Mode): raise, lower or paint
	uint8_t mode {0};
	/// The radius of the circle that takes the full change, in metres
	float innerRadius {0.0f};
	/// The radius of the circle out of which nothing changes, in metres
	float outerRadius {0.0f};
	/// The altitude steps that a corner in the inner circle goes up or down
	uint8_t step {0};
	/// The country that a paint gives, an index into the countries of the land
	uint8_t country {0};
	/// The middle of the stroke on the land, x and z in metres
	glm::vec2 centre {0.0f};

	[[nodiscard]] bool operator==(const EditorLandStroke&) const = default;
};

/// The most points of a path of the editor
inline constexpr size_t k_MostEditorPathPoints = 256;

/// A footpath or a stream that the editor drew, point by point. Each computer makes it at the same turn, and the land of
/// that turn gives the height of each point. Only a player with the editor claim sends it in a network game, and the game
/// server refuses it from every other player.
struct EditorPath
{
	/// The kind of the path (editor::paths::Kind): a footpath or a stream
	uint8_t kind {0};
	/// The points of the path on the land, x and z in metres, in the order of the clicks
	std::vector<glm::vec2> points;
	/// The building that the first point of a footpath is on, which the footpath leads from. None for no building.
	entt::entity firstHolder {entt::null};
	/// The building that the last point of a footpath is on. None for no building.
	entt::entity lastHolder {entt::null};

	[[nodiscard]] bool operator==(const EditorPath&) const = default;
};

/// A mind file of the creature tools of the editor goes into a creature at the turn. The file goes to the game server on
/// the game socket before the command, and the server sends it to each computer before the turn that holds the command.
/// Each computer finds the file by the hash of its bytes. Only a player with the editor claim sends it in a network game,
/// and the game server refuses it from every other player.
struct EditorMindLoad
{
	/// What the mind file does
	enum class Use : uint8_t
	{
		/// The creature takes the learning, the desires and the stage of growing up of the file
		LoadInto = 0,
		/// A new creature with the body of the file takes the mind of the file
		Spawn = 1,
	};

	Use use {Use::LoadInto};
	/// The hash of the bytes of the mind file (editor::mind_loads::HashOf)
	uint64_t file {0};
	/// The creature that takes the mind. Only for LoadInto.
	entt::entity creature {entt::null};
	/// The new creature. Only for Spawn.
	EditorCreatureSpawn spawn {};

	[[nodiscard]] bool operator==(const EditorMindLoad&) const = default;
};

/// The most parameters of a script that the Scripts tab of the editor starts
inline constexpr size_t k_MostEditorScriptParameters = 16;

/// An act of the Scripts tab of the editor on the script machine: a task starts, holds, steps, continues or stops, a
/// breakpoint changes, or a variable takes a new value. Each computer does the act at the same turn. Only a player with
/// the editor claim sends it in a network game, and the game server refuses it from every other player.
struct EditorScript
{
	enum class Act : uint8_t
	{
		/// A new task of the script id starts with the parameters
		Start = 0,
		/// Each task of the script id stops
		StopScript = 1,
		HoldAll = 2,
		ContinueAll = 3,
		/// The held task id does one instruction
		StepTask = 4,
		/// The held task id runs again
		ContinueTask = 5,
		HoldTask = 6,
		StopTask = 7,
		/// The breakpoint at the address goes on (index 1) or off (index 0)
		Breakpoint = 8,
		/// All the breakpoints go off
		RemoveBreakpoints = 9,
		/// The global variable id takes the value
		SetGlobal = 10,
		/// The local variable index of the task id takes the value
		SetTaskVariable = 11,
	};

	Act act {Act::Start};
	/// The script, the task or the global variable of the act
	uint32_t id {0};
	/// The address of a breakpoint
	uint32_t address {0};
	/// The local variable of a task, or 1 for a breakpoint that goes on and 0 for one that goes off
	uint16_t index {0};
	/// The bits of the new value of a variable. The variable keeps its type.
	uint32_t value {0};
	/// The parameters of a start
	std::vector<float> parameters;

	[[nodiscard]] bool operator==(const EditorScript&) const = default;
};

/// An act of the Land script tab of the editor: the stepper reads a land script, or does, skips or stops at its commands.
/// The editor system of each computer keeps the stepper, and does the act at the same turn. Only a player with the editor
/// claim sends it in a network game, and the game server refuses it from every other player.
struct EditorLandScript
{
	enum class Act : uint8_t
	{
		/// The stepper reads the script of the land that the game loaded last
		LoadedLand = 0,
		/// The stepper reads the text of a land script of the editor, by the hash of its bytes
		Open = 1,
		Step = 2,
		Continue = 3,
		/// The stepper does the commands up to the command index, and not that command
		RunTo = 4,
		Skip = 5,
		Restart = 6,
		/// The breakpoint of the command index goes on or off
		Breakpoint = 7,
	};

	Act act {Act::LoadedLand};
	/// The command of RunTo and Breakpoint
	uint32_t command {0};
	/// The hash of the text of an Open (editor::mind_loads::HashOf)
	uint64_t file {0};

	[[nodiscard]] bool operator==(const EditorLandScript&) const = default;
};

/// The Scripts tab of the editor changes the program of the script machine: it compiles the edited source of a script,
/// opens another program, or opens the program of the game again. The file goes to the game server on the game socket
/// before the command, and the server sends it to each computer before the turn that holds the command. Only a player with
/// the editor claim sends it in a network game, and the game server refuses it from every other player.
struct EditorScriptProgram
{
	enum class Use : uint8_t
	{
		/// Each computer compiles the source of the file into the script id
		Compile = 0,
		/// The compiled program of the file takes the place of the program
		Open = 1,
		/// The program of the game takes the place of the program again. It uses no file.
		GamesProgram = 2,
	};

	Use use {Use::Compile};
	/// The hash of the bytes of the file (editor::mind_loads::HashOf), 0 for GamesProgram
	uint64_t file {0};
	/// The script of a compile
	uint32_t script {0};

	[[nodiscard]] bool operator==(const EditorScriptProgram&) const = default;
};

/// A debug cheat of the player of the command goes on or off (debug_cheats::Cheat). Only a player with the editor claim sends
/// it in a network game, and the game server refuses it from every other player.
struct DebugCheat
{
	uint8_t cheat {0};
	bool on {false};

	[[nodiscard]] bool operator==(const DebugCheat&) const = default;
};

/// The debug cheat of the editor skips to a land of the story after the first (story_land_skip). Only a single player
/// game does it. The game server refuses it in every network game, because a network game has no lands of the story.
struct DebugLandSkip
{
	uint8_t land {0};

	[[nodiscard]] bool operator==(const DebugLandSkip&) const = default;
};

/// An act of the Advisors debug window on the advisors (debug_acts). Only a player with the debugger claim sends it in a
/// network game, and the game server refuses it from every other player. Each computer does the act at the turn.
struct DebugAdvisor
{
	enum class Act : uint8_t
	{
		/// The advisor flies out
		Out,
		/// The advisor appears where it is
		Appear,
		/// The advisor flies home
		Home,
		/// The advisor vanishes
		Vanish,
		/// The advisor flies to the place on the screen
		Fly,
		/// The advisor clings to the place on the screen
		Cling,
		/// The advisor points at the place on the screen
		ScreenPoint,
		/// The advisor points at the position in the world
		PointAt,
		/// The advisor looks at the position in the world
		LookAt,
		StopPointing,
		StopLooking,
		/// The advisor plays the animation at the place on the screen, at the speed
		PlayAnim,
		/// The advisor feels the emotion
		Feel,
	};
	Act act {Act::Out};
	/// The advisor: 1 the good one, 2 the evil one, or 0 for both
	uint8_t advisor {0};
	/// The place on the screen, as fractions across and down
	glm::vec2 place {0.0f};
	/// The position in the world
	glm::vec3 position {0.0f};
	/// When set, the advisor points out in the world, else near the screen
	bool inWorld {false};
	/// The animation (help::spirits::k_AnimSlots)
	uint8_t anim {0};
	/// The speed of the animation
	float speed {1.0f};
	/// The emotion
	uint8_t emotion {0};

	[[nodiscard]] bool operator==(const DebugAdvisor&) const = default;
};

/// A change of the World and the Bonus Creatures menus of the debug menu bar, or of the alignment slider of the Audio
/// debug window (debug_acts). Only a player with the debugger claim sends it in a network game, and the game server
/// refuses it from every other player. Each computer does the change at the turn.
struct DebugWorld
{
	enum class Act : uint8_t
	{
		/// The time of the day goes to the value, in hours
		TimeOfDay,
		/// The alignment of the player goes to the value, -1 to 1
		PlayerAlignment,
		/// The bonus creature of the index is unlocked when the value is 1, and locked when it is 0
		BonusCreature,
	};
	Act act {Act::TimeOfDay};
	/// The player of an alignment (PlayerNames)
	uint8_t player {0};
	/// The bonus creature (creature_swap::Bonus)
	uint8_t index {0};
	float value {0.0f};

	[[nodiscard]] bool operator==(const DebugWorld&) const = default;
};

/// The most bytes of a line of the debug Console that goes as a command
inline constexpr size_t k_MostConsoleLineBytes = 512;

/// A line of land script commands of the debug Console. Only a player with the debugger claim sends it in a network game,
/// and the game server refuses it from every other player. Each computer runs the line at the turn.
struct DebugConsoleLine
{
	std::string text;

	[[nodiscard]] bool operator==(const DebugConsoleLine&) const = default;
};

/// An act of the Running tab of the Magic debug window (debug_acts). Only a player with the debugger claim sends it in a
/// network game, and the game server refuses it from every other player. Each computer does the act at the turn, for the
/// player of the command.
struct DebugMagic
{
	enum class Act : uint8_t
	{
		/// The miracles go also outside the influence of their player when the value is on
		IgnoreInfluence,
		/// A dispenser of the miracle goes down at the position
		PlaceDispenser,
		/// A bubble of the miracle comes above the position
		PlaceBubble,
		/// The hand of the player gets the miracle
		GiveToHand,
		/// The worship site of the player makes the miracle for the hand
		SummonFromWorship,
		/// The player casts the miracle down on the position
		CastAtPoint,
		/// The player casts the miracle on the creature
		CastOnCreature,
		/// The hand of the player drops the miracle that it holds
		DropSeed,
		/// The prayer power of the player goes to the value
		PrayerPower,
		/// The prayer power of the player is infinite when the value is on
		InfinitePrayer,
	};
	Act act {Act::IgnoreInfluence};
	/// The miracle (MagicType)
	uint8_t magic {0};
	/// The position on the land that the window gives, before the window adds its height for a bubble or a cast
	glm::vec3 position {0.0f};
	/// The direction of the camera of the sender, for the effect of a cast
	glm::vec3 forward {0.0f, 0.0f, 1.0f};
	/// The creature of a cast on a creature
	entt::entity creature {entt::null};
	/// The prayer power
	float value {0.0f};
	bool on {false};

	[[nodiscard]] bool operator==(const DebugMagic&) const = default;
};

/// An act of the Town miracles tab of the Magic debug window (debug_acts): a town gets a miracle or loses it. Only a
/// player with the debugger claim sends it in a network game. Each computer does the act at the turn.
struct DebugTownMiracle
{
	/// The number of the town
	uint32_t town {0};
	/// The miracle (MagicType)
	uint8_t magic {0};
	/// The town gets the miracle, else it loses it
	bool give {true};
	/// The town also gets the plain miracle of a power-up level
	bool withPlain {true};
	/// The town loses the plain miracle and each power-up level
	bool allLevels {false};
	/// A town with no worship site can then have its site
	bool openSite {true};
	/// The first land can also have the site
	bool anyLand {true};

	[[nodiscard]] bool operator==(const DebugTownMiracle&) const = default;
};

/// An act of the Weather debug window (debug_acts). Only a player with the debugger claim sends it in a network game.
/// Each computer does the act at the turn.
struct DebugWeather
{
	enum class Act : uint8_t
	{
		/// Each storm clears and the climates stop the new storms. Then the storm comes, if it has rain or snow.
		Preset,
		/// The storm comes over the whole island
		ForceStorm,
		/// Each storm clears
		ClearStorms,
		/// Each storm with lightning flashes with a bolt on its next turn
		Bolt,
		/// Each storm with lightning flashes with thunder on its next turn
		Thunder,
		/// All the snow on the land melts at once
		MeltSnow,
		/// The climates work when the value is on
		Climates,
		/// The climates make new storms when the value is on
		ClimateStorms,
		/// The storm clears over its time to fade
		EndStorm,
	};
	Act act {Act::ClearStorms};
	/// What the storm brings: the temperature, the rain, the snow and the cloud in percent, and the wind
	int8_t temperature {0};
	int8_t rain {0};
	int8_t snow {0};
	int8_t overcast {0};
	int8_t windX {0};
	int8_t windZ {0};
	/// How long the storm lasts and how long it fades, in seconds
	float seconds {600.0f};
	float fadeSeconds {5.0f};
	/// The least and the most seconds between the flashes with thunder, and between the flashes with a bolt
	glm::vec2 thunderWait {0.0f};
	glm::vec2 boltWait {0.0f};
	/// How high the clouds are and how fast the rain falls. The climate gives them when they are 0.
	float cloudHeight {0.0f};
	float rainSpeed {0.0f};
	/// The storm of an end
	entt::entity storm {entt::null};
	bool on {false};

	[[nodiscard]] bool operator==(const DebugWeather&) const = default;
};

/// An act of the Moon debug window on the clock of the day and on the date of the moon (debug_acts). Only a player with
/// the debugger claim sends it in a network game. Each computer does the act at the turn.
struct DebugSky
{
	enum class Act : uint8_t
	{
		/// The script time goes to the hour
		ScriptHour,
		/// The clock of the day runs when the value is on
		ClockRuns,
		/// The moon takes its phase from the date
		MoonDate,
		/// The moon takes its phase from the date of the computer again
		ComputerDate,
		/// The moon takes its phase from the date of the computer again, and the clock runs
		Reset,
	};
	Act act {Act::ScriptHour};
	/// The script time, in hours
	float hour {0.0f};
	bool on {false};
	/// The date, in seconds after 1970
	int64_t date {0};

	[[nodiscard]] bool operator==(const DebugSky&) const = default;
};

/// A throw of the Physics debug window (debug_thing_acts). Only a player with the debugger claim sends it in a network game,
/// and the game server refuses it from every other player. Each computer does the throw at the turn.
struct DebugPhysics
{
	/// The thing to throw, or none (or a thing that is not in the world) for a new static thing
	entt::entity object {entt::null};
	/// The type of the new static thing (MobileStaticInfo)
	uint16_t type {0};
	/// The new static thing goes this high above the land
	float height {0.0f};
	float scale {1.0f};
	/// The place of the new static thing on the land, x and z. Each computer finds the height of the land at the turn.
	glm::vec2 place {0.0f};
	glm::vec3 velocity {0.0f};

	[[nodiscard]] bool operator==(const DebugPhysics&) const = default;
};

/// The most bytes of the name of a particle file in a command of the Particles tab
inline constexpr size_t k_MostParticleFileBytes = 64;

/// An act of the Particles tab of the Magic debug window on the effects with the shared random numbers (debug_thing_acts).
/// An effect with the random numbers of this computer shows only on this computer, and it goes as no command. Only a
/// player with the debugger claim sends it in a network game, and the game server refuses it from every other player.
struct DebugParticle
{
	enum class Act : uint8_t
	{
		/// A new effect with the shared random numbers, from the file or of the type
		Start,
		/// The effects stop or start their steps again
		Pause,
		/// Each effect closes down
		CloseAll,
		/// Each effect goes at once
		DeleteAll,
		/// The effect of the number closes down
		Close,
		/// The effect of the number goes at once
		Delete,
	};
	Act act {Act::Start};
	/// The name of the particle file, or empty for the type
	std::string file;
	/// The particle type (ParticleType)
	uint8_t type {0};
	glm::vec3 position {0.0f};
	float magnitude {1.0f};
	/// The player of the effect, 0 to 7
	uint8_t player {0};
	/// How the effect draws (particles::draw::DrawPath)
	uint8_t drawPath {0};
	/// The count of the nearest creatures and villagers that the effect gets to act on
	uint8_t targets {0};
	/// The number of the effect to close or delete. Only an effect with the shared random numbers has the same number on
	/// each computer.
	uint32_t effect {0};
	/// Pause: true stops the steps
	bool paused {false};

	[[nodiscard]] bool operator==(const DebugParticle&) const = default;
};

/// The most villagers that one command of the Temple villagers window takes away
inline constexpr size_t k_MostTakenVillagers = 64;

/// An act of the Temple villagers debug window (debug_thing_acts). Only a player with the debugger claim sends it in a
/// network game, and the game server refuses it from every other player. Each computer does the act at the turn.
struct DebugTempleVillager
{
	enum class Act : uint8_t
	{
		/// The town takes the share of its people at worship, in the value
		Share,
		/// The town sends one more of its people to worship
		SendOne,
		/// The town calls one of its people back from worship
		CallOneBack,
		/// New villagers of the town stand round its home
		Make,
		/// The villager goes to the worship site of its town
		SendToWorship,
		/// The villagers that the window made go from the world
		TakeAway,
	};
	Act act {Act::Share};
	entt::entity town {entt::null};
	/// The villager to send to worship
	entt::entity villager {entt::null};
	/// Make: the position of the home of the town
	glm::vec3 home {0.0f};
	/// Share: the share, 0 to 1
	float value {0.0f};
	/// Make: the tribe (Tribe), the job (VillagerNumber), the age in years and the count of the new villagers
	uint8_t tribe {0};
	uint8_t number {0};
	uint8_t age {1};
	uint8_t count {1};
	/// Make: the new villagers go to worship
	bool send {false};
	/// TakeAway: the villagers, at most k_MostTakenVillagers
	std::vector<entt::entity> villagers;

	[[nodiscard]] bool operator==(const DebugTempleVillager&) const = default;
};

/// An act of the Path finding debug window on a villager (debug_thing_acts). Only a player with the debugger claim sends it
/// in a network game, and the game server refuses it from every other player. Each computer does the act at the turn.
struct DebugVillagerPath
{
	enum class Act : uint8_t
	{
		/// The villager goes to the position at once
		Teleport,
		/// The villager walks in a line to the position
		MoveTo,
		/// The villager gets the goal of its walk
		Goal,
		/// The villager gets the speed of its walk, in metres a second
		Speed,
	};
	Act act {Act::Teleport};
	entt::entity villager {entt::null};
	glm::vec3 position {0.0f};
	glm::ivec2 goal {0, 0};
	float speed {0.0f};

	[[nodiscard]] bool operator==(const DebugVillagerPath&) const = default;
};

/// What a command does
using Action =
    std::variant<LeashKey, LeashTap, CreatureFeedback, HandTake, HandLetGo, HandTap, HandGive, LeashOrder, LeashPostTap,
                 HandReport, HandTwist, HandLock, HandUnlock, HandApply, TotemShare, CreatureBruise, FightGround, FightMove,
                 FightCharge, CreatureReaction, HandClick, SeedGesture, PlayerLeft, HandPick, PlayerJoined, MiracleRequest,
                 MiracleRepeat, HandCast, HandCastUnlock, LeashPick, HandShakeOff, GripDust, HelpReport, HelpQuery,
                 CreatureFightCheat, ViewReport, TempleLeashOff, GiveCallOff, CreatureTattoo, LeashLength, ChatSpeech,
                 ChatTaunt, QuickSave, QuickLoad, MusicMood, IconChargeStop, DialogueClick, PuzzleTap, FishScare,
                 ObserversJoined, ObserversLeft, ObserverIn, ObserverOut, HandLockRelease, EditorEdit, DebugCheat,
                 EditorCreatureEdit, EditorCreatureAct, DebugLandSkip, EditorVillagerEdit, EditorCreatureSpawn,
                 EditorLandStroke, EditorPath, EditorRemake, EditorMindLoad, EditorScript, EditorLandScript,
                 EditorScriptProgram, DebugAdvisor, DebugWorld, DebugConsoleLine, DebugMagic, DebugTownMiracle, DebugWeather,
                 DebugSky, DebugPhysics, DebugParticle, DebugTempleVillager, DebugVillagerPath>;

/// A command and the player that sent it
struct Command
{
	PlayerNames player;
	Action action;
	/// The observer of the god of the observers that sent the command, from 1. 0 for the human of a god and for the
	/// commands that the server makes for a god.
	uint8_t observer {0};
};

/// Whether an observer sends a command. An observer is a human of the god of the observers, which has no citadel and
/// so no influence: its hand does not act on things. Only the report of its hand and camera and the lines of the chat
/// go from an observer. The game server leaves out every other command of an observer.
[[nodiscard]] constexpr bool ObserverSends(const Action& action) noexcept
{
	return std::holds_alternative<HandReport>(action) || std::holds_alternative<ChatSpeech>(action) ||
	       std::holds_alternative<ChatTaunt>(action);
}

/// Whether only a player with the editor claim sends the command: the edits of the editor and the debug cheats. The game server
/// refuses these commands from a player whose join ticket has no editor claim.
[[nodiscard]] constexpr bool OnlyEditorSends(const Action& action) noexcept
{
	return std::holds_alternative<EditorEdit>(action) || std::holds_alternative<DebugCheat>(action) ||
	       std::holds_alternative<EditorCreatureEdit>(action) || std::holds_alternative<EditorCreatureAct>(action) ||
	       std::holds_alternative<EditorVillagerEdit>(action) || std::holds_alternative<EditorCreatureSpawn>(action) ||
	       std::holds_alternative<DebugLandSkip>(action) || std::holds_alternative<EditorLandStroke>(action) ||
	       std::holds_alternative<EditorPath>(action) || std::holds_alternative<EditorRemake>(action) ||
	       std::holds_alternative<EditorMindLoad>(action) || std::holds_alternative<EditorScript>(action) ||
	       std::holds_alternative<EditorLandScript>(action) || std::holds_alternative<EditorScriptProgram>(action);
}

/// Whether only a player with the debugger claim sends the command: the acts of the debug windows and of the menus of the
/// debug menu bar. The game server refuses these commands from a player whose join ticket has no debugger claim. An
/// observer does not send them.
[[nodiscard]] constexpr bool OnlyDebuggerSends(const Action& action) noexcept
{
	return std::holds_alternative<DebugAdvisor>(action) || std::holds_alternative<DebugWorld>(action) ||
	       std::holds_alternative<DebugConsoleLine>(action) || std::holds_alternative<DebugMagic>(action) ||
	       std::holds_alternative<DebugTownMiracle>(action) || std::holds_alternative<DebugWeather>(action) ||
	       std::holds_alternative<DebugSky>(action) || std::holds_alternative<DebugPhysics>(action) ||
	       std::holds_alternative<DebugParticle>(action) || std::holds_alternative<DebugTempleVillager>(action) ||
	       std::holds_alternative<DebugVillagerPath>(action);
}

/// Whether the game server refuses the command from every player, also from a player with the editor claim. A network game
/// has no lands of the story, so the server refuses the skip to a land of the story.
[[nodiscard]] constexpr bool ServerAlwaysRefuses(const Action& action) noexcept
{
	return std::holds_alternative<DebugLandSkip>(action);
}

/// Whether the game server takes the command from an observer. An observer with the editor claim also sends the edits of the
/// editor, also the edits, the acts, the new creatures and the tattoos of a creature, the edits of a villager, and the
/// strokes and the paths of the land tools, the mind files of a creature, and the acts of the script tabs. The god of the
/// observers has no citadel, so an observer sends no debug cheat.
[[nodiscard]] constexpr bool ServerTakesFromObserver(const Action& action, bool editor) noexcept
{
	return ObserverSends(action) ||
	       (editor &&
	        (std::holds_alternative<EditorEdit>(action) || std::holds_alternative<EditorCreatureEdit>(action) ||
	         std::holds_alternative<EditorCreatureAct>(action) || std::holds_alternative<EditorCreatureSpawn>(action) ||
	         std::holds_alternative<EditorVillagerEdit>(action) || std::holds_alternative<CreatureTattoo>(action) ||
	         std::holds_alternative<EditorLandStroke>(action) || std::holds_alternative<EditorPath>(action) ||
	         std::holds_alternative<EditorRemake>(action) || std::holds_alternative<EditorMindLoad>(action) ||
	         std::holds_alternative<EditorScript>(action) || std::holds_alternative<EditorLandScript>(action) ||
	         std::holds_alternative<EditorScriptProgram>(action)));
}

/// The commands that wait for a turn: those that went and wait for the start of the next turn, and those in the open
/// block, which go after the turns of this frame
struct WaitingCommands
{
	std::vector<Command> sent;
	std::vector<Command> open;

	[[nodiscard]] bool Empty() const noexcept { return sent.empty() && open.empty(); }
};

/// The commands that wait after the load of a saved game, from the commands that wait in the game before the load. As in
/// the original game, the load removes the blocks that went and wait for a turn. The open block stays and goes at once,
/// so that the first turn of the loaded world does its commands. A saved game keeps no command, so the load of a save made
/// in the temple keeps the leash on.
[[nodiscard]] WaitingCommands KeptByLoad(WaitingCommands before);

/// The commands that wait after a change of land in the same session, from the commands that wait before the change. As
/// in the original game, the change removes the blocks that went and wait for a turn. The open block stays open and goes
/// after the turns of the frame, so that the first turn of the new land does its commands.
[[nodiscard]] WaitingCommands KeptByLandChange(WaitingCommands before);

/// The commands that the players made and that the world did not see yet. The queue keeps no other game state, so that
/// the game and the tests use it both.
class CommandQueue
{
public:
	/// The most commands in one block. The block goes at once when it is full.
	static constexpr size_t k_BlockSize = 6;

	/// Put a command in the open block, with the number of the player that sends it. Send the block if it is full.
	void Submit(PlayerNames sender, Action action);
	/// Send the open block, if it has commands. The commands then wait for the start of the next turn.
	void Send();
	/// Take the commands of all the blocks that the queue sent, in the order that it sent them
	[[nodiscard]] std::vector<Command> TakeSent();
	/// Remove all the commands, those that wait and those in the open block
	void Clear();
	/// The commands that wait, in the order that they go
	[[nodiscard]] WaitingCommands Waiting() const;
	/// Put back the commands that waited, in place of all the commands of the queue
	void PutBack(WaitingCommands waiting);

	/// The number of commands in the open block
	[[nodiscard]] size_t OpenCount() const { return _open.size(); }
	/// The number of commands that the queue sent and that wait for the start of a turn
	[[nodiscard]] size_t SentCount() const;
	/// The number of blocks that the queue sent and that wait for the start of a turn
	[[nodiscard]] size_t SentBlocks() const { return _sent.size(); }

private:
	std::vector<Command> _open;
	std::vector<std::vector<Command>> _sent;
};

} // namespace openblack::player_commands
