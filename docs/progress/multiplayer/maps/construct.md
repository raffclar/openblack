# Construct

openblack's own test land, not one of the game's: a flat plain with one Norse house, your temple and a weeping stone,
used by openblack's automatic screenshot checks. Copied into the game's playground folder, the game's skirmish box
would list it as "construct" like any other script there.

**Players:** 1 · **Landscape:** `Data/Landscape/construct.lnd` · **Script:** `Scripts/Playgrounds/construct.txt` · **Mode:** skirmish (test land)

**Progress: 6/9 done, 3 partial — 83%**

What the script builds is in [playground_scripts.md](../../scripts/playground_scripts.md#construct-loading) (its
"Construct" sections). The land and script were added to openblack's test data in 2021 and now live in the
openblack-assets repository; the continuous-integration workflow (`.github/workflows/ci-vcpkg.yml`) starts openblack
on it and shows its screenshot beside one taken in the original game.

## Choosing and starting it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game's skirmish box would list it by its file name, "construct" | partial | the debug menu lists it by its start message title, "Construct", with "Testing landscape" as a tooltip (`src/Level.cpp`); no skirmish box |
| It can be started straight from the command line for the screenshot check | done | `-s Playgrounds/construct.txt` in the CI workflow |
| The camera starts on the temple (the CI moves it to a fixed point for its screenshot) | done | `START_CAMERA_POS` |

## The land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A plain about 1,750 by 1,600 paces, flat at one low height everywhere, all of one ground type | done | `Game::LoadLandscape` |
| Towns project 0.6 of their normal influence and you thirty times yours, so the whole plain is yours to act on | done | `InfluenceSystem` |

## What you start with

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Your temple and a Norse worship site on the same spot | partial | the temple is made (`CitadelArchetype`); `CREATE_WORSHIP_SITE` is empty |
| A Norse town of one house, which holds 850,000 wood, with a forester and a housewife | done | `TownArchetype`, `AbodeArchetype`, `VillagerArchetype` |
| The town believes in you and offers food, wood, a stronger heal, a stronger water and the strongest fire | partial | belief is set; village centre miracles are not offered (`CREATE_NEW_TOWN_SPELL`) |
| A weeping stone and its reward stone | done | `MobileStaticArchetype` |
