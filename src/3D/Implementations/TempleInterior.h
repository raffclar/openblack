/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <SaveSlots.h>
#include <glm/vec3.hpp>

#include "3D/CreatureCaveTrophies.h"
#include "3D/OrientedText.h"
#include "3D/TempleDoors.h"
#include "3D/TempleInteriorInterface.h"
#include "3D/TempleMap.h"
#include "3D/TempleMapSpells.h"
#include "3D/TempleToggles.h"
#include "3D/TempleToolTips.h"
#include "Common/FramePace.h"
#include "Temple/FutureRoomRules.h"
#include "Temple/SaveGameRoomRules.h"
#include "Temple/TempleHelp.h"
#include "Temple/TemplePeople.h"
#include "Temple/TempleRoomLock.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack
{
class CameraModel;
class CreatureCaveEffects;
class TempleCameraModel;
class TempleChallengeRoom;
class TempleSaveGameRoom;
class TempleScrolls;
class TempleSigns;

class TempleInterior final: public TempleInteriorInterface
{
public:
	TempleInterior();
	~TempleInterior() override;

	[[nodiscard]] bool Active() const override { return _active; }
	[[nodiscard]] glm::vec3 GetPosition() const override { return _templePosition; }
	[[nodiscard]] TempleRoom GetCurrentRoom() const override { return _currentRoom; }
	void SetCurrentRoom(TempleRoom room) override;
	bool ReleaseOutsideScriptCamera() override;
	[[nodiscard]] std::optional<OutsideCamera> GetOutsideCamera() const override;
	[[nodiscard]] std::optional<TempleRoom> GetTransitionRoom() const override { return _transitionRoom; }
	void SetTransitionRoom(std::optional<TempleRoom> room) override { _transitionRoom = room; }
	void GoToRoom(TempleRoom room) override;
	void EnterRoom(TempleRoom room) override;
	void LookAtScroll(size_t place) override;
	[[nodiscard]] bool IsRoomDrawn(TempleRoom room) const override;
	[[nodiscard]] std::optional<float> GetWayOutSeconds() const override;
	[[nodiscard]] bool IsRoomAvailable(TempleRoom room) const override;
	void TellRoomNotAvailable() override;
	[[nodiscard]] TempleDoors& GetDoors() override { return _doors; }
	[[nodiscard]] const TempleDoors& GetDoors() const override { return _doors; }
	[[nodiscard]] std::optional<TempleCursorHit> GetCursorHit() const override;
	void SetInterface(gui::GameInterface* interface) override;
	bool HoldControl(bool pressed, float mouseY) override;
	[[nodiscard]] std::vector<TempleSubMeshTexture> GetScrollTextures(TempleRoom room) const override;
	[[nodiscard]] std::vector<uint32_t> GetHiddenSubMeshes(TempleRoom room) const override;
	[[nodiscard]] std::span<const uint32_t> GetControlSubMeshes(TempleRoom room) const override;
	[[nodiscard]] const std::vector<OrientedTextVertex>& GetText() const override { return _text; }
	[[nodiscard]] const std::vector<temple::scroll_pictures::WorldPicture>& GetScrollPictures() const override
	{
		return _scrollPictures;
	}
	[[nodiscard]] std::vector<TempleSubMeshGlow> GetControlGlows(TempleRoom room) const override;
	[[nodiscard]] std::vector<TempleSubMeshOffset> GetControlOffsets(TempleRoom room) const override;
	[[nodiscard]] const graphics::Texture2D* GetTextTexture() const override;
	[[nodiscard]] const std::vector<OrientedTextVertex>& GetMap() const override { return _mapTriangles; }
	[[nodiscard]] uint32_t GetVisits() const override { return _visits; }
	[[nodiscard]] const std::vector<TempleMapMarker>& GetMapMarkers() const override { return _mapMarkers; }
	[[nodiscard]] const std::vector<TempleCaveTrophy>& GetCaveTrophies() const override { return _caveTrophies; }
	[[nodiscard]] const std::vector<TempleCaveSeed>& GetCaveSeeds() const override { return _caveSeeds; }
	[[nodiscard]] const TempleLight& GetLight() const override { return _light; }
	[[nodiscard]] const TempleFog& GetFog() const override { return _fog; }
	[[nodiscard]] const std::vector<TemplePerson>& GetPeople() const override;
	[[nodiscard]] size_t GetPeopleInside() const override { return _people != nullptr ? _people->GetCount() : 0; }
	[[nodiscard]] const TemplePeople* GetWalkingPeople() const override { return _people.get(); }
	[[nodiscard]] glm::mat4 GetPlacement() const override;
	[[nodiscard]] float GetMapMarkerTurn() const override { return _mapMarkerTurn; }
	[[nodiscard]] const std::vector<TempleMapSpellSeed>& GetMapSpellSeeds() const override { return _mapSpells.GetSeeds(); }
	[[nodiscard]] std::optional<TempleMapInfluence> GetMapInfluence() const override { return _mapInfluence; }
	[[nodiscard]] float GetPoolTime() const override { return _poolTime; }
	void Escape() override;
	void RequestLeave() override { _leaveRequested = true; }
	void FadeToWhite() override;
	void CoverSkippedFlight() override;
	[[nodiscard]] float GetFadeAmount() const override;
	void Update(std::chrono::microseconds dt) override;
	[[nodiscard]] glm::vec2 GetWaterfallSlide() const override;
	void Activate() override { Activate(TempleRoom::Main); }
	void Activate(TempleRoom room) override;
	void ActivateAfterFrame(TempleRoom room) override;
	void FrameDrawn() override;
	[[nodiscard]] bool GoesInAfterFrame() const override { return _activateAfterFrame.has_value(); }
	void EnterLockedRoom(TempleRoom room) override;
	[[nodiscard]] bool TakeGameEnded() override { return _lock.TakeEnded(); }
	void Deactivate() override;
	void CloseForNewLand() override;
	void EnterMultiplayerFrontEnd() override;
	void LeaveMultiplayerFrontEnd() override;
	[[nodiscard]] bool IsMultiplayerFrontEnd() const override { return _multiplayerFrontEnd; }
	void ShowHelpAfterLoad() override { BringBackDialogue(); }
	void WarmUpVisit(const std::function<void()>& draw) override;
	/// A save keeps the dialogue that the temple keeps for the world while the player is inside: its owner, its last six
	/// texts and the click waits of the newest
	void SaveState(ecs::reflection::BinaryWriter& out) const override;
	/// A load gives the temple the kept dialogue back, to give back to the world in ShowHelpAfterLoad
	bool LoadState(ecs::reflection::BinaryReader& in) override;
	[[nodiscard]] ecs::Registry& GetRooms() override { return *_rooms; }
	[[nodiscard]] const ecs::Registry& GetRooms() const override { return *_rooms; }

private:
	/// Takes down what the temple made for the visit and gives the camera and drawing back to the island
	void Close();
	/// Looks through the camera model's lens, or the configured one without
	void ApplyLens() const;
	/// The temple gives back the dialogue that it kept when the player entered
	void BringBackDialogue();

	bool _active {false};
	/// True while a warm-up draws a visit, in which the temple draws all of its rooms
	bool _warmUpVisit {false};
	/// The things of the rooms, made as the player goes in and taken down as the player leaves
	std::unique_ptr<ecs::Registry> _rooms;
	/// The room that the player goes into after the frame that is drawn now
	std::optional<TempleRoom> _activateAfterFrame;
	/// The room the player last came into on this visit, whose help was asked for then
	std::optional<TempleRoom> _enteredRoom;
	bool _leaveRequested {false};
	/// The room the player is kept in once the game is over
	temple::RoomLock _lock;
	/// True after the temple drew the main room as the player's room in this game. This opens the other rooms. The
	/// game keeps this value in its saves.
	bool _roomsOpened {false};
	/// The world's dialogue, kept while the player is inside
	std::optional<temple_help::HeldDialogue> _heldDialogue;
	/// The temple starts in the main room unless told of another
	TempleRoom _currentRoom {TempleRoom::Main};
	std::optional<TempleRoom> _transitionRoom;
	/// The game places each room at the origin, unturned: the temple is a scene of its own
	glm::vec3 _templePosition {0.0f};
	glm::vec3 _templeRotation {0.0f};
	glm::vec3 _playerPositionOutside {0.0f};
	glm::vec3 _playerRotationOutside {0.0f};
	/// The camera's model outside, while the temple's has the camera
	std::unique_ptr<CameraModel> _outsideCameraModel;
	TempleCameraModel* _cameraModel {nullptr};
	TempleDoors _doors;
	/// The main room's buttons of what its map shows, which keep their state from one visit to the next
	TempleToggles _toggles;
	/// The island over the main room's pool, and its triangles this frame
	TempleMap _map;
	std::vector<OrientedTextVertex> _mapTriangles;
	/// The markers on the map this frame, and how far they have turned
	std::vector<TempleMapMarker> _mapMarkers;
	/// The seeds of the miracles that the players cast, over the map
	TempleMapSpells _mapSpells;
	/// How the main room shows the influence of the players this frame, if it does
	std::optional<TempleMapInfluence> _mapInfluence;
	/// The seeds of the miracles over the map, by the game's time of the frame
	void UpdateMapSpells();
	/// Removes the seeds of the miracles from the map, with their effects
	void ClearMapSpells();
	std::vector<TempleCaveTrophy> _caveTrophies;
	TempleLight _light;
	TempleFog _fog;
	/// The creature's room's belts for how the creature fights, the medals for its miracles and the seeds of its best
	/// learnt, at the points of the room's mesh, while the room is drawn. The seeds turn and pulse by `gameSeconds`.
	void UpdateCaveTrophies(float gameSeconds);
	/// The seeds go, with their effects, as the player leaves the temple
	void ClearCaveSeeds();
	/// How the magic scroll last worked out the creature's miracles, which the medals and seeds show; none until the
	/// room is first drawn on a visit
	std::optional<CreatureCaveTrophies::MiracleLearning> _caveLearning;
	/// Whether the magic scroll's text was in front of it last frame
	bool _magicScrollWritten {false};
	/// Each place by the plinths, with its seed once made and its holder's effect
	struct CaveSeedPlace
	{
		TempleCaveSeed seed;
		uint32_t holderEffect;
	};
	std::array<std::optional<CaveSeedPlace>, CreatureCaveTrophies::k_BestMiracles> _caveSeedPlaces;
	/// The seeds drawn this frame
	std::vector<TempleCaveSeed> _caveSeeds;
	/// When the effects of the seeds in the creature's room and over the map take their next step, and by how many
	/// seconds they step in this frame (none in a frame where they do not step)
	frame_pace::Pace _caveEffectPace;
	frame_pace::Pace _mapEffectPace;
	std::optional<float> _caveEffectStep;
	std::optional<float> _mapEffectStep;
	float _mapMarkerTurn {0.0f};
	/// How long the main room's pool has shimmered, in seconds
	float _poolTime {0.0f};
	/// The main room's markers of the temples and creatures
	void UpdateMapMarkers(float seconds);
	/// Where the camera is to look from and at as the player leaves for a place double clicked on the map
	std::optional<std::pair<glm::vec3, glm::vec3>> _leaveTo;
	/// Whether the temple is fading to white to leave for the place on the map, which it does once the screen is white
	bool _leavingForMapPoint {false};
	/// A double click on the main room's map: leaves the temple for that place
	void LeaveForMapPoint(glm::vec3 point);
	/// Covers the screen as the player is put straight into a room, and fades it back over a second and a fifth
	void FadeIntoRoom();
	uint32_t _visits {0};
	/// How far through its slide the waterfall's texture is, from 0 to 1
	float _waterfallSlide {0.0f};
	/// Whether the creature's room's sounds of its water and fire are playing
	bool _soundsOfCreatureCave {false};
	/// The flames, smoke, spray and mist of the creature's room
	std::unique_ptr<CreatureCaveEffects> _creatureCaveEffects;
	/// The player's worshippers walking about the temple, who come in with the player and go as the player leaves
	std::unique_ptr<TemplePeople> _people;
	/// The game's interface, whose text and font the scrolls and signs are written with
	gui::GameInterface* _interface {nullptr};
	/// The rooms' scrolls, written as the temple opens, and the labels of their signs
	std::unique_ptr<TempleScrolls> _scrolls;
	std::unique_ptr<TempleSigns> _signs;
	/// As each room is set up: makes and writes the scrolls, and finds the signs and the challenge room's pictures
	void CreateScrolls();
	/// The challenge room's pictures, frames and buttons
	std::unique_ptr<TempleChallengeRoom> _challengeRoom;
	/// The challenge whose reminder the player is leaving the temple to replay, once the screen is white
	std::optional<entt::entity> _replay;
	/// Fades to white with a woosh to leave and replay a challenge
	void ReplayChallenge(entt::entity record);
	/// Leaves the temple, the camera put back where the challenge's picture was taken, and starts its reminder
	void LeaveToReplayChallenge();
	/// Moves on the challenge room's pictures and its close look's words
	void UpdateChallengeRoom();
	/// The save game room's pictures and buttons
	std::unique_ptr<TempleSaveGameRoom> _saveGameRoom;
	/// The question the save game room asks the player about a slot, until it is answered
	struct SaveGameQuestion
	{
		temple::save_game_room::Click click;
		uint32_t slot;
	};
	std::optional<SaveGameQuestion> _saveGameQuestion;
	/// What the save game room's controls depend on now
	[[nodiscard]] temple::save_game_room::View SaveGameRoomView() const;
	/// Reads the saved games that the save game room lists again
	void ListSavedGames();
	/// Asks the player about a slot, as a click in the save game room asks
	void AskAboutSlot(temple::save_game_room::Click click, uint32_t slot);
	/// Acts on the player's answer to the save game room's question
	void UpdateSaveGameRoom();
	/// Saves the game into the slot with the name, as the save game room saves
	void SaveIntoSlot(uint32_t slot, std::u16string_view name, gamesave::RoomSave why);
	/// This frame's text in the rooms
	std::vector<OrientedTextVertex> _text;
	std::vector<temple::scroll_pictures::WorldPicture> _scrollPictures;
	/// Whether the options room has opened the game's options
	bool _optionsShown {false};
	/// The multiplayer front end is up over the future room
	bool _multiplayerFrontEnd {false};
	/// The submeshes of each room that are its controls, found as the rooms are set up
	std::unordered_map<TempleRoom, std::vector<uint32_t>> _controlSubMeshes;
	/// The future room's words in this visit, which fade in
	temple::future_room::Words _futureWords;
	/// The submesh the cursor is over, and how long is left of the glow it set off, in milliseconds. Moving onto
	/// another submesh sets the glow off again, unless a control is being dragged.
	std::optional<std::pair<TempleRoom, uint32_t>> _hovered;
	float _hoverGlow {0.0f};
	/// What the hand shows, which the rooms choose every frame and submit every turn, and how long since the last turn
	TempleToolTip _toolTip {k_FirstTempleToolTip};
	float _toolTipTurnTime {0.0f};
	/// Moves on the options room and the future room, and what the future room draws
	void UpdateOptionsAndFutureRooms(float milliseconds);
	/// The tooltip the hand shows, and where on the screen the hand is
	void UpdateToolTips(float milliseconds);
	void StopCreatureCaveSounds();
	/// Whether a submesh of a room's mesh is one of its controls drawn, and whether a control has the mouse
	[[nodiscard]] bool IsControl(TempleRoom room, uint32_t subMesh) const;
	[[nodiscard]] bool IsControlHeld() const;
};
} // namespace openblack
