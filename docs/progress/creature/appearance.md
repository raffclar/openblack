# Creature appearance

A creature's body shows what it has become. Each species has a base mesh and a mesh at either end of three axes: evil
to good, thin to fat and weak to strong. The body is drawn as the base pulled towards one mesh on each axis, and its
skin is blended towards the evil or good skin, so an evil creature grows dark and twisted and a good one bright and
noble.

**Progress: 30/35 done, 1 partial — 87%**

## The body's shape

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each species has a base mesh and evil, good, thin, fat, weak and strong meshes | done | `creature_body` mesh names; test `NamesTheMeshOfEachAppearance` |
| The ogre's variants keep its base name | done | test `TheOgresVariantsKeepItsBaseName` |
| A species missing a variant mesh uses its base for that side | done | test `MissingMeshesFallBackOnTheBase` |
| The creature's alignment is the evil to good axis | done | test `AlignmentIsTheEvilGoodAxis` |
| Its fatness runs from thin to fat | done | test `FatnessRunsFromThinToFat` |
| Its strength, with its species' own strength counted in, runs from weak to strong | done | test `StrengthCountsFourTimesTheSpecies` |
| Each axis pulls every vertex and normal from the base towards the mesh on its side | done | tests `EachAxisPullsTowardsTheMeshOnItsSide`, `BlendMovesTheBaseTowardsEachMesh` |
| The fatness the body shows follows the creature's fatness by at most a hundredth a turn | done | test `ShownFatnessFollowsByAHundredthATurn` |
| The body is only redrawn once an axis has moved far enough | done | test `SmallChangesDoNotRedrawTheBody` |
| A new alignment redraws every axis; a new fatness the fatness and strength; a new strength only the vertices | done | tests `ANewAlignmentRedrawsEveryAxis`, `ANewFatnessRedrawsFatnessAndStrength`, `ANewStrengthOnlyReshapesTheVertices` |
| The rest pose and the animations follow the evil to good and thin to fat axes | done | see [animation.md](animation.md) |

## The skin

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each skin of the base mesh is blended towards the matching skin of the evil or good mesh | done | `creature_skin`; tests `SkinsArePairedByTheirPlaceInTheList`, `WeightIsTheAxisIn` |
| A neutral creature shows its base skins | done | test `NeutralShowsNoVariant` |
| The blend is done a 4-bit channel at a time in whole steps, as the game does | done | tests `ChannelIsAWeightedMeanRoundedDown`, `TexelBlendsEachChannelOnItsOwn`, `WeightIsCappedAt` |
| Tattoos, then wounds, then blood are painted over the blended skin | done | see [creature_tattoos.md](creature_tattoos.md) and [marks.md](marks.md) |
| The skin is repainted only when the alignment drawn, the tattoos or the marks change | done | `CreatureSkinSystem` revisions |

## How the alignment moves

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature has its own alignment, separate from its player's | done | `Creature::alignment`; see [../worship](../worship/) for the player's |
| Its alignment moves by at most its species' change a turn | done | `AlignmentSystem::UpdateTurn`, `StepPendingAlignment` |
| The miracles it casts move its alignment by what they do to the world | done | `MagicSystem.cpp` adds to its pending alignment |
| What it does with its body moves its alignment: eating villagers, killing, destroying, helping, healing | todo | the game's deeds for creatures are not counted; see [object_actions.md](object_actions.md) and [town_actions.md](town_actions.md) |
| The nice and nasty spells pull its alignment for a while and put it back | done | see [../miracles](../miracles/) |
| The aggression and compassion leashes don't change how it looks, only how it feels | done | see [leash.md](leash.md) |
| Scripts can read and set alignments | todo | `GET_ALIGNMENT`/`SET_ALIGNMENT` are stubs |
| The alignment is kept in creature files | done | `CreatureMindFileBody.cpp` |

## Drawing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature is drawn with the land's lighting | done | `Renderer.cpp`; see [../rendering](../rendering/) |
| A creature casts a soft shadow of its silhouette onto the land and what stands on it, from the scene's light, kept at least 45 degrees up | done | `src/Graphics/CreatureShadow.*` |
| The shadow fades out as the camera pulls away from the creature | done | `CreatureShadow` |
| Only the nearest few creatures cast shadows | done | `CreatureShadow::k_MaxShadows` |
| The hair casts no shadow | done | `CreatureShadow` |
| A frozen creature looks icy, a dark blue sheen growing over it as it freezes | done | `RenderingSystem.cpp`, `Renderer.cpp` |
| An invisible creature dissolves into static and casts no shadow | done | `Renderer.cpp`, the invisible spell's fizz |
| Creatures far from the camera are drawn with simpler meshes | todo | (unconfirmed whether the game has level of detail for creatures) |
| Species-specific extras: eyes, eyelids and hair tufts | done | see [face_eyes_hair.md](face_eyes_hair.md) |
| Each player's creature can be told apart by its tattoos and colours | partial | tattoos are drawn; see [creature_tattoos.md](creature_tattoos.md) |
| A creature carrying a glow from its player's creed (the hand's glow) when a script sets it | todo | `SET_CREATURE_CREED_PROPERTIES` is a stub |
