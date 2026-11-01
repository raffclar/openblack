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

#include <algorithm>
#include <chrono>
#include <deque>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <entt/signal/sigh.hpp>

#include "3D/WaterRings.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ParticleObjectEffects.h"
#include "Particles/ParticleClassRegistry.h"
#include "Particles/ParticleCreators.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleMaths.h"
#include "Particles/ParticleSoundRelease.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::particles
{
class EffectWriter;
class EffectReader;
struct SharedTables;
} // namespace openblack::particles

namespace openblack::ecs::systems
{

/// The particle effects' world: the land's height, the players' colours, the camera and the objects miracles act on
class GameParticleWorld final: public particles::ParticleWorldInterface
{
public:
	[[nodiscard]] float LandHeight(glm::vec2 xz) const override;
	[[nodiscard]] float LandHeightAtMap(int32_t x, int32_t z) const override;
	[[nodiscard]] uint32_t PlayerColour(int player) const override;
	[[nodiscard]] glm::vec3 CameraRight() const override;
	[[nodiscard]] glm::vec3 CameraUp() const override;
	[[nodiscard]] glm::vec3 CameraPosition() const override;
	void PlayListenerSound(uint32_t inGameSample, bool playsOnce) override;
	[[nodiscard]] std::optional<TargetInfo> Target(entt::entity target, bool centre) const override;
	[[nodiscard]] bool IsTargetHeld(entt::entity target) const override;
	[[nodiscard]] bool IsTargetClaimed(entt::entity target) const override { return _claimed.contains(target); }
	void ClaimTarget(entt::entity target, bool claimed) override;
	void SetTargetGlow(entt::entity target, glm::u8vec3 rgb) override;
	void StartSound(const particles::Effect& effect, const std::shared_ptr<particles::ParticleSoundLink>& sound) override;
	[[nodiscard]] bool IsWater(glm::vec3 point) const override;
	[[nodiscard]] int SurfaceAt(glm::vec3 point) const override;
	[[nodiscard]] int SoundAlignment(int player) const override;
	[[nodiscard]] glm::vec3 LandNormal(glm::vec2 xz) const override;
	/// The game's sound actions by name, read from the data folder the first time they are needed
	[[nodiscard]] const std::map<std::string, int32_t, std::less<>>& SoundActions() const;
	[[nodiscard]] bool IsRainingAt(glm::vec3 point) const override;
	[[nodiscard]] glm::vec3 WindAt(glm::vec3 point) const override;
	[[nodiscard]] glm::vec3 SmoothWindAt(glm::vec3 point) const override;
	[[nodiscard]] uint32_t AddRainStorm(const particles::storm::RainStorm& storm) override;
	bool MoveRainStorm(uint32_t storm, glm::vec3 centre) override;
	[[nodiscard]] bool IsDryLand(glm::vec3 point) const override;
	void RemoveRainStorm(uint32_t storm) override;
	[[nodiscard]] bool VillagerInCell(glm::vec3 point) const override;
	[[nodiscard]] std::vector<particles::StrikeCandidate> StrikeCandidates(glm::vec3 centre, size_t cells) const override;
	[[nodiscard]] bool LandBlocks(glm::vec3 from, glm::vec3 to) const override;
	void QueueArcs(entt::entity object) override;
	void StrikeWithoutMiracle(glm::vec3 point) override;
	[[nodiscard]] std::optional<particles::BeliefSprite> TakeBeliefSprite() override;
	[[nodiscard]] std::optional<particles::TownCentreBelief> TownCentreBeliefOf(entt::entity centre) const override;
	[[nodiscard]] std::vector<int> PlayersInGame() const override;
	/// A symbol of belief waits to rise; whether there was room
	bool QueueBeliefSprite(const particles::BeliefSprite& sprite);
	[[nodiscard]] std::optional<entt::entity> TakeArcs() override;
	[[nodiscard]] std::shared_ptr<const void> HoldArc() const override;
	[[nodiscard]] std::vector<particles::SurfacePoint>
	SurfacePoints(entt::entity object, size_t count, const std::function<int32_t(int32_t)>& random) const override;
	void AddBolt(const std::shared_ptr<particles::BoltShare>& bolt) override;
	[[nodiscard]] std::vector<std::shared_ptr<particles::BoltShare>> Bolts() const override;
	[[nodiscard]] uint64_t NextBoltOrder() override { return ++_boltOrder; }
	/// Whether an object has been struck for arcs since last asked, so that the effect the arcs crawl in is to run
	[[nodiscard]] bool TakeArcsWanted();
	[[nodiscard]] std::shared_ptr<particles::GestureTrail> TakeGestureTrail() override;
	void AddLightSheet(const std::shared_ptr<particles::LightSheet>& sheet) override;
	/// A sheet of light that a system or an entity keeps, such as the ring of an arena. A save of the particles does not
	/// keep it: its owner keeps it and makes it again after a load. A sheet that is in the list already is not added
	/// again.
	void AddOwnedSheet(const std::shared_ptr<particles::LightSheet>& sheet);
	void SetHandGlow(uint32_t rgb) override;
	void QueueGestureTrail(std::shared_ptr<particles::GestureTrail> trail);
	/// The sheets of light still standing; those gone are forgotten
	[[nodiscard]] std::vector<std::shared_ptr<particles::LightSheet>> LightSheets() const;
	void AddShield(const std::shared_ptr<particles::ShieldSphere>& shield) override;
	[[nodiscard]] std::shared_ptr<particles::ShieldSphere> FindShield(glm::vec3 point, float margin) const override;
	[[nodiscard]] std::shared_ptr<particles::ShieldSphere> FindShieldCrossing(glm::vec3 before, glm::vec3 point,
	                                                                          float margin) const override;
	[[nodiscard]] std::shared_ptr<particles::ShieldSphere> ShieldOf(const particles::Effect& effect) const override;
	// What a tornado asks of the world, through the tornado system
	[[nodiscard]] uint32_t GameTurn() const override;
	[[nodiscard]] glm::u8vec3 LandColour(glm::vec2 xz) const override;
	[[nodiscard]] std::vector<particles::TornadoCandidate> TornadoCandidates(glm::vec3 foot, float reach) const override;
	void CatchCreature(entt::entity creature) override;
	entt::entity TakeFromPile(entt::entity pile, uint32_t amount, float sizeShare) override;
	std::optional<glm::mat3> Carry(const std::shared_ptr<particles::CarriedObject>& carried) override;
	std::optional<VortexEntry> EnterVortex(const std::shared_ptr<particles::CarriedObject>& carried) override;
	void ThrowOutOfVortex(entt::entity object, float angle, float scale) override;
	[[nodiscard]] std::vector<glm::vec3> TargetExtraPoints(entt::entity target) const override;
	[[nodiscard]] std::optional<glm::vec3> ObjectPosition(entt::entity object) const override;
	[[nodiscard]] uint64_t GameMilliseconds() const override;
	[[nodiscard]] SurfacePoint RandomSurfacePoint(entt::entity object, particles::Effect& effect) const override;
	[[nodiscard]] float PlayerAlignment(int player) const override;
	[[nodiscard]] std::optional<particles::CreatureSpellBody> CreatureBody(entt::entity creature) const override;
	[[nodiscard]] uint32_t TargetPointCount(entt::entity object) const override;
	[[nodiscard]] std::optional<glm::vec3> TargetPoint(entt::entity object, uint32_t index) const override;
	[[nodiscard]] float TargetScale(entt::entity object) const override;
	void FollowCameraPath(std::string_view file, std::string_view animation, const glm::mat4& placement, float pauseSeconds,
	                      float speedUp, entt::entity spell) override;
	[[nodiscard]] particles::ObjectEffectsInterface* ObjectEffects() override { return &_objectEffects; }
	[[nodiscard]] particles::FragmentSourceInterface* FragmentSource() override { return &_fragments; }

	/// Once a game turn: the sounds follow their particles, loops play on, and the sounds let go stop
	void ProcessSounds();
	/// Once a game turn: the glows nothing lights any more go
	void ProcessGlows();
	[[nodiscard]] size_t SoundCount() const { return _sounds.size(); }
	/// The sounds that travel to the listener and do not play yet
	[[nodiscard]] size_t WaitingSoundCount() const
	{
		return static_cast<size_t>(std::ranges::count_if(_sounds, [](const PlayingSound& sound) { return sound.wait > 0.0f; }));
	}
	void Reset();
	/// The sounds of the particles of effects that a save does not keep, which the save leaves out too
	using LeftOutSounds = std::set<const particles::ParticleSoundLink*>;
	/// The things that the world shares with the effects go first in the tables of a save, in the order of the world.
	/// The local tables hold the things of the effects of this computer. With local false, these things stay out. With
	/// local true, only these things go in.
	void NumberShared(particles::SharedTables& tables, const LeftOutSounds& leftOut, const particles::SharedTables& localThings,
	                  bool local) const;
	/// Adds the things that only the effects of this computer use: the gesture trails that wait for their effect
	void AddLocalThings(particles::SharedTables& localThings) const;
	/// The forms of the parts of a save that the world reads
	enum class SavedForm : uint8_t
	{
		/// The part of a save from before the split, or a part from before the move of the sound waits to this computer.
		/// Each sound keeps its wait.
		WaitWithSound,
		/// The synced part keeps no value from the listener. The part of this computer keeps the waits of the sounds.
		WaitOnThisComputer,
	};
	/// The synced running state of the world: its lists, and the sounds of the synced particles that live. The things
	/// in the local tables stay out. LoadState also reads this part of an older save, which kept all the things.
	void SaveState(particles::EffectWriter& out, const LeftOutSounds& leftOut,
	               const particles::SharedTables& localThings) const;
	bool LoadState(particles::EffectReader& in, SavedForm form);
	/// The running state of the world that only this computer uses: the gesture trails, the things of the local tables
	/// with their places in the lists, and the sounds that travel to the listener. LoadLocalState puts them back after
	/// LoadState.
	void SaveLocalState(particles::EffectWriter& out, const LeftOutSounds& leftOut,
	                    const particles::SharedTables& localThings) const;
	bool LoadLocalState(particles::EffectReader& in, SavedForm form);

private:
	/// A sound a particle started, and what plays it
	struct PlayingSound
	{
		std::shared_ptr<particles::ParticleSoundLink> link;
		entt::entity emitter {entt::null};
		/// What the sound is played for: its own, so that a second effect playing the same sound doesn't find this one
		/// playing and give up
		entt::entity owner {entt::null};
		/// Let go of by its particle, and how it is fading out
		bool letGo {false};
		particles::SoundLetGo fade;
		/// Seconds before it is heard, while it travels to the listener. This value comes from the listener of this
		/// computer.
		float wait {0.0f};
		/// Where the listener hears the sound start: where its particle was when it started the sound
		glm::vec3 start {0.0f};
	};
	/// How a save keeps a sound
	enum class SoundRecord : uint8_t
	{
		/// The sound of a synced particle in the synced part: only its link
		Link,
		/// A sound of an older save, with its wait
		LinkAndWait,
		/// A sound in the part of this computer, with its wait and the place where the listener hears it start
		LinkWaitAndStart,
	};
	/// A sound that its particle let go of was dying away, and a sound whose particle is gone and that does not wait to
	/// be heard is over: a load ends them
	[[nodiscard]] static bool EndsWithLoad(const PlayingSound& sound);
	/// Whether a save keeps the sound
	[[nodiscard]] static bool Saved(const PlayingSound& sound, const LeftOutSounds& leftOut);
	/// Whether the synced part keeps the sound: a kept sound of a synced particle that lives. A sound whose particle has
	/// gone stays only while it travels to the listener of this computer, so the part of this computer keeps it.
	[[nodiscard]] static bool InSyncedPart(const PlayingSound& sound, const LeftOutSounds& leftOut,
	                                       const particles::SharedTables& localThings);
	/// Writes a sound in the part of this computer
	static void WriteSound(particles::EffectWriter& out, const PlayingSound& sound);
	/// A sound of a save, none when the load ends it
	[[nodiscard]] static std::optional<PlayingSound> ReadSound(particles::EffectReader& in, SoundRecord record);
	void Play(PlayingSound& sound) const;
	/// The particle let go of the sound: stopped at once, or, when its effect file soft-releases it, its loop released
	/// and its first step quieter taken
	static void LetGo(PlayingSound& sound);
	/// A let-go sound one step quieter, stopped once silent
	static void Fade(PlayingSound& sound);
	/// The sound has gone: its owner goes with it
	static void Forget(PlayingSound& sound);
	/// A new owner of its own for a sound of a particle, among the entities of the audio
	static entt::entity NewSoundOwner();

	GameObjectEffects _objectEffects;
	GameFragmentSource _fragments;
	std::set<entt::entity> _claimed;
	/// What lightning struck, waiting for its arcs
	std::vector<entt::entity> _arcsWaiting;
	/// Held by every arc crawling over an object, so that its count is how many there are
	std::shared_ptr<char> _liveArcs {std::make_shared<char>()};
	std::vector<particles::BeliefSprite> _beliefSprites;
	/// The recognised gestures' trails waiting to be shown, and the sheets of light the trails raised
	std::vector<std::shared_ptr<particles::GestureTrail>> _gestureTrails;
	mutable std::vector<std::weak_ptr<particles::LightSheet>> _lightSheets;
	/// The sheets of light that systems and entities keep
	mutable std::vector<std::weak_ptr<particles::LightSheet>> _ownedSheets;
	bool _arcsWanted {false};
	/// The bolts from hands, and how many there have been
	std::vector<std::weak_ptr<particles::BoltShare>> _bolts;
	uint64_t _boltOrder {0};
	std::vector<PlayingSound> _sounds;
	/// The storms the effects laid over the land, by the numbers the effects know them by
	std::unordered_map<uint32_t, entt::entity> _rainStorms;
	uint32_t _nextRainStorm {1};
	mutable std::vector<std::weak_ptr<particles::ShieldSphere>> _shields;
	/// The game's sound actions by name, read when first needed
	mutable std::optional<std::map<std::string, int32_t, std::less<>>> _soundActions;
};

/// The models and light maps of the particle creators, found by their names and loaded once through the resource caches
class GameCreatorResources final: public particles::CreatorResourcesInterface
{
public:
	[[nodiscard]] std::optional<entt::id_type> MeshByName(std::string_view name) override;
	[[nodiscard]] std::optional<entt::id_type> MeshByFile(std::string_view path) override;
	[[nodiscard]] std::optional<entt::id_type> LightMap(std::string_view path, int pitch, int channels, int framesInFile,
	                                                    int framesInUse) override;

private:
	/// The game's list of models by name, read when first needed
	std::optional<std::map<std::string, int32_t, std::less<>>> _meshNames;
};

class ParticleRuleSystem final: public ParticleRuleSystemInterface
{
public:
	/// The first number of an effect of this computer only. The synced effects take the numbers below it, and the
	/// simulation's numbers start above it, so the three sets of numbers never meet.
	static constexpr EffectId k_FirstLocalEffect = 0x4000'0001u;

	ParticleRuleSystem();
	~ParticleRuleSystem() override;
	ParticleRuleSystem(const ParticleRuleSystem&) = delete;
	ParticleRuleSystem& operator=(const ParticleRuleSystem&) = delete;
	ParticleRuleSystem(ParticleRuleSystem&&) = delete;
	ParticleRuleSystem& operator=(ParticleRuleSystem&&) = delete;

	EffectId Start(std::string_view file, glm::vec3 origin, float magnitude, bool synced) override;
	EffectId Start(ParticleType type, glm::vec3 origin, float magnitude, bool synced) override;
	void LoadAllTypes() override;
	EffectId StartForSpell(ParticleType type, glm::vec3 origin, glm::vec3 direction, float magnitude,
	                       particles::SpellSink& sink, bool synced) override;
	bool ProcessForSpell(EffectId id, const particles::ProcessInfo& info, float seconds) override;
	bool ProcessByFrame(EffectId id, float seconds) override;
	EffectId StartByFrame(ParticleType type, glm::vec3 origin, float magnitude) override;
	EffectId StartOwned(ParticleType type, glm::vec3 origin, float magnitude) override;
	void SetShown(EffectId id, bool shown) override;
	EffectId StartSpotVisual(SpotVisualType type, glm::vec3 position, std::optional<int> turns, entt::entity owner,
	                         float magnitude, bool synced) override;

	EffectHandle Serve(const EffectRequest& request, EffectHandle handle) override;
	void Move(EffectHandle handle, glm::vec3 position) override { SetOrigin(handle, position); }
	void Stop(EffectHandle handle) override { CloseDown(handle); }
	void Remove(EffectHandle handle) override { Delete(handle); }
	void RemoveOwnedBy(entt::entity owner) override;
	void SetMagnitude(EffectHandle handle, float magnitude) override;
	void SetAlpha(EffectHandle handle, float alpha) override;
	void SetProcessInfo(EffectHandle handle, const particles::ProcessInfo& info) override;
	void SetEnabled(EffectHandle handle, bool enabled) override;

	void SetOrigin(EffectId id, glm::vec3 origin) override;
	void SetPlayer(EffectId id, int player) override;
	void SetPower(EffectId id, float power) override;
	void SetPlayerSymbols(std::vector<particles::draw::PlayerSymbol> symbols) override;
	void SetFightMarks(std::vector<particles::draw::FightMark> marks) override { _fightMarks = std::move(marks); }
	void SetDrawPath(EffectId id, particles::draw::DrawPath path) override;
	bool Renumber(EffectId from, EffectId to) override;
	void KeepOutOfSaves(EffectId id) override;
	void SetDrawOffset(EffectId id, glm::vec3 offset) override;
	void SetHidden(EffectId id, bool hidden) override;
	void SetLifeListener(EffectLifeListener* listener) override { _lifeListener = listener; }
	void SetLiveHand(EffectId id, glm::vec3 hand) override;
	void SetInTemple(EffectId id) override;
	void SetDrawnAlone(EffectId id) override;
	[[nodiscard]] std::shared_ptr<particles::ShieldSphere> FindShield(glm::vec3 point, float margin) const override;
	[[nodiscard]] size_t GetSoundCount() const override { return _world.SoundCount(); }
	[[nodiscard]] size_t GetWaitingSoundCount() const override { return _world.WaitingSoundCount(); }
	void AddTarget(EffectId id, entt::entity target) override;
	void AddTargetPosition(EffectId id, glm::vec3 position) override;
	void AddPlasma(EffectId id, const particles::PlasmaCommand& command) override;
	void AddManaPathSpark(EffectId id, const particles::mana_path::Spark& spark) override;
	void AddBeliefSprite(const particles::BeliefSprite& sprite) override;
	void AddHandManaPathSpark(const particles::mana_path::Spark& spark) override;
	void AddStormBolt(glm::vec3 cloudPoint) override;
	void ExplodeObject(entt::entity object, glm::vec3 origin, float speed) override
	{
		_world.ObjectEffects()->ExplodeObject(object, origin, speed);
	}
	void AddGestureTrail(std::shared_ptr<particles::GestureTrail> trail) override;
	void AddLightSheet(const std::shared_ptr<particles::LightSheet>& sheet) override { _world.AddOwnedSheet(sheet); }
	void UpdateFrame(float gameSeconds, const HandFrame& hand) override;
	void ScrollChains(std::chrono::milliseconds frameRealTime, std::chrono::milliseconds frameGameTime) override;
	void CloseDown(EffectId id) override;
	void RestoreSink(EffectId id, particles::SpellSink* sink) override;
	void Delete(EffectId id) override;
	[[nodiscard]] bool IsRunning(EffectId id) const override;
	[[nodiscard]] particles::Effect* Find(EffectId id) override;

	void ProcessTurn() override;
	void Reset() override;
	void SetFreshIdsAtNextReset() override { _freshIdsAtNextReset = true; }
	/// The synced part of the save: the synced effects and the world that they use
	void SaveState(reflection::BinaryWriter& out) const override;
	bool LoadState(reflection::BinaryReader& in) override;
	[[nodiscard]] SavedStateInterface* LocalEffectsState() override { return &_localPart; }
	[[nodiscard]] SavedStateInterface* OldSaveState() override { return &_oldPart; }

	void ForEachDrawn(const std::function<void(const DrawnEffect&)>& visit) const override;
	[[nodiscard]] std::optional<DrawnEffect> FindDrawn(EffectId id) const override;
	[[nodiscard]] std::vector<std::shared_ptr<particles::LightSheet>> LightSheets() const override
	{
		return _world.LightSheets();
	}
	[[nodiscard]] std::span<const particles::draw::PlayerSymbol> PlayerSymbols() const override { return _playerSymbols; }
	[[nodiscard]] std::span<const particles::draw::FightMark> FightMarks() const override { return _fightMarks; }
	[[nodiscard]] std::span<const std::string> SheetNames() const override { return _sheetNames; }
	[[nodiscard]] const particles::ParticleWorldInterface* World() const override { return &_world; }
	[[nodiscard]] std::vector<EffectInfo> GetEffects() const override;
	[[nodiscard]] BeliefSpritesSeen GetBeliefSpritesSeen() const override;
	[[nodiscard]] std::vector<std::string> GetFileNames() const override;
	void SetPaused(bool paused) override { _paused = paused; }
	[[nodiscard]] bool IsPaused() const override { return _paused; }

private:
	/// A thing holding a visual a script started has been deleted: its visual ends at once
	void OnScriptSpotVisualGone(entt::registry& registry, entt::entity thing);
	/// The effect of a selection at the hand steps
	void StepSelectionEffect(float gameSeconds, const HandFrame& hand);
	std::vector<entt::scoped_connection> _connections;
	/// The effect that throws the pieces of what the blasts break
	EffectId _explodeObject {k_NoEffect};
	/// The effect every storm's forked bolts come down in, and the bolts let down before it first ran
	EffectId _stormBolts {k_NoEffect};
	std::vector<glm::vec3> _pendingStormBolts;
	/// The effect every recognised gesture's trail shows in, and the chain behind the hand while it gestures; both run
	/// all the time, started again whenever they end
	EffectId _gestureTrails {k_NoEffect};
	EffectId _gestureChain {k_NoEffect};
	/// The effect round the hand while the miracle selection or the leash picker is open
	EffectId _selectionEffect {k_NoEffect};
	struct Running
	{
		EffectId id;
		std::string file;
		std::unique_ptr<particles::Effect> effect;
		/// Stepped by its miracle, or as it is drawn, not by ProcessTurn
		bool ownedBySpell {false};
		/// Stepped every frame, not by ProcessTurn
		bool everyFrame {false};
		/// One of the temple's rooms' effects, drawn only while the player is in the temple
		bool inTemple {false};
		/// Drawn only by itself, never with the world's or the temple's effects
		bool drawnAlone {false};
		/// Drawn in this frame; false while the frame does not draw the thing that draws it
		bool shown {true};
		/// A spot visual's turns left, negative for ever
		std::optional<int> turnsLeft;
		/// An object it follows and ends with
		entt::entity owner {entt::null};
		particles::draw::DrawPath path {particles::draw::DrawPath::Sorted};
		/// Everything it draws is moved by this, as the miracle in the hand follows the hand between turns
		glm::vec3 drawOffset {0.0f};
		/// A save keeps it, unless its owner starts it again after a load
		bool saved {true};
		/// The point where this computer draws its hand this frame, when it casts the miracle from the hand. The frame sets it
		/// again before each draw, so a save does not keep it.
		std::optional<glm::vec3> liveHand;
	};

	/// What the draw reads of a running effect
	[[nodiscard]] static DrawnEffect Drawn(const Running& running);
	std::deque<Running>::iterator FindRunning(EffectId id);
	/// The effect ends and goes. The record of the simulation hears of it.
	void Forget(std::deque<Running>::iterator effect);
	/// Tells the record of the simulation, if any, of the start of an effect
	void TellStarted(EffectId id) const;
	[[nodiscard]] std::deque<Running>::const_iterator FindRunning(EffectId id) const;
	/// Steps an effect; true once it has ended
	bool StepEffect(particles::Effect& effect, float seconds);
	/// The particle file of an effect, loaded the first time; none if it cannot be loaded. The system does not
	/// search again for a file that it could not load.
	std::shared_ptr<const psys::ParticleFile> LoadFile(std::string_view file);
	/// Adds the names of the sheets that the effect uses to the list of the draw
	void AddSheetNames(const particles::Effect& effect);
	/// The gesture trails' effect, started when it isn't running
	void KeepGestureTrails();
	/// The sparks showers over objects move on by a frame's game time
	void StepShowers(float gameSeconds);

	/// The part of the save that keeps the effects of this computer
	class LocalPart final: public SavedStateInterface
	{
	public:
		explicit LocalPart(ParticleRuleSystem& rules)
		    : _rules(&rules)
		{
		}
		void SaveState(reflection::BinaryWriter& out) const override { _rules->SaveLocalState(out); }
		bool LoadState(reflection::BinaryReader& in) override { return _rules->LoadLocalState(in); }

	private:
		ParticleRuleSystem* _rules;
	};
	/// The one part of the particles in a save from before the split. A save does not write it.
	class OldPart final: public SavedStateInterface
	{
	public:
		explicit OldPart(ParticleRuleSystem& rules)
		    : _rules(&rules)
		{
		}
		bool LoadState(reflection::BinaryReader& in) override { return _rules->LoadOldState(in); }

	private:
		ParticleRuleSystem* _rules;
	};
	struct SaveNumbering;
	/// The things of the synced part of the last load, by their numbers. The part of this computer refers to them. The
	/// part holds them weakly, so that the things that nothing holds go after the load as before.
	struct LoadedSyncedThings
	{
		std::vector<std::weak_ptr<particles::ParticleSoundLink>> sounds;
		std::vector<std::weak_ptr<particles::BoltShare>> bolts;
		std::vector<std::weak_ptr<particles::ShieldSphere>> shields;
		std::vector<std::weak_ptr<particles::LightSheet>> sheets;
		std::vector<std::weak_ptr<particles::CarriedObject>> carried;
		std::vector<std::weak_ptr<particles::GestureTrail>> trails;
	};
	enum class ReadResult
	{
		Read,
		/// The file of the effect is gone, so the load leaves out the effect
		Skipped,
		Failed,
	};

	/// A synced effect that a save keeps
	[[nodiscard]] static bool IsSynced(const Running& running) { return running.saved && running.effect->IsSynced(); }
	/// An effect of this computer that a save keeps
	[[nodiscard]] static bool IsLocal(const Running& running) { return running.saved && !running.effect->IsSynced(); }
	/// Gives the numbers of the shared things to the two parts of a save
	void NumberForSave(const reflection::BinaryWriter& out, SaveNumbering& numbering) const;
	static void SaveRunning(reflection::BinaryWriter& bytes, particles::EffectWriter& effects, const Running& running);
	static void WarnUnsaved(std::initializer_list<const reflection::BinaryWriter*> writers);
	void SaveLocalState(reflection::BinaryWriter& out) const;
	bool LoadLocalState(reflection::BinaryReader& in);
	bool LoadOldState(reflection::BinaryReader& in);
	/// Reads the tables, the effects and the world of the synced part, or of the one part of an old save
	bool LoadSyncedBody(reflection::BinaryReader& in, GameParticleWorld::SavedForm form);
	ReadResult ReadRunning(reflection::BinaryReader& in, particles::EffectReader& reader, Running& running);
	static void RestoreCarried(std::span<const std::shared_ptr<particles::CarriedObject>> carried);

	LocalPart _localPart {*this};
	OldPart _oldPart {*this};
	/// The things of the synced part of the last load, for the part of this computer
	std::optional<LoadedSyncedThings> _loadedSynced;
	GameCreatorResources _resources;
	particles::ParticleClassRegistry _classes;
	GameParticleWorld _world;
	particles::maths::ValueNoise _noise;
	/// Newest first, as they are stepped and drawn
	std::deque<Running> _effects;
	/// The number of the next synced effect. Only the synced effects take these numbers, so each computer of a network game
	/// gives the same numbers to the same effects.
	EffectId _nextId {1};
	/// The number of the next effect of this computer only. These numbers start at k_FirstLocalEffect.
	EffectId _nextLocalId {k_FirstLocalEffect};
	/// The record of the simulation that hears of each start and end of an effect
	EffectLifeListener* _lifeListener {nullptr};
	/// The number that the simulation gave the next effect to start, none when the particles give their own
	EffectId _givenId {k_NoEffect};
	bool _freshIdsAtNextReset {false};
	bool _paused {false};
	/// When the ribbons' textures last slid on
	water_rings::Pace _chainScrollPace;
	/// A symbol of belief waits for its effect
	bool _beliefWanted {false};
	/// The symbols of belief that rose last, the newest at the front, and the count of all of them. Only the inspector
	/// reads these.
	static constexpr size_t k_BeliefSpritesSeen = 32;
	std::deque<particles::BeliefSprite> _beliefSpritesSeen;
	uint64_t _beliefSpriteCount {0};
	/// The effect the hand's mana path sparks run in, once started
	EffectId _handManaPath {k_NoEffect};
	/// The players' symbols drawn by themselves this frame
	std::vector<particles::draw::PlayerSymbol> _playerSymbols;
	std::vector<particles::draw::FightMark> _fightMarks;
	/// The names of the sheets that the started effects use, in the order of their first use
	std::vector<std::string> _sheetNames;
	std::set<std::string, std::less<>> _knownSheetNames;
	/// The particle classes already reported as not run yet
	std::set<std::string, std::less<>> _reportedUnported;
	/// The particle files that the game's files do not have. The system does not search for them again.
	std::set<entt::id_type> _missingFiles;
};

} // namespace openblack::ecs::systems
