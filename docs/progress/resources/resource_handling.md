# Giving resources by hand

The player's hand can take food and wood from piles, fields and forests, carry them and give them to a town, the worship
site or anyone else, which the creature watches and may copy.

**Progress: 9/16 done, 0 partial — 56%**

## Taking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Picking from a pile takes a handful into a pot in the hand, drawn by how much it holds | done | `HandGrabSystem` scoop; the handful is drawn by its amount |
| Holding the button on a pile keeps taking more | done | The scoop's ramp, 8 rising to 70 a turn over 6 s (`hand_grab` rules); see `../hand/multi_pickup.md` |
| Tugging at a ripe field pulls up a handful of crop (wheat in the hand) | done | Field scoop (`HandGrabSystem`) |
| Pulling up a tree gives its branches or logs in the hand | todo | see `../nature/trees.md` |
| Tugging at a big forest pulls out a tree | todo | see `../nature/forests.md` |
| The hand may take only inside its player's influence | done | `HandGrabSystem::Press` tests the hand's point on the land against the player's influence |
| Another player's store or field can be taken from (unconfirmed what is allowed) | todo | |

## Giving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Dropping food or wood on a store adds it to the store | done | `ResourceStoreSystem` (pressed onto a storage pit, or a slow pour onto it); tests `test_store_rules`, `test_hand_grab_system` |
| Dropping it on the worship site feeds the worshippers | todo | see `../worship/worship_sites.md` |
| Dropping it on a building site or the workshop gives it wood | todo | see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Dropping it anywhere else makes a pile there | done | The slow pour (`GameHandGrabWorld`, `ResourceStoreSystem::PourAt`); lost in water |
| Throwing a pot sends it flying; it lands as a pile, or floats in the water | done | `object_physics::EndPot`: on land a pile (or into stores and piles), in water it floats |
| Giving to a town that wants it impresses it, more the more it wants | done | Hand gifts go through the giving formula (`ResourceStoreSystem`, `TownDesireSystem::RecomputeDesire`) |
| Giving food or wood is a good deed | done | The giver's alignment moves by the damped value (`ResourceStoreSystem`) |
| The creature sees its god adding to a store and may copy it | todo | see `../creature/` |
| Tooltips over a pile or store say what the hand can do | todo | see `../interface/` |
