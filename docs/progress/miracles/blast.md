# Blast

A beam of light drops from the sky onto the hand's point and explodes: a shock wave shatters trees, people and small
things nearby into flying pieces, leaves buildings standing as empty shells, and scars the land. The first power-up adds
six more blasts around it, the second a long barrage. It is a real single-player miracle: Land 5's script gives it (and
its power-ups) to towns 2, 3 and 5, and the skirmish maps hand it out through dispensers, one-off seeds and towns; the
firefly rewards of Lands 1–4 never offer it, though Land 5's firefly table does (weight 0.1, about 0.6%;
see [../nature/fireflies.md](../nature/fireflies.md#the-lands-reward-table)). (Players know it as the Megablast; that name is unconfirmed in the data.)

**Progress: 36/42 done, 4 partial — 90%**

## Casting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The blast is cast the moment the gesture is pressed, at the hand's point within the player's influence | done | `src/Magic/CastInput.cpp`, `src/Magic/SpellRules.cpp` |
| The held seed shows no effect on the hand, only the seed itself | done | Info tables (no in-hand effect) |
| It costs 16000, 32000 or 60000 to cast, 10 per event and 3000 or 10000 when it strikes a shield | done | `src/Magic/SpellRules.cpp`, `src/Magic/SpellChants.cpp`. See [prayer_cost.md](prayer_cost.md) |
| The second power-up's long barrage keeps drawing prayer power as it goes, and only weakens when the caster can't pay | done | `src/Magic/SpellChants.cpp` |
| A script can cast it at a point | done | `src/Magic/ScriptCast.cpp` |
| A creature casts the plain blast: sometimes shows it first, goes near, backs off, faces the target and gestures | todo | No blast plan in `src/Creature/CreaturePlanActions.cpp`. See [creature_spells.md](creature_spells.md) |
| A blast under way is kept in a saved game | todo | openblack has no save games |

## The beam and the explosion

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The beam drops 120 m from the sky in 0.4 s onto the point | done | `src/Particles/ParticleBlastRules.cpp`; test `Blast.TheBeamDrops120MetresIn04Seconds` (`test/test_blast.cpp`) |
| A white flash marks the landing point for a second | done | `ParticleBlastRules.cpp` (spot visual) |
| A shield over the point is struck where the beam enters it, and sparks | done | Test `Blast.AShieldOverTheBlastIsStruckWhereTheWayDownEntersIt`. See [physical_shield.md](physical_shield.md) |
| For its time the blast burns and crushes around the point: heat 200/400/800 and radius 5/5/10 by power-up | done | `ParticleBlastRules.cpp`, `src/Magic/SpellBehaviours.cpp`; test `Blast.ItActsFromItsInitialDelayForItsTimeToDoEvents` |
| Its strength and the shock wave's reach grow with the caster's tribal power, up to five times | partial | The formula is done (test `Blast.ItsWaveReachesFurtherWithTribalPowerAndReachesObjectsByTheirEdge`), but tribal power only comes with worship, which openblack lacks, so it is always 1 |
| On dry land (higher than sea level by more than 3) it leaves smoke and a rubble scar that follows the land and lasts 15 s | done | `src/ECS/Systems/Implementations/ParticleObjectEffects.cpp`, `src/ECS/Systems/Implementations/ExplosionSystem.cpp`; scenarios `miracles.blast`, `miracles.blast_coast` |
| On water it leaves steam and three rings spreading to 5, 7 and 10 m over 0.7 s | done | Scenario `miracles.blast_water`; `src/ECS/Systems/Implementations/WaterRingSystem.cpp` |
| Old rings drift with the wind the way a reused ring slot did in the game | partial | Original quirk not reproduced: openblack has no fixed ring pool |
| Five rocks are thrown out from the centre | done | `ParticleBlastRules.cpp`; test `Blast.PiecesFlyOutFromBelowTheCentreAtTheBlastsSpeed` |
| A brown dust puff of fifteen sprites rises and fades over 1.5 s | done | Test `DustPuff.FifteenSpritesFlyUpAndOutGrowingAndFadingOverASecondAndAHalf` |
| Three bright cones stand over the point, keeping their height as they widen, and light the land white | done | Particle data; test `Blast.TheConesKeepTheirHeightWhateverTheirWidth` |
| Every new effect steps twice on its first turn, so the bang comes as soon after the cast as in the game | done | `src/Particles/` effect start (cross-group fix) |
| Emitters make at most one particle a step unless their data allows more | done | `src/Particles/ParticleCreateRules.cpp` (fixed in the firewater round) |

## The shock wave

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A wave spreads out from the point and reaches each object by its edge | done | `ParticleBlastRules.cpp`, `ParticleObjectEffects.cpp` |
| Up to 15 things are shattered into flying pieces and up to 15 removed | done | `ParticleBlastRules.cpp` |
| Creatures are never shattered or removed | done | `src/ECS/WorldObjects.cpp` |
| Trees, statues and small things are shattered and gone | done | `WorldObjects.cpp`; scenario `miracles.blast` |
| Buildings and spell dispensers are shattered but left standing with no life | done | `WorldObjects.cpp`; scenario `miracles.blast_spares` |
| Villagers are not shattered: the blast's heat sets those nearest alight and they burn to death | done | `src/ECS/Systems/Implementations/VillagerFire.cpp`; scenario `miracles.blast` |
| Animals in the wave fall dead | partial | Re-audit: they still vanish (`WorldObjects.cpp`); only the tornado's kills use the new dying clips (open fire item 56) |
| Teleport stones and totems are spared; pots and one-off seeds are never touched | partial | `WorldObjects.cpp`, `ParticleObjectEffects.cpp`. Re-audit: in `miracles.blast_spares` the teleport stone was never reached, so its case is unverified. Objects a script makes indestructible, or holds while an advisor speaks, are not spared: those script features aren't in openblack |
| Where the wave meets a shield it sparks it and pushes on it outward from the centre | done | `ParticleBlastRules.cpp` (fixed in the firewater round) |
| People nearby react to the miracle once | done | `src/Magic/ReactionRules.cpp` |

## Flying pieces

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A model breaks into chains of up to sixteen triangles joined by shared edges | done | `src/Particles/ParticleBlast.cpp`; test `Blast.AModelBreaksIntoChainsOfAtMostSixteenJoinedTriangles` |
| Pieces fly out, fall, bounce, tumble, fade over 3 s, shrink over 1–5 s and are gone at 6 s | done | `ParticleBlastRules.cpp` |
| Pieces are lit by their model's light | done | Test `Blast.BrokenPiecesAreShadedByTheModelLightInTheirOwnFrame` |
| The last thing to break is the first to fly | done | `ParticleBlastRules.cpp` (fixed in the firewater round) |

## Power-ups

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The first power-up adds six blasts 30–50 m around the first | done | Scenario `miracles.blast_pu1` |
| The second adds six at 25–40 m, then a barrage every few tenths of a second at 20–60 m | done | Scenario `miracles.blast_pu2` |
| Beams still falling when the barrage ends still bang | done | `ParticleBlastRules.cpp` |

## Sound and shake

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The falling beam whooshes, one of four samples at a random pitch | done | `src/Particles/ParticleSoundRules.cpp`; checked in game |
| It bangs 0.35 s later, one of two samples at a random pitch, at the point at sea level | done | `ParticleSoundRules.cpp`; checked in game |
| The camera shakes for 0.7 s if within 200 m; the nearest blast counts | done | Test `CameraShake.OnlyTheNearestCountsAtFullStrengthWithinItsRadiusFallingOverItsTime` |
| Every power-up blast has its own whoosh, bang and shake | done | Scenarios `miracles.blast_pu1`, `miracles.blast_pu2` |
| Pieces, smoke, scars and rings make no sound | done | Matches the particle data |
