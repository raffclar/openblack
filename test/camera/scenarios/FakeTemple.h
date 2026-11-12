/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>
#include <cstdlib>

#include <chrono>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <3D/OrientedText.h>
#include <3D/TempleInteriorInterface.h>
#include <3D/TempleSubMeshTexture.h>
#include <Temple/TemplePeople.h>

/// A temple with no rooms. The test tells if the player is inside and gives the camera of the world behind the temple.
/// A getter of the rooms' data stops the test.
class FakeTemple final: public openblack::TempleInteriorInterface
{
public:
	using TempleRoom = openblack::TempleRoom;
	using TempleDoors = openblack::TempleDoors;
	using OutsideCamera = openblack::OutsideCamera;
	using Camera = openblack::Camera;
	using TempleCursorHit = openblack::TempleCursorHit;
	using TempleScreenInterface = openblack::TempleScreenInterface;
	using TempleSubMeshTexture = openblack::TempleSubMeshTexture;
	using TempleSubMeshGlow = openblack::TempleSubMeshGlow;
	using TempleSubMeshOffset = openblack::TempleSubMeshOffset;
	using OrientedTextVertex = openblack::OrientedTextVertex;
	using TempleMapMarker = openblack::TempleMapMarker;
	using TempleMapSpellSeed = openblack::TempleMapSpellSeed;
	using TempleMapInfluence = openblack::TempleMapInfluence;
	using TempleLight = openblack::TempleLight;
	using TempleFog = openblack::TempleFog;
	using TempleCaveTrophy = openblack::TempleCaveTrophy;
	using TempleCaveSeed = openblack::TempleCaveSeed;
	using TemplePerson = openblack::TemplePerson;
	using TemplePeople = openblack::TemplePeople;

	[[nodiscard]] bool Active() const override { return active; }
	[[nodiscard]] std::optional<OutsideCamera> GetOutsideCamera() const override { return {}; }
	[[nodiscard]] Camera* GetWorldCamera() override { return worldCamera; }
	void ProcessToolTipTurn() override {}
	[[nodiscard]] glm::vec3 GetPosition() const override { return {}; }
	[[nodiscard]] TempleRoom GetCurrentRoom() const override { return {}; }
	void SetCurrentRoom(TempleRoom /*room*/) override {}
	[[nodiscard]] std::optional<TempleRoom> GetTransitionRoom() const override { return {}; }
	void SetTransitionRoom(std::optional<TempleRoom> /*room*/) override {}
	void GoToRoom(TempleRoom /*room*/) override {}
	void EnterRoom(TempleRoom /*room*/) override {}
	void LookAtScroll(size_t /*place*/) override {}
	void LookAtSaveGamePicture(uint32_t /*slot*/) override {}
	void ZoomToCaveTarget(size_t /*target*/) override {}
	[[nodiscard]] bool IsRoomAvailable(TempleRoom /*room*/) const override { return {}; }
	void TellRoomNotAvailable() override {}
	void AskNewGame() override {}
	[[nodiscard]] bool IsRoomDrawn(TempleRoom /*room*/) const override { return {}; }
	[[nodiscard]] std::optional<float> GetWayOutSeconds() const override { return {}; }
	[[nodiscard]] TempleDoors& GetDoors() override { std::abort(); }
	[[nodiscard]] const TempleDoors& GetDoors() const override { std::abort(); }
	[[nodiscard]] std::optional<TempleCursorHit> GetCursorHit() const override { return {}; }
	[[nodiscard]] std::optional<openblack::temple_press::PressTrace> GetLastCameraPress() const override { return {}; }
	void SetInterface(TempleScreenInterface* /*interface*/) override {}
	void SetDrawMakers(DrawMakers /*makers*/) override {}
	bool HoldControl(bool /*pressed*/, float /*mouseY*/) override { return {}; }
	[[nodiscard]] std::vector<TempleSubMeshTexture> GetScrollTextures(TempleRoom /*room*/) const override { return {}; }
	[[nodiscard]] std::vector<uint32_t> GetHiddenSubMeshes(TempleRoom /*room*/) const override { return {}; }
	[[nodiscard]] std::span<const uint32_t> GetControlSubMeshes(TempleRoom /*room*/) const override { return {}; }
	[[nodiscard]] std::vector<TempleSubMeshGlow> GetControlGlows(TempleRoom /*room*/) const override { return {}; }
	[[nodiscard]] std::vector<TempleSubMeshOffset> GetControlOffsets(TempleRoom /*room*/) const override { return {}; }
	[[nodiscard]] const std::vector<OrientedTextVertex>& GetText() const override { std::abort(); }
	[[nodiscard]] const std::vector<openblack::temple::scroll_pictures::WorldPicture>& GetScrollPictures() const override
	{
		std::abort();
	}
	[[nodiscard]] const openblack::graphics::Texture2D* GetTextTexture() const override { return nullptr; }
	[[nodiscard]] const std::vector<OrientedTextVertex>& GetMap() const override { std::abort(); }
	[[nodiscard]] uint32_t GetVisits() const override { return {}; }
	[[nodiscard]] const std::vector<TempleMapMarker>& GetMapMarkers() const override { std::abort(); }
	[[nodiscard]] const std::vector<TempleMapSpellSeed>& GetMapSpellSeeds() const override { std::abort(); }
	[[nodiscard]] std::optional<TempleMapInfluence> GetMapInfluence() const override { return {}; }
	[[nodiscard]] const TempleLight& GetLight() const override { std::abort(); }
	[[nodiscard]] const TempleFog& GetFog() const override { std::abort(); }
	[[nodiscard]] const std::vector<TempleCaveTrophy>& GetCaveTrophies() const override { std::abort(); }
	[[nodiscard]] const std::vector<TempleCaveSeed>& GetCaveSeeds() const override { std::abort(); }
	[[nodiscard]] const std::vector<TemplePerson>& GetPeople() const override { std::abort(); }
	[[nodiscard]] size_t GetPeopleInside() const override { return {}; }
	[[nodiscard]] const TemplePeople* GetWalkingPeople() const override { return nullptr; }
	[[nodiscard]] openblack::ecs::Registry& GetRooms() override { std::abort(); }
	[[nodiscard]] const openblack::ecs::Registry& GetRooms() const override { std::abort(); }
	[[nodiscard]] glm::mat4 GetPlacement() const override { return {}; }
	[[nodiscard]] float GetMapMarkerTurn() const override { return {}; }
	[[nodiscard]] float GetPoolTime() const override { return {}; }
	void Escape() override {}
	bool ReleaseOutsideScriptCamera() override { return {}; }
	void RequestLeave() override {}
	void PickMapPlace(glm::vec2 /*place*/) override {}
	void FadeToWhite() override {}
	void TurnBackRisingFade() override {}
	void CoverSkippedFlight() override {}
	[[nodiscard]] float GetFadeAmount() const override { return {}; }
	void Update(std::chrono::microseconds /*dt*/) override {}
	[[nodiscard]] glm::vec2 GetWaterfallSlide() const override { return {}; }
	void Activate() override {}
	void Activate(TempleRoom /*room*/) override {}
	void ActivateAfterFrame(TempleRoom /*room*/) override {}
	void TapIntoTemple(TempleRoom /*room*/) override {}
	void FrameDrawn() override {}
	[[nodiscard]] bool GoesInAfterFrame() const override { return {}; }
	void NoteFrameWrite(openblack::temple::FrameWrite /*write*/) override {}
	[[nodiscard]] openblack::temple::FrameWrites TakeFrameWrites() override { return {}; }
	void EnterLockedRoom(TempleRoom /*room*/) override {}
	bool TakeGameEnded() override { return {}; }
	void Deactivate() override {}
	void CloseForNewLand() override {}
	void EnterMultiplayerFrontEnd() override {}
	void LeaveMultiplayerFrontEnd() override {}
	[[nodiscard]] bool IsMultiplayerFrontEnd() const override { return {}; }
	void ShowHelpAfterLoad() override {}
	void WarmUpVisit(const std::function<void()>& /*draw*/) override {}

	bool active {false};
	Camera* worldCamera {nullptr};
};
