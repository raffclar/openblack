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

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureTattoo.h"
#include "CreatureSpawnerModel.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "LeftClickCapture.h"
#include "Window.h"

namespace openblack::debug::gui
{

/// Spawns creatures for trying them out: pick the species, owner, alignment, physique and size, then left click on the
/// land to place one there, until placing is stopped. Lists the creatures on the land, to remove them again. The
/// selected creature can be commanded: while commanding, a left click on the land sends it walking or running there,
/// running away from there or turning to face it, picking up or knocking down what is there, throwing what it holds
/// there or pointing there, and its route is drawn over the land. At the top, the player's own creature: what it is,
/// how big it is and is drawn, how grown up it is, its leash, desires, plan, body and the hand on it, with buttons to
/// bring it to the hand, set its stage of growing up and change its leashes. Closed, it reads and writes nothing.
class CreatureSpawner final: public Window
{
public:
	CreatureSpawner() noexcept;

	void Close() noexcept override;

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;
	[[nodiscard]] bool TakesEvent(const SDL_Event& event) const noexcept override;

private:
	/// The player's own creature: what it is and does, and the buttons that act on it
	void DrawPlayersCreature() noexcept;
	void DrawSettings() noexcept;
	/// The creature picked in the list: its alignment, physique and size, changed as it stands
	void DrawSelected() noexcept;
	/// The picked creature's mind: what it is doing, wants and looks at, and buttons to make it do things
	void DrawMind(entt::entity entity) noexcept;
	/// What the mind has learnt: the planner's plans, opinions, decision trees, recent actions and thoughts, and the
	/// game's mind files to load into it
	void DrawLearning(entt::entity entity) noexcept;
	/// The picked creature's body: its needs as bars to set, the body's time to speed up, and buttons to make it see to
	/// a need
	void DrawBody(entt::entity entity) noexcept;
	/// The picked creature's tattoos and marks, to put on and take off
	void DrawAppearance(entt::entity entity) noexcept;
	/// The mute for every creature's sounds and the switch for other players' creatures' voices
	void DrawAudioSettings() noexcept;
	/// Whether the creatures' footprints are drawn, how many there are, and a switch to leave smiley faces
	void DrawFootprintSettings() noexcept;
	/// The switches for every creature: hair, sounds and footprints
	void DrawSharedSettings() noexcept;
	/// The picked creature's last sounds, and buttons to play each sound its animations make
	void DrawAudio(entt::entity entity) noexcept;
	/// The picked creature's leashes: which it knows, the one it wears and how taut it is, buttons to put leashes on,
	/// tie and untie them, and the rope drawn point by point over the scene
	void DrawLeash(entt::entity entity) noexcept;
	/// The picked creature's fights: starting one with another creature, both fighters' health, stamina, state and
	/// queue, orders as the player's clicks give them, fighting by itself, and knocking it out and bringing it round
	void DrawFight(entt::entity entity) noexcept;
	/// The rope's points over the scene, coloured by how far each segment is stretched
	void DrawRope(entt::entity entity) noexcept;
	void DrawPlacing() noexcept;
	/// The picked creature's movement: its speed and route, and the command mode
	void DrawMovement(entt::entity entity) noexcept;
	/// The picked creature's route, over the land
	void DrawRoute(entt::entity entity) noexcept;
	/// The picked creature's hands: what it holds and does with things, things to put by it, buttons to make it act on
	/// what it holds, and how the player's hand last treated it
	void DrawHands(entt::entity entity) noexcept;
	/// The mind new creatures are spawned with: fresh, or from one of the game's mind files
	void DrawSpawnMind() noexcept;
	/// The game's mind files, each with a button to load it into the selected creature or to spawn new creatures from it
	void DrawMindFiles(bool intoSelected) noexcept;
	void Command() noexcept;
	void DrawCreatures() noexcept;
	void Spawn() noexcept;
	/// Places a creature with the settings, and the mind and tattoos of the mind file it is to be spawned from, if any
	entt::entity SpawnAt(const glm::vec3& position) noexcept;
	/// Loads a mind file of the game's mind folder through the resource cache: into the selected creature, or as the
	/// mind new creatures are spawned with
	void UseMindFile(const std::filesystem::path& path, bool intoSelected) noexcept;
	/// Starts the body as a new creature of the species is: its size, fatness and strength
	void UseSpeciesDefaults() noexcept;

	/// A component to change, only when the entity has it, so that looking for it makes no storage
	template <typename Component>
	[[nodiscard]] static Component* Find(ecs::Registry& registry, entt::entity entity)
	{
		return std::as_const(registry).AllOf<Component>(entity) ? &registry.Get<Component>(entity) : nullptr;
	}

	CreatureType _species {CreatureType::Tiger};
	PlayerNames _owner {PlayerNames::PLAYER_ONE};
	float _alignment {0.0f};
	float _fatness {0.5f};
	float _strength {0.5f};
	float _scale {1.0f};
	/// The species the body was last started for, once the game's creature tables are loaded
	std::optional<CreatureType> _defaultsFor;
	float _facingDegrees {180.0f};
	bool _randomFacing {false};

	bool _placing {false};
	/// What a left click on the land tells the selected creature to do while commanding
	enum class Order : uint8_t
	{
		Walk,
		Run,
		Flee,
		Face,
		PickUp,
		Throw,
		Destroy,
		Point,
	};
	bool _commanding {false};
	Order _order {Order::Walk};
	/// A left click on the land was taken, to place or command at the next update
	bool _clicked {false};
	/// The left press the window took, whose release it takes too
	debug::LeftClickCapture _leftCapture;
	bool _showRoute {true};
	bool _showRope {false};
	/// What became of the last order
	std::string _lastOrder;
	/// What became of the last need it was told to see to
	std::string _lastNeed;
	/// What became of the last button of the player's creature section
	std::string _lastPlayers;
	/// The window's own random picks, never the game's
	creature_spawner::RandomEngine _random {creature_spawner::k_RandomSeed};
	std::optional<entt::entity> _selected;
	/// The creature the selected tab was last brought to the front for
	std::optional<entt::entity> _tabFor;
	/// The thing to put by the creature, what it does to what it holds, and what became of the last thing it was told to
	/// do with its hands
	int _objectType {0};
	int _keepAction {3};
	std::string _lastHands;
	/// The action and gesture picked to play on the selected creature
	size_t _action {0};
	size_t _gesture {0};
	/// The tattoo being edited: its slot, design, site and colour from the palette at a brightness
	int _tattooSlot {0};
	int _tattooDesign {0};
	int _tattooSite {0};
	int _paletteColumn {0};
	int _paletteRow {64};
	float _tattooBrightness {0.5f};
	/// Where the next wound, burn or blood goes, unless at random, and the wound's kind and column of the damage atlas
	bool _randomMarkPlace {true};
	int _markU {128};
	int _markV {128};
	int _markSkin {0};
	int _woundType {3};
	int _woundColumn {3};
	/// The creature to fight, by its place among the others nearest first, how long blows ordered here are charged,
	/// whether a fight started here is fought by itself, and what became of the last fight started
	int _fightOpponent {0};
	float _fightChargeMs {0.0f};
	bool _fightAuto {true};
	std::string _lastFight;
	/// The game's mind files, as last listed, and what became of the last one loaded or spawned from
	std::vector<std::filesystem::path> _mindFiles;
	bool _mindFilesListed {false};
	std::string _lastMindFile;
	/// The mind new creatures are spawned with, from the resource cache (0 for a fresh mind), its name, and the
	/// tattoos it gives them
	entt::id_type _spawnMind {0};
	std::string _spawnMindName;
	std::optional<creature_tattoo::Slots> _spawnTattoos;
	/// The skill, miracle and player's deed picked to show the selected creature
	int _skill {0};
	int _miracle {0};
	int _deed {0};
	/// The stage of growing up the player's creature section sets
	int _phase {0};
};

} // namespace openblack::debug::gui
