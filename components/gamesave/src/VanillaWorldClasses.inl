/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The layout of each type of object in the game's world file, from the game's save code. Each function reads the
// values of one type in the order that the game writes them, its base type first. A tool made this file. Only
// VanillaWorld.cpp includes it, after the reader and after the functions whose layout changes with the values.

void ReadPlayerActionState(WorldReader& r);
void ReadComputerAttitude(WorldReader& r);
void ReadGameStats(WorldReader& r);
void ReadCitadelPart(WorldReader& r);
void ReadCitadelHeart(WorldReader& r);
void ReadFootpathLink(WorldReader& r);
void ReadFootpath(WorldReader& r);
void ReadFootpathNode(WorldReader& r);
void ReadStoragePit(WorldReader& r);
void ReadDataForScriptRemind(WorldReader& r);
void ReadField(WorldReader& r);
void ReadFishFarm(WorldReader& r);
void ReadSpecialVillager(WorldReader& r);
void ReadFlock(WorldReader& r);
void ReadMagicWood(WorldReader& r);
void ReadPileResource(WorldReader& r);
void ReadPotStructure(WorldReader& r);
void ReadPot(WorldReader& r);
void ReadMobileObject(WorldReader& r);
void ReadForest(WorldReader& r);
void ReadBigForest(WorldReader& r);
void ReadTree(WorldReader& r);
void ReadTownDesireFlags(WorldReader& r);
void ReadStandardBuildingSite(WorldReader& r);
void ReadBuildingSite(WorldReader& r);
void ReadPileFood(WorldReader& r);
void ReadGraveyard(WorldReader& r);
void ReadTownCentre(WorldReader& r);
void ReadTotemStatue(WorldReader& r);
void ReadPlannedAbode(WorldReader& r);
void ReadPlannedMultiMapFixed(WorldReader& r);
void ReadInterface(WorldReader& r);
void ReadLeashStatus(WorldReader& r);
void ReadDataPath(WorldReader& r);
void ReadParticleContainer(WorldReader& r);
void ReadJPSysInterface(WorldReader& r);
void ReadPSysManager(WorldReader& r);
void ReadStreetLantern(WorldReader& r);
void ReadDove(WorldReader& r);
void ReadMobileStatic(WorldReader& r);
void ReadAtomCore(WorldReader& r);
void ReadParticle3DSprite(WorldReader& r);
void ReadRenderParticle(WorldReader& r);
void ReadParticle3DPnt(WorldReader& r);
void ReadRenderParticleGoldenShower(WorldReader& r);
void ReadRenderParticleGameObjectRef(WorldReader& r);
void ReadBonfire(WorldReader& r);
void ReadRock(WorldReader& r);
void ReadCollectionDataEmitterRule(WorldReader& r);
void ReadBaseCollectionModifierData(WorldReader& r);
void ReadReaction(WorldReader& r);
void ReadArena(WorldReader& r);
void ReadMist(WorldReader& r);
void ReadInfluenceRing(WorldReader& r);
void ReadScriptMarker(WorldReader& r);
void ReadScriptHighlight(WorldReader& r);
void ReadStream(WorldReader& r);
void ReadDeadTree(WorldReader& r);
void ReadAnimatedStatic(WorldReader& r);
void ReadHand(WorldReader& r);
void ReadChallenge(WorldReader& r);
void ReadScriptTimer(WorldReader& r);
void ReadWorshipSite(WorldReader& r);
void ReadShowNeeds(WorldReader& r);
void ReadWorshipTotem(WorldReader& r);
void ReadShowNeedsVisuals(WorldReader& r);
void ReadDance(WorldReader& r);
void ReadGroupBehaviour(WorldReader& r);
void ReadDanceKeyFrame(WorldReader& r);
void ReadDanceKeyAction(WorldReader& r);
void ReadDanceGroup(WorldReader& r);
void ReadLandscapeVortexIn(WorldReader& r);
void ReadFootpathFinder(WorldReader& r);
void ReadSpell(WorldReader& r);
void ReadSpellResource(WorldReader& r);
void ReadSpellWater(WorldReader& r);
void ReadSpellWithObjects(WorldReader& r);
void ReadSpellFlock(WorldReader& r);
void ReadSpellFlockFlying(WorldReader& r);
void ReadSpellFlockGround(WorldReader& r);
void ReadSpellForest(WorldReader& r);
void ReadSpellShield(WorldReader& r);
void ReadSpellCreature(WorldReader& r);
void ReadMapShield(WorldReader& r);
void ReadPhysicalShield(WorldReader& r);
void ReadMagicShield(WorldReader& r);
void ReadMagicTeleport(WorldReader& r);
void ReadMagicTree(WorldReader& r);
void ReadMagicFood(WorldReader& r);
void ReadMagicFireBall(WorldReader& r);
void ReadSpellSeedGraphic(WorldReader& r);
void ReadSpellSeed(WorldReader& r);
void ReadOneOffSpellSeed(WorldReader& r);
void ReadFireEffect(WorldReader& r);
void ReadFireFly(WorldReader& r);
void ReadTotem(WorldReader& r);
void ReadHanoiBlock(WorldReader& r);
void ReadPuzzleTotem(WorldReader& r);
void ReadPlannedCitadelPart(WorldReader& r);
void ReadPlannedTownCitadelHeart(WorldReader& r);
void ReadPlannedWorshipSite(WorldReader& r);
void ReadWaterfall(WorldReader& r);
void ReadArenaSpellIcon(WorldReader& r);
void ReadBall(WorldReader& r);
void ReadReward(WorldReader& r);
void ReadSpellIcon(WorldReader& r);
void ReadTownSpellIcon(WorldReader& r);
void ReadCitadelBuildingSite(WorldReader& r);
void ReadScaffold(WorldReader& r);
void ReadComputerSeen(WorldReader& r);
void ReadComputerSpellCast(WorldReader& r);
void ReadCreed(WorldReader& r);
void ReadPlannedFeature(WorldReader& r);
void ReadWorshipSpellIcon(WorldReader& r);
void ReadWonder(WorldReader& r);
void ReadSpellWolf(WorldReader& r);
void ReadSpellDove(WorldReader& r);
void ReadWhale(WorldReader& r);
void ReadMissionaryControl(WorldReader& r);
void ReadDrawOffset(WorldReader& r);
void ReadDrawOffsetLT(WorldReader& r);
void ReadDrawOffsetDecay(WorldReader& r);
void ReadParticle3DObj(WorldReader& r);
void ReadParticleLightMap(WorldReader& r);
void ReadRenderParticleMist(WorldReader& r);
void ReadParticle3DAnim(WorldReader& r);
void ReadParticle3DAnimWithCamera(WorldReader& r);
void ReadParticle3DObjAnimTextured(WorldReader& r);
void ReadRenderParticleCreatureRef(WorldReader& r);
void ReadRenderParticleGameObject(WorldReader& r);
void ReadParticleChainJoint(WorldReader& r);
void ReadParticlePlayerSymbol(WorldReader& r);
void ReadSpellPointInf(WorldReader& r);
void ReadURPlasmaInf(WorldReader& r);
void ReadBaseAtomModifierData(WorldReader& r);
void ReadAtomDataURVolFXOnObject(WorldReader& r);
void ReadAtomDataURPlasma(WorldReader& r);
void ReadAtomDataURBeliefSprite(WorldReader& r);
void ReadAtomDataURSimpleBeam(WorldReader& r);
void ReadAtomDataURManaPathNew(WorldReader& r);
void ReadCollectionDataURCreatureSpellGeneric(WorldReader& r);
void ReadAtomDataURCreatureSpellGeneric(WorldReader& r);
void ReadAtomDataUROrientSpriteWithVelocity(WorldReader& r);
void ReadCollectionDataURAtomsAtEPTarget(WorldReader& r);
void ReadCollectionDataURCreatureSpellCompassion(WorldReader& r);
void ReadCollectionDataURCreatureSpell(WorldReader& r);
void ReadCollectionDataURCreatureSpellItch(WorldReader& r);
void ReadCollectionDataURCreatureSpellFreeze(WorldReader& r);
void ReadAtomDataURVortexAttract(WorldReader& r);
void ReadVortexObjectInfo(WorldReader& r);
void ReadAtomDataURSphereSurfaceTracer(WorldReader& r);
void ReadAtomDataCheckShieldDeflections(WorldReader& r);
void ReadAtomDataURForestPath(WorldReader& r);
void ReadAtomDataURExplosion(WorldReader& r);
void ReadCollectionDataURFlocking(WorldReader& r);
void ReadCollectionDataURCloudMoverNew(WorldReader& r);
void ReadCollectionDataURCloudGather(WorldReader& r);
void ReadAtomDataURCloudGather(WorldReader& r);
void ReadFlyingAtomDataURTornado(WorldReader& r);
void ReadCollectionDataURTornado(WorldReader& r);
void ReadCollectionDataEmitterRuleLightningSprite(WorldReader& r);
void ReadAtomDataARFadeOutOnceConditionTrue(WorldReader& r);
void ReadAtomDataAttatchFireBallToAtom(WorldReader& r);
void ReadAtomDataAddSoundToAtom(WorldReader& r);
void ReadAtomDataRemoveSoundFromAtom(WorldReader& r);
void ReadAtomDataRippleUpdateRuleGravityWithFloor(WorldReader& r);
void ReadAtomDataURBankedTurning(WorldReader& r);
void ReadAtomDataUpdateRuleShieldSpark(WorldReader& r);
void ReadAtomDataURVapourEndEffect(WorldReader& r);
void ReadAtomDataAddSubCollectionsToAtom(WorldReader& r);
void ReadAtomDataCreateNewBaseAtom(WorldReader& r);
void ReadParentAtomDataERGlintsOnTarget(WorldReader& r);
void ReadAtomDataERGlintsOnTarget(WorldReader& r);
void ReadAtomDataUROrientSpriteWithRandomAngle(WorldReader& r);
void ReadAtomDataURSideSpin(WorldReader& r);
void ReadAtomDataURInitialSpin(WorldReader& r);
void ReadAtomDataRemoveRuleAfterConditionTrue(WorldReader& r);
void ReadAtomDataUROrientWithVelocity(WorldReader& r);
void ReadAtomDataURFollowTargets(WorldReader& r);
void ReadCollectionDataURAddDefensiveSphere(WorldReader& r);
void ReadCollectionDataURWillowWisp(WorldReader& r);
void ReadCollectionDataERBurstFromParentAtom(WorldReader& r);
void ReadCollectionDataEREmitFromParentAtom(WorldReader& r);
void ReadAtomDataURHealSpellChakra(WorldReader& r);
void ReadCollectionDataLightningForkFlicker(WorldReader& r);
void ReadCollectionDataURHandSprinkle(WorldReader& r);
void ReadPlayerSubAction(WorldReader& r);
void ReadStreetLight(WorldReader& r);
void ReadAudio(WorldReader& r);
void ReadTownArtifact(WorldReader& r);
void ReadPuzzleGrain(WorldReader& r);
void ReadPuzzleMobileObject(WorldReader& r);
void ReadThingMusicInfo(WorldReader& r);
void ReadSpellDispenser(WorldReader& r);
void ReadAtomDataURLightSheetOnObject(WorldReader& r);

void ReadPlayerActionState(WorldReader& r)
{
	ReadPositioned(r);
	r.Ref();
	r.Refs();
	for (uint32_t i = 0; i < 8 && !r.Failed(); ++i)
	{
		r.Word();
	}
	r.Bytes(12);
	r.Point();
	r.Float();
	r.Ref();
	r.Float();
	r.Point();
}

void ReadComputerAttitude(WorldReader& r)
{
	r.Word();
}

void ReadGameStats(WorldReader& r)
{
	r.Ref();
	r.Word();
	r.Word();
	r.Word();
	r.Bytes(40);
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Bytes(2012);
	r.Bytes(2012);
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
}

void ReadCitadelPart(WorldReader& r)
{
	ReadMultiMapFixed(r);
	r.Word();
	r.Ref();
	r.Ref();
}

void ReadCitadelHeart(WorldReader& r)
{
	ReadCitadelPart(r);
	r.Word();
	r.Ref();
	r.Ref();
	r.Word();
	r.Word();
	r.Byte();
	r.Word();
	r.Ref();
	r.Ref();
	r.Word();
	r.Ref();
	r.Ref();
	r.Word();
	r.Ref();
}

void ReadFootpathLink(WorldReader& r)
{
	r.Refs("footpaths");
}

void ReadFootpath(WorldReader& r)
{
	r.Refs("nodes");
	r.Word("word20");
}

void ReadFootpathNode(WorldReader& r)
{
	r.Coords("pos");
	r.Byte("byte14");
	r.Refs("links");
}

void ReadStoragePit(WorldReader& r)
{
	ReadAbode(r);
	r.Refs();
	r.Refs();
}

void ReadDataForScriptRemind(WorldReader& r)
{
	r.Ref();
	r.Bytes(12);
	r.Word();
	r.Bytes(8);
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Ref();
}

void ReadField(WorldReader& r)
{
	ReadAbode(r);
	r.Word("cropInfo");
	r.Word();
	r.Word("food");
	r.Ref("fieldTown");
	r.Word("growthTurn");
	r.Byte("timesSown");
	r.Word("age");
	r.Refs("farmers");
	r.Byte();
}

void ReadFishFarm(WorldReader& r)
{
	ReadMultiMapFixed(r);
	r.Refs("fishermen");
	r.Ref("town");
	r.Word();
	r.Word("fish");
}

void ReadSpecialVillager(WorldReader& r)
{
	ReadVillager(r);
	r.Word("specialRow");
}

void ReadFlock(WorldReader& r)
{
	ReadContainer(r);
	r.Word();
	r.Word();
	r.Word();
	r.Ref("shepherd");
	r.Ref();
	r.Ref("heart");
	r.Refs("members");
	r.Word();
	r.Word();
	r.Half("domainRadius");
	r.Half("flockDistance");
	r.Word();
	r.Word();
	r.Word();
	r.Bytes(12, "domainCentre");
	r.Bytes(12);
	r.Word();
	r.Word();
	r.Word();
	r.Ref();
	r.Word();
}

void ReadMagicWood(WorldReader& r)
{
	ReadPileResource(r);
	r.Ref();
}

void ReadPileResource(WorldReader& r)
{
	ReadPotStructure(r);
	r.Bytes(48);
}

void ReadPotStructure(WorldReader& r)
{
	ReadPot(r);
	r.Ref();
	r.Ref();
	r.Byte();
}

void ReadPot(WorldReader& r)
{
	ReadMobileObject(r);
	r.Word();
	r.Ref();
	r.Word();
	r.Byte();
}

void ReadMobileObject(WorldReader& r)
{
	ReadMobile(r);
	r.Ref("heldObject");
	r.Float("xAngle");
	r.Float("zAngle");
	r.Ref("path");
}

void ReadForest(WorldReader& r)
{
	ReadContainer(r);
	r.Word("scenic");
	r.Ref();
	r.Word("forestId");
	r.Refs("grownTrees");
	r.Refs("growingTrees");
	r.Ref("bigForest");
	r.Word("lastTreeAddedTurn");
	r.Word("nextForestId");
}

void ReadBigForest(WorldReader& r)
{
	ReadMultiMapFixed(r);
	r.Ref("forest");
	r.Word("wood");
}

void ReadTree(WorldReader& r)
{
	ReadFixed(r);
	r.Half("growthCountdown");
	r.Word("maxSize");
	r.Ref("forest");
	r.Half("treeFlags");
}

void ReadTownDesireFlags(WorldReader& r)
{
	ReadObject(r);
	r.Ref();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
}

void ReadStandardBuildingSite(WorldReader& r)
{
	ReadBuildingSite(r);
	r.Ref("siteWoodPile");
}

void ReadBuildingSite(WorldReader& r)
{
	r.Ref("siteBuilding");
	r.Refs("siteWorkers");
	r.Refs();
	r.Refs();
	r.Word();
	// The places round the building where its builders stand: 128 points of three floats
	r.Bytes(1536, "sitePlaces");
	r.Word("siteWorkerCount");
	r.Word();
	r.Float("siteDesire");
	r.Float("siteLife");
}

void ReadPileFood(WorldReader& r)
{
	ReadPileResource(r);
	r.Ref();
}

void ReadGraveyard(WorldReader& r)
{
	ReadAbode(r);
	r.Word();
}

void ReadTownCentre(WorldReader& r)
{
	ReadAbode(r);
	r.Ref();
	r.Refs();
}

void ReadTotemStatue(WorldReader& r)
{
	ReadMultiMapFixed(r);
	r.Ref();
	r.Word();
	r.Word();
	r.Word();
}

void ReadPlannedAbode(WorldReader& r)
{
	ReadPlannedMultiMapFixed(r);
	r.Ref("town");
}

void ReadPlannedMultiMapFixed(WorldReader& r)
{
	ReadPositioned(r);
	r.Float("heading");
	r.Float("scale");
	r.Word("wasBuilt");
	r.Word();
	r.Word("created");
	r.Ref("footpathLink");
	r.Word("info");
}

void ReadInterface(WorldReader& r)
{
	ReadPositioned(r);
	r.Ref();
}

void ReadLeashStatus(WorldReader& r)
{
	r.Word();
	r.Word();
	r.Word();
	r.Byte();
	r.Word();
	r.Ref();
	r.Word();
	r.Word();
	r.Ref();
	r.Word();
}

void ReadDataPath(WorldReader& r)
{
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
}

void ReadParticleContainer(WorldReader& r)
{
	ReadPositioned(r);
	r.Byte();
	r.Ref();
	r.Ref();
	r.Word();
	r.Word();
}

void ReadJPSysInterface(WorldReader& r)
{
	r.Ref();
}

void ReadPSysManager(WorldReader& r)
{
	r.Word();
	r.Ref();
	r.Refs();
	r.Bytes(60);
	r.Bytes(12);
	r.Byte();
	r.Word();
	r.Word();
	r.Bytes(12);
	r.Bytes(12);
	r.Bytes(12);
	r.Word();
	r.Word();
	r.Word();
	r.Ref();
	r.Byte();
	r.Byte();
	r.Persistent();
	ReadSpellTargets(r);
}

void ReadStreetLantern(WorldReader& r)
{
	ReadObject(r);
	r.Word();
	r.Byte();
}

void ReadDove(WorldReader& r)
{
	ReadAnimal(r);
}

void ReadMobileStatic(WorldReader& r)
{
	ReadMultiMapFixed(r);
	r.Ref();
	r.Float("xAngle");
	r.Float("zAngle");
}

void ReadAtomCore(WorldReader& r)
{
	r.Refs();
	r.Refs();
	r.Refs();
	r.Bytes(12);
	r.Word();
	r.Bytes(48);
	r.Word();
	r.Word();
	r.Word();
	r.Bytes(12);
	r.Word();
	r.Word();
	r.Half();
	r.Bytes(56);
	r.Bytes(56);
	r.Word();
	r.Word();
	r.Bytes(12);
	r.Word();
	r.Ref();
	r.Ref();
	r.Ref();
	r.Byte();
}

void ReadParticle3DSprite(WorldReader& r)
{
	ReadRenderParticle(r);
	r.Byte();
	r.Persistent();
	r.Byte();
	r.Byte();
	r.Word();
	r.Word();
}

void ReadRenderParticle(WorldReader& r)
{
	r.Byte();
}

void ReadParticle3DPnt(WorldReader& r)
{
	ReadRenderParticle(r);
}

void ReadRenderParticleGoldenShower(WorldReader& r)
{
	ReadRenderParticleGameObjectRef(r);
}

void ReadRenderParticleGameObjectRef(WorldReader& r)
{
	ReadRenderParticle(r);
	r.Word();
	r.Ref();
}

void ReadBonfire(WorldReader& r)
{
	ReadRock(r);
	r.Word();
}

void ReadRock(WorldReader& r)
{
	ReadMobileStatic(r);
	r.Word();
	r.Word();
	r.Ref("rockPlayer");
}

void ReadCollectionDataEmitterRule(WorldReader& r)
{
	ReadBaseCollectionModifierData(r);
	r.Word();
	r.Word();
	r.Byte();
}

void ReadBaseCollectionModifierData(WorldReader& r)
{
	r.Persistent();
}

void ReadReaction(WorldReader& r)
{
	r.Ref();
	r.Refs();
	r.Word();
	r.Word();
	r.Ref();
	r.Word();
	r.Word();
	r.Ref();
	r.Word();
}

void ReadArena(WorldReader& r)
{
	ReadPositioned(r);
	r.Word();
	r.Word();
	r.Word();
	r.Refs();
	r.Ref();
}

void ReadMist(WorldReader& r)
{
	ReadPositioned(r);
	r.Word();
	r.Word();
	r.Word();
}

void ReadInfluenceRing(WorldReader& r)
{
	ReadPositioned(r);
	r.Ref("follows");
	r.Ref("player");
	r.Float("radius");
	r.Word("anti");
}

void ReadScriptMarker(WorldReader& r)
{
	ReadPositioned(r);
}

void ReadScriptHighlight(WorldReader& r)
{
	ReadFixed(r);
	r.Word("highlightUnknown");
	r.Word("highlightActive");
	r.Word("highlightHeightSet");
	r.Float("highlightHeight");
	r.Word("highlightText");
	r.Word("highlightCategory");
}

void ReadStream(WorldReader& r)
{
	r.Array(16, "nodes");
	r.Word();
	r.Word();
}

void ReadDeadTree(WorldReader& r)
{
	ReadRock(r);
	r.Word("treeInfo");
	r.Float("woodMultiplier");
}

void ReadAnimatedStatic(WorldReader& r)
{
	ReadMultiMapFixed(r);
	r.Array(4);
	r.Word();
}

void ReadHand(WorldReader& r)
{
	r.Word();
	r.Ref();
	for (uint32_t i = 0; i < 4 && !r.Failed(); ++i)
	{
		r.Word();
	}
	r.Bytes(12);
	r.Ref();
	r.Ref();
	r.Byte();
}

void ReadChallenge(WorldReader& r)
{
	r.Half();
	r.Array(1);
	r.Word();
	r.Word();
	r.Word();
	r.Bytes(12);
	r.Bytes(12);
	for (uint32_t i = 0; i < 4 && !r.Failed(); ++i)
	{
		r.Word();
	}
	r.Array(4);
	r.Array(4);
	r.Word();
}

void ReadScriptTimer(WorldReader& r)
{
	ReadPositioned(r);
	r.Word("timerSetTurn");
	r.Word("timerTurns");
}

void ReadWorshipSite(WorldReader& r)
{
	ReadCitadelPart(r);
	r.Word();
	r.Ref();
	r.Ref();
	r.Refs();
	r.Ref();
	r.Word();
	r.Word();
	r.Byte();
	r.Word();
	r.Word();
	r.Word();
	r.Refs();
	r.Refs();
	r.Array(4);
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Byte();
	r.Byte();
	r.Refs();
	r.Ref();
	r.Word();
	r.Ref();
	r.Word();
	r.Word();
}

void ReadShowNeeds(WorldReader& r)
{
	r.Ref();
	r.Refs();
}

void ReadWorshipTotem(WorldReader& r)
{
	ReadCitadelPart(r);
	r.Word();
	r.Ref();
}

void ReadShowNeedsVisuals(WorldReader& r)
{
	ReadObject(r);
	r.Word();
	r.Word();
	r.Ref();
	r.Word();
}

void ReadDance(WorldReader& r)
{
	ReadGroupBehaviour(r);
	r.Word();
	r.Word();
	r.Word();
	r.Ref();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Bytes(16);
	r.Word();
	r.Ref();
}

void ReadGroupBehaviour(WorldReader& r)
{
	ReadPositioned(r);
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Refs();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Refs();
	r.Refs();
	r.Refs();
	r.Word();
	r.Byte();
	r.Word();
	r.Word();
	r.Word();
	r.Refs();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
}

void ReadDanceKeyFrame(WorldReader& r)
{
	r.Word();
	r.Refs();
	r.Word();
}

void ReadDanceKeyAction(WorldReader& r)
{
	r.Array(4);
	r.Word();
	r.Bytes(56);
}

void ReadDanceGroup(WorldReader& r)
{
	r.Ref();
	r.Ref();
	r.Refs();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Bytes(8);
	r.Word();
	r.Word();
	r.Word();
	r.Bytes(16);
	r.Word();
	r.Ref();
	r.Byte();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Bytes(12);
	r.Bytes(12);
	r.Word();
	r.Word();
	r.Bytes(64);
	r.Bytes(12);
	r.Array(4);
	r.Word();
}

void ReadLandscapeVortexIn(WorldReader& r)
{
	ReadLandscapeVortex(r);
	r.Ref();
	r.Ref();
	r.Array(4);
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadFootpathFinder(WorldReader& r)
{
	r.Ref();
	r.Ref();
	r.Ref();
	r.Ref();
	r.Ref();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadSpell(WorldReader& r)
{
	ReadPositioned(r);
	r.Word();
	r.Ref();
	r.Word();
	r.Ref();
	r.Bytes(12);
	r.Bytes(12);
	r.Bytes(12);
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Bytes(60);
	r.Word();
	r.Byte();
	r.Byte();
	r.Ref();
	r.Ref();
	r.Ref();
	r.Ref();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Bytes(12);
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadSpellResource(WorldReader& r)
{
	ReadSpell(r);
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadSpellWater(WorldReader& r)
{
	ReadSpell(r);
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadSpellWithObjects(WorldReader& r)
{
	ReadSpell(r);
	r.Refs();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadSpellFlock(WorldReader& r)
{
	ReadSpellWithObjects(r);
	r.Ref();
	r.Word();
	r.Word();
	r.Bytes(12);
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadSpellFlockFlying(WorldReader& r)
{
	ReadSpellFlock(r);
	r.Word();
	r.Word();
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadSpellFlockGround(WorldReader& r)
{
	ReadSpellFlock(r);
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadSpellForest(WorldReader& r)
{
	ReadSpell(r);
	r.Byte();
	r.Ref();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadSpellShield(WorldReader& r)
{
	ReadSpellWithObjects(r);
	r.Refs();
	r.Ref();
	r.Ref();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadSpellCreature(WorldReader& r)
{
	ReadSpell(r);
	r.Ref();
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadMapShield(WorldReader& r)
{
	ReadFixed(r);
	r.Ref();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadPhysicalShield(WorldReader& r)
{
	ReadMapShield(r);
	r.Word();
	r.Word();
	r.Bytes(52);
	r.Bytes(52);
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Byte();
	r.Byte();
	r.Word();
	r.Ref();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadMagicShield(WorldReader& r)
{
	ReadMapShield(r);
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadMagicTeleport(WorldReader& r)
{
	ReadMobileStatic(r);
	r.Ref();
	r.Ref();
	r.Ref();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadMagicTree(WorldReader& r)
{
	ReadTree(r);
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadMagicFood(WorldReader& r)
{
	ReadPileFood(r);
	r.Ref();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadMagicFireBall(WorldReader& r)
{
	ReadObject(r);
	r.Ref();
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadSpellSeedGraphic(WorldReader& r)
{
	ReadPositioned(r);
	r.Byte();
	r.Word();
	r.Bytes(12);
	r.Byte();
	r.Word();
	r.Ref();
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadSpellSeed(WorldReader& r)
{
	ReadObject(r);
	r.Word();
	r.Byte();
	r.Word();
	r.Ref();
	r.Ref();
	r.Ref();
	r.Ref();
	r.Byte();
	r.Byte();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Byte();
	r.Word();
	r.Word();
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadOneOffSpellSeed(WorldReader& r)
{
	ReadMobileObject(r);
	r.Word();
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadFireEffect(WorldReader& r)
{
	r.Word();
	r.Byte();
	r.Byte();
	r.Byte();
	r.Word();
	r.Word();
	r.Ref();
	r.Ref();
	r.Ref();
	r.Ref();
	r.Byte();
	r.Ref();
	r.Ref();
	r.Refs();
	r.Byte();
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadFireFly(WorldReader& r)
{
	ReadObject(r);
	r.Word();
	r.Bytes(12);
	r.Bytes(12);
	r.Bytes(12);
	r.Bytes(12);
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Byte();
	r.Array(4);
	r.Array(4);
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadTotem(WorldReader& r)
{
	ReadAbode(r);
	r.Bytes(12);
	r.Bytes(12);
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadHanoiBlock(WorldReader& r)
{
	ReadMobileObject(r);
	r.Bytes(12);
	r.Ref();
	r.Ref();
	r.Ref();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadPuzzleTotem(WorldReader& r)
{
	ReadTotem(r);
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadPlannedCitadelPart(WorldReader& r)
{
	ReadPlannedMultiMapFixed(r);
	r.Ref();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadPlannedTownCitadelHeart(WorldReader& r)
{
	ReadPlannedMultiMapFixed(r);
	r.Ref();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadPlannedWorshipSite(WorldReader& r)
{
	ReadPlannedCitadelPart(r);
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadWaterfall(WorldReader& r)
{
	static_cast<void>(r);
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadArenaSpellIcon(WorldReader& r)
{
	ReadFixed(r);
	r.Ref();
	r.Word();
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadBall(WorldReader& r)
{
	ReadMobileObject(r);
	r.Bytes(12);
	r.Bytes(12);
	r.Ref();
	r.Ref();
	r.Ref();
	r.Ref();
	r.Bytes(12);
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadReward(WorldReader& r)
{
	ReadMobileObject(r);
	r.Ref();
	r.Ref();
	r.Ref();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadSpellIcon(WorldReader& r)
{
	ReadMultiMapFixed(r);
	r.Word();
	r.Bytes(12);
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadTownSpellIcon(WorldReader& r)
{
	ReadSpellIcon(r);
	r.Ref();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadCitadelBuildingSite(WorldReader& r)
{
	ReadBuildingSite(r);
	r.Refs();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadScaffold(WorldReader& r)
{
	ReadMobileObject(r);
	r.Ref();
	r.Ref();
	r.Ref();
	r.Word();
	r.Ref();
	r.Word();
	r.Word();
	r.Half();
	r.Ref();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadComputerSeen(WorldReader& r)
{
	r.Ref();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadComputerSpellCast(WorldReader& r)
{
	r.Ref();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadCreed(WorldReader& r)
{
	ReadMobileObject(r);
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadPlannedFeature(WorldReader& r)
{
	ReadPlannedMultiMapFixed(r);
	r.Ref();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadWorshipSpellIcon(WorldReader& r)
{
	ReadSpellIcon(r);
	r.Half();
	r.Ref();
	r.Word();
	r.Word();
	r.Half();
	r.Byte();
	r.Word();
	r.Ref();
	r.Word();
	r.Refs();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadWonder(WorldReader& r)
{
	ReadAbode(r);
	r.Word("wonderPower");
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadSpellWolf(WorldReader& r)
{
	ReadAnimal(r);
	r.Bytes(12);
	r.Bytes(16);
	r.Word();
	r.Bytes(48);
	r.Ref();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadSpellDove(WorldReader& r)
{
	ReadDove(r);
	r.Bytes(48);
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadWhale(WorldReader& r)
{
	ReadMobileObject(r);
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadMissionaryControl(WorldReader& r)
{
	ReadPositioned(r);
	r.Ref();
	r.Ref();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadDrawOffset(WorldReader& r)
{
	r.Half();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadDrawOffsetLT(WorldReader& r)
{
	ReadDrawOffset(r);
	r.Word();
	r.Bytes(12);
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadDrawOffsetDecay(WorldReader& r)
{
	ReadDrawOffset(r);
	r.Bytes(12);
	r.Bytes(12);
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadParticle3DObj(WorldReader& r)
{
	ReadRenderParticle(r);
	r.Persistent();
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadParticleLightMap(WorldReader& r)
{
	ReadRenderParticle(r);
	r.Persistent();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Half();
	r.Bytes(12);
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadRenderParticleMist(WorldReader& r)
{
	ReadRenderParticle(r);
	r.Persistent();
	r.Word();
	r.Byte();
	r.Half();
	r.Bytes(12);
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadParticle3DAnim(WorldReader& r)
{
	ReadRenderParticle(r);
	r.Persistent();
	r.Word();
	r.Word();
	r.Byte();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadParticle3DAnimWithCamera(WorldReader& r)
{
	ReadParticle3DAnim(r);
	r.Persistent();
	r.Byte();
	r.Byte();
	r.Byte();
	r.Word();
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadParticle3DObjAnimTextured(WorldReader& r)
{
	ReadParticle3DObj(r);
	r.Word();
	r.Word();
	r.Byte();
	r.Byte();
	r.Byte();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadRenderParticleCreatureRef(WorldReader& r)
{
	ReadRenderParticle(r);
	r.Word();
	r.Ref();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadRenderParticleGameObject(WorldReader& r)
{
	ReadRenderParticle(r);
	r.Word();
	r.Ref();
	r.Ref();
	r.Bytes(48);
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadParticleChainJoint(WorldReader& r)
{
	ReadRenderParticle(r);
	r.Ref();
	r.Word();
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadParticlePlayerSymbol(WorldReader& r)
{
	ReadRenderParticle(r);
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadSpellPointInf(WorldReader& r)
{
	r.Word();
	r.Bytes(12);
	r.Bytes(12);
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadURPlasmaInf(WorldReader& r)
{
	r.Bytes(12);
	r.Bytes(12);
	r.Bytes(12);
	r.Bytes(12);
	r.Bytes(12);
	r.Word();
	r.Word();
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadBaseAtomModifierData(WorldReader& r)
{
	r.Persistent();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataURVolFXOnObject(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Word();
	r.Ref();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataURPlasma(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Bytes(12);
	r.Bytes(12);
	r.Bytes(12);
	r.Bytes(12);
	r.Word();
	r.Word();
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataURBeliefSprite(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Bytes(12);
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataURSimpleBeam(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Word();
	r.Ref();
	r.Bytes(12);
	r.Bytes(12);
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataURManaPathNew(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Bytes(12);
	r.Bytes(12);
	r.Bytes(12);
	r.Bytes(12);
	r.Bytes(12);
	r.Bytes(12);
	r.Bytes(12);
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadCollectionDataURCreatureSpellGeneric(WorldReader& r)
{
	ReadBaseCollectionModifierData(r);
	r.Byte();
	r.Bytes(12);
	r.Bytes(12);
	r.Byte();
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataURCreatureSpellGeneric(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataUROrientSpriteWithVelocity(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Byte();
	r.Bytes(12);
	r.Word();
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadCollectionDataURAtomsAtEPTarget(WorldReader& r)
{
	ReadBaseCollectionModifierData(r);
	r.Word();
	r.Ref();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadCollectionDataURCreatureSpellCompassion(WorldReader& r)
{
	ReadBaseCollectionModifierData(r);
	r.Byte();
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadCollectionDataURCreatureSpell(WorldReader& r)
{
	ReadBaseCollectionModifierData(r);
	r.Byte();
	r.Word();
	r.Ref();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadCollectionDataURCreatureSpellItch(WorldReader& r)
{
	ReadBaseCollectionModifierData(r);
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadCollectionDataURCreatureSpellFreeze(WorldReader& r)
{
	ReadBaseCollectionModifierData(r);
	r.Byte();
	r.Byte();
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataURVortexAttract(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Word();
	r.Bytes(12);
	r.Byte();
	r.Byte();
	r.Byte();
	r.Word();
	r.Ref();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadVortexObjectInfo(WorldReader& r)
{
	r.Bytes(12);
	r.Bytes(48);
	r.Ref();
	r.Byte();
	r.Byte();
	r.Ref();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataURSphereSurfaceTracer(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Byte();
	r.Word();
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataCheckShieldDeflections(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Byte();
	r.Bytes(12);
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataURForestPath(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Byte();
	r.Word();
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataURExplosion(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadCollectionDataURFlocking(WorldReader& r)
{
	ReadBaseCollectionModifierData(r);
	r.Byte();
	r.Bytes(12);
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadCollectionDataURCloudMoverNew(WorldReader& r)
{
	ReadBaseCollectionModifierData(r);
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadCollectionDataURCloudGather(WorldReader& r)
{
	ReadBaseCollectionModifierData(r);
	r.Bytes(12);
	r.Word();
	r.Word();
	r.Byte();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Ref();
	r.Ref();
	r.Byte();
	r.Byte();
	r.Byte();
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataURCloudGather(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Word();
	r.Word();
	r.Word();
	r.Byte();
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadFlyingAtomDataURTornado(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Word();
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadCollectionDataURTornado(WorldReader& r)
{
	ReadBaseCollectionModifierData(r);
	r.Byte();
	r.Bytes(12);
	r.Bytes(12);
	r.Bytes(12);
	r.Bytes(12);
	r.Bytes(12);
	r.Word();
	r.Word();
	r.Byte();
	r.Word();
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadCollectionDataEmitterRuleLightningSprite(WorldReader& r)
{
	ReadBaseCollectionModifierData(r);
	r.Word();
	r.Byte();
	r.Word();
	r.Word();
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataARFadeOutOnceConditionTrue(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Byte();
	r.Word();
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataAttatchFireBallToAtom(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Byte();
	r.Word();
	r.Ref();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataAddSoundToAtom(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataRemoveSoundFromAtom(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataRippleUpdateRuleGravityWithFloor(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Bytes(12);
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataURBankedTurning(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataUpdateRuleShieldSpark(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Bytes(48);
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataURVapourEndEffect(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataAddSubCollectionsToAtom(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataCreateNewBaseAtom(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadParentAtomDataERGlintsOnTarget(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Byte();
	r.Word();
	r.Word();
	r.Ref();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataERGlintsOnTarget(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataUROrientSpriteWithRandomAngle(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Byte();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataURSideSpin(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Word();
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataURInitialSpin(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Word();
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataRemoveRuleAfterConditionTrue(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Byte();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataUROrientWithVelocity(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Bytes(12);
	r.Word();
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataURFollowTargets(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Word();
	r.Ref();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadCollectionDataURAddDefensiveSphere(WorldReader& r)
{
	ReadBaseCollectionModifierData(r);
	r.Byte();
	r.Ref();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadCollectionDataURWillowWisp(WorldReader& r)
{
	ReadBaseCollectionModifierData(r);
	r.Bytes(12);
	r.Word();
	r.Word();
	r.Byte();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadCollectionDataERBurstFromParentAtom(WorldReader& r)
{
	ReadBaseCollectionModifierData(r);
	r.Byte();
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadCollectionDataEREmitFromParentAtom(WorldReader& r)
{
	ReadBaseCollectionModifierData(r);
	r.Byte();
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataURHealSpellChakra(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Word();
	r.Ref();
	r.Byte();
	r.Byte();
	r.Byte();
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadCollectionDataLightningForkFlicker(WorldReader& r)
{
	ReadBaseCollectionModifierData(r);
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadCollectionDataURHandSprinkle(WorldReader& r)
{
	ReadBaseCollectionModifierData(r);
	r.Byte();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadPlayerSubAction(WorldReader& r)
{
	r.Word();
	r.Refs();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadStreetLight(WorldReader& r)
{
	ReadPositioned(r);
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAudio(WorldReader& r)
{
	r.Refs();
	r.Word();
	r.Word();
	r.Ref();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadTownArtifact(WorldReader& r)
{
	r.Ref();
	r.Ref();
	r.Ref();
	r.Word();
	r.Ref();
	r.Ref();
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadPuzzleGrain(WorldReader& r)
{
	ReadPileFood(r);
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadPuzzleMobileObject(WorldReader& r)
{
	ReadMobileObject(r);
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadThingMusicInfo(WorldReader& r)
{
	r.Word();
	r.Ref();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
	r.Bytes(12);
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadSpellDispenser(WorldReader& r)
{
	ReadAbode(r);
	r.Ref();
	r.Word();
	r.Word();
	r.Word();
	r.Word();
}

// No save file of the tests has this type. This layout comes only from the game's save code.
void ReadAtomDataURLightSheetOnObject(WorldReader& r)
{
	ReadBaseAtomModifierData(r);
	r.Word();
}

constexpr std::array k_Readers = {
    ClassReader {0x001, &ReadFootpath},
    ClassReader {0x002, &ReadFootpathLink},
    ClassReader {0x003, &ReadFootpathNode},
    ClassReader {0x005, &ReadFootpathFinder},
    ClassReader {0x006, &ReadVillager},
    ClassReader {0x007, &ReadAbode},
    ClassReader {0x008, &ReadStoragePit},
    ClassReader {0x009, &ReadSpell},
    ClassReader {0x00a, &ReadSpellResource},
    ClassReader {0x00c, &ReadSpellWater},
    ClassReader {0x00d, &ReadSpellFlockFlying},
    ClassReader {0x00e, &ReadSpellFlockGround},
    ClassReader {0x00f, &ReadSpellForest},
    ClassReader {0x010, &ReadSpell},
    ClassReader {0x011, &ReadSpellShield},
    ClassReader {0x012, &ReadSpell},
    ClassReader {0x013, &ReadSpellCreature},
    ClassReader {0x015, &ReadSpellWithObjects},
    ClassReader {0x017, &ReadPhysicalShield},
    ClassReader {0x018, &ReadMagicShield},
    ClassReader {0x019, &ReadMagicTeleport},
    ClassReader {0x01a, &ReadLandscapeVortex},
    ClassReader {0x01b, &ReadLandscapeVortexIn},
    ClassReader {0x01d, &ReadMagicTree},
    ClassReader {0x01e, &ReadMagicFood},
    ClassReader {0x01f, &ReadMagicWood},
    ClassReader {0x020, &ReadMagicFireBall},
    ClassReader {0x023, &ReadSpellSeedGraphic},
    ClassReader {0x024, &ReadSpellSeed},
    ClassReader {0x025, &ReadOneOffSpellSeed},
    ClassReader {0x026, &ReadReaction},
    ClassReader {0x027, &ReadDance},
    ClassReader {0x028, &ReadTown},
    ClassReader {0x029, &ReadFireEffect},
    ClassReader {0x02a, &ReadFireFly},
    ClassReader {0x02b, &ReadTotem},
    ClassReader {0x02d, &ReadAnimal},
    ClassReader {0x02e, &ReadAnimal},
    ClassReader {0x02f, &ReadAnimal},
    ClassReader {0x030, &ReadHanoiBlock},
    ClassReader {0x031, &ReadPuzzleTotem},
    ClassReader {0x032, &ReadField},
    ClassReader {0x035, &ReadCitadel},
    ClassReader {0x036, &ReadCitadelPart},
    ClassReader {0x037, &ReadPlannedCitadelPart},
    ClassReader {0x038, &ReadPlannedMultiMapFixed},
    ClassReader {0x039, &ReadPlannedTownCitadelHeart},
    ClassReader {0x03a, &ReadCitadelHeart},
    ClassReader {0x03b, &ReadPlannedWorshipSite},
    ClassReader {0x03c, &ReadWorshipSite},
    ClassReader {0x03d, &ReadShowNeedsVisuals},
    ClassReader {0x03e, &ReadShowNeeds},
    ClassReader {0x03f, &ReadScriptHighlight},
    ClassReader {0x040, &ReadWaterfall},
    ClassReader {0x041, &ReadArena},
    ClassReader {0x042, &ReadArenaSpellIcon},
    ClassReader {0x044, &ReadBall},
    ClassReader {0x046, &ReadReward},
    ClassReader {0x047, &ReadStream},
    ClassReader {0x048, &ReadMist},
    ClassReader {0x049, &ReadInfluenceRing},
    ClassReader {0x04c, &ReadTownDesireFlags},
    ClassReader {0x04d, &ReadBigForest},
    ClassReader {0x04e, &ReadForest},
    ClassReader {0x04f, &ReadTree},
    ClassReader {0x050, &ReadAbode},
    ClassReader {0x051, &ReadGraveyard},
    ClassReader {0x053, &ReadMobileObject},
    ClassReader {0x054, &ReadMobileStatic},
    ClassReader {0x055, &ReadTownSpellIcon},
    ClassReader {0x059, &ReadPileFood},
    ClassReader {0x05a, &ReadPileResource},
    ClassReader {0x05b, &ReadFishFarm},
    ClassReader {0x05c, &ReadStandardBuildingSite},
    ClassReader {0x05d, &ReadCitadelBuildingSite},
    ClassReader {0x05e, &ReadBuildingSite},
    ClassReader {0x05f, &ReadScaffold},
    ClassReader {0x060, &ReadTownCentre},
    ClassReader {0x061, &ReadPlannedAbode},
    ClassReader {0x062, &ReadPlannedAbode},
    ClassReader {0x063, &ReadPlayer},
    ClassReader {0x064, &ReadInterface},
    ClassReader {0x065, &ReadInterfaceStatus},
    ClassReader {0x066, &ReadComputerPlayer},
    ClassReader {0x067, &ReadComputerSeen},
    ClassReader {0x068, &ReadComputerSpellCast},
    ClassReader {0x069, &ReadCreature},
    ClassReader {0x06a, &ReadGame},
    ClassReader {0x06b, &ReadMobileObject},
    ClassReader {0x06c, &ReadMobileObject},
    ClassReader {0x06d, &ReadCreed},
    ClassReader {0x06e, &ReadParticleContainer},
    ClassReader {0x06f, &ReadRock},
    ClassReader {0x070, &ReadDeadTree},
    ClassReader {0x071, &ReadDeadTree},
    ClassReader {0x072, &ReadBonfire},
    ClassReader {0x073, &ReadPlannedFeature},
    ClassReader {0x074, &ReadMultiMapFixed},
    ClassReader {0x075, &ReadAnimatedStatic},
    ClassReader {0x076, &ReadSpecialVillager},
    ClassReader {0x077, &ReadCamera},
    ClassReader {0x078, &ReadWorshipSpellIcon},
    ClassReader {0x079, &ReadDanceKeyAction},
    ClassReader {0x07a, &ReadDanceKeyFrame},
    ClassReader {0x07b, &ReadDanceGroup},
    ClassReader {0x07c, &ReadScriptMarker},
    ClassReader {0x07d, &ReadScriptTimer},
    ClassReader {0x07e, &ReadFlock},
    ClassReader {0x07f, &ReadTotemStatue},
    ClassReader {0x080, &ReadWonder},
    ClassReader {0x081, &ReadSpellIcon},
    ClassReader {0x082, &ReadAnimal},
    ClassReader {0x083, &ReadAnimal},
    ClassReader {0x084, &ReadAnimal},
    ClassReader {0x085, &ReadAnimal},
    ClassReader {0x086, &ReadAnimal},
    ClassReader {0x087, &ReadAnimal},
    ClassReader {0x088, &ReadAnimal},
    ClassReader {0x089, &ReadAnimal},
    ClassReader {0x08a, &ReadAnimal},
    ClassReader {0x08b, &ReadAnimal},
    ClassReader {0x08c, &ReadAnimal},
    ClassReader {0x08d, &ReadSpellWolf},
    ClassReader {0x08e, &ReadDove},
    ClassReader {0x08f, &ReadDove},
    ClassReader {0x090, &ReadSpellDove},
    ClassReader {0x091, &ReadDove},
    ClassReader {0x092, &ReadDove},
    ClassReader {0x093, &ReadDove},
    ClassReader {0x094, &ReadDove},
    ClassReader {0x095, &ReadSpellDove},
    ClassReader {0x096, &ReadDove},
    ClassReader {0x097, &ReadWhale},
    ClassReader {0x098, &ReadWorshipTotem},
    ClassReader {0x099, &ReadDataPath},
    ClassReader {0x09a, &ReadAnimal},
    ClassReader {0x09b, &ReadAnimal},
    ClassReader {0x09c, &ReadAnimal},
    ClassReader {0x09d, &ReadAnimal},
    ClassReader {0x09e, &ReadMissionaryControl},
    ClassReader {0x09f, &ReadGameStats},
    ClassReader {0x0a1, &ReadPositioned},
    ClassReader {0x0a2, &ReadAtomCore},
    ClassReader {0x0a3, &ReadAtomCollection},
    ClassReader {0x0a4, &ReadPSysManager},
    ClassReader {0x0a5, &ReadJPSysInterface},
    ClassReader {0x0a6, &ReadDrawOffsetLT},
    ClassReader {0x0a7, &ReadDrawOffsetDecay},
    ClassReader {0x0a8, &ReadParticle3DPnt},
    ClassReader {0x0a9, &ReadParticle3DObj},
    ClassReader {0x0aa, &ReadParticleLightMap},
    ClassReader {0x0ab, &ReadRenderParticleMist},
    ClassReader {0x0ac, &ReadParticle3DAnim},
    ClassReader {0x0ad, &ReadParticle3DAnimWithCamera},
    ClassReader {0x0ae, &ReadParticle3DObjAnimTextured},
    ClassReader {0x0af, &ReadRenderParticleCreatureRef},
    ClassReader {0x0b0, &ReadRenderParticleGameObjectRef},
    ClassReader {0x0b1, &ReadRenderParticleGoldenShower},
    ClassReader {0x0b2, &ReadRenderParticleGameObject},
    ClassReader {0x0b3, &ReadParticleChainJoint},
    ClassReader {0x0b4, &ReadParticle3DSprite},
    ClassReader {0x0b5, &ReadParticlePlayerSymbol},
    ClassReader {0x0b8, &ReadSpellPointInf},
    ClassReader {0x0ba, &ReadURPlasmaInf},
    ClassReader {0x0bb, &ReadAtomDataURVolFXOnObject},
    ClassReader {0x0bd, &ReadAtomDataURPlasma},
    ClassReader {0x0be, &ReadAtomDataURBeliefSprite},
    ClassReader {0x0bf, &ReadAtomDataURSimpleBeam},
    ClassReader {0x0c0, &ReadAtomDataURManaPathNew},
    ClassReader {0x0c1, &ReadCollectionDataURCreatureSpellGeneric},
    ClassReader {0x0c2, &ReadAtomDataURCreatureSpellGeneric},
    ClassReader {0x0c3, &ReadAtomDataUROrientSpriteWithVelocity},
    ClassReader {0x0c4, &ReadCollectionDataURAtomsAtEPTarget},
    ClassReader {0x0c5, &ReadCollectionDataURCreatureSpellCompassion},
    ClassReader {0x0c6, &ReadCollectionDataURCreatureSpell},
    ClassReader {0x0c7, &ReadCollectionDataURCreatureSpellItch},
    ClassReader {0x0c8, &ReadCollectionDataURCreatureSpellFreeze},
    ClassReader {0x0c9, &ReadAtomDataURVortexAttract},
    ClassReader {0x0ca, &ReadVortexObjectInfo},
    ClassReader {0x0cb, &ReadAtomDataURSphereSurfaceTracer},
    ClassReader {0x0cc, &ReadAtomDataCheckShieldDeflections},
    ClassReader {0x0cd, &ReadAtomDataURForestPath},
    ClassReader {0x0ce, &ReadAtomDataURExplosion},
    ClassReader {0x0d0, &ReadCollectionDataURFlocking},
    ClassReader {0x0d1, &ReadCollectionDataURCloudMoverNew},
    ClassReader {0x0d2, &ReadCollectionDataURCloudGather},
    ClassReader {0x0d3, &ReadAtomDataURCloudGather},
    ClassReader {0x0d4, &ReadFlyingAtomDataURTornado},
    ClassReader {0x0d7, &ReadCollectionDataURTornado},
    ClassReader {0x0d9, &ReadCollectionDataEmitterRuleLightningSprite},
    ClassReader {0x0da, &ReadAtomDataARFadeOutOnceConditionTrue},
    ClassReader {0x0db, &ReadAtomDataAttatchFireBallToAtom},
    ClassReader {0x0dc, &ReadAtomDataAddSoundToAtom},
    ClassReader {0x0dd, &ReadAtomDataRemoveSoundFromAtom},
    ClassReader {0x0de, &ReadAtomDataRippleUpdateRuleGravityWithFloor},
    ClassReader {0x0df, &ReadAtomDataURBankedTurning},
    ClassReader {0x0e0, &ReadAtomDataUpdateRuleShieldSpark},
    ClassReader {0x0e1, &ReadAtomDataURVapourEndEffect},
    ClassReader {0x0e2, &ReadAtomDataAddSubCollectionsToAtom},
    ClassReader {0x0e3, &ReadAtomDataCreateNewBaseAtom},
    ClassReader {0x0e4, &ReadParentAtomDataERGlintsOnTarget},
    ClassReader {0x0e5, &ReadAtomDataERGlintsOnTarget},
    ClassReader {0x0e6, &ReadAtomDataUROrientSpriteWithRandomAngle},
    ClassReader {0x0e7, &ReadAtomDataURSideSpin},
    ClassReader {0x0e8, &ReadAtomDataURInitialSpin},
    ClassReader {0x0e9, &ReadAtomDataRemoveRuleAfterConditionTrue},
    ClassReader {0x0ea, &ReadAtomDataUROrientWithVelocity},
    ClassReader {0x0eb, &ReadAtomDataURFollowTargets},
    ClassReader {0x0ec, &ReadCollectionDataURAddDefensiveSphere},
    ClassReader {0x0ed, &ReadCollectionDataEmitterRule},
    ClassReader {0x0ee, &ReadCollectionDataURWillowWisp},
    ClassReader {0x0ef, &ReadCollectionDataERBurstFromParentAtom},
    ClassReader {0x0f0, &ReadCollectionDataEREmitFromParentAtom},
    ClassReader {0x0f2, &ReadAtomDataURHealSpellChakra},
    ClassReader {0x0f4, &ReadCollectionDataLightningForkFlicker},
    ClassReader {0x0f5, &ReadCollectionDataURHandSprinkle},
    ClassReader {0x0f6, &ReadObject},
    ClassReader {0x0f9, &ReadPlayerActionState},
    ClassReader {0x0fa, &ReadPlayerSubAction},
    ClassReader {0x0fc, &ReadClimate},
    ClassReader {0x0fe, &ReadStreetLantern},
    ClassReader {0x0ff, &ReadStreetLight},
    ClassReader {0x101, &ReadAudio},
    ClassReader {0x102, &ReadLeashStatus},
    ClassReader {0x103, &ReadTownArtifact},
    ClassReader {0x105, &ReadDataForScriptRemind},
    ClassReader {0x106, &ReadVortexSave},
    ClassReader {0x107, &ReadPuzzleGrain},
    ClassReader {0x108, &ReadPuzzleMobileObject},
    ClassReader {0x109, &ReadThingMusicInfo},
    ClassReader {0x10a, &ReadMultiMapFixed},
    ClassReader {0x10b, &ReadSpellDispenser},
    ClassReader {0x10d, &ReadComputerAttitude},
    ClassReader {0x10e, &ReadAtomDataURLightSheetOnObject},
    ClassReader {0x110, &ReadObject},
};
