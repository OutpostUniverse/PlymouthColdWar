// Include important header files needed to interface with Outpost 2
#include "Outpost2DLL/Outpost2DLL.h"
// Include header files to make it easier to build levels
#include "OP2Helper/OP2Helper.h"


// Forward declare all functions
void SetDifficultyMultiplier();
void SetStartingResearch();
extern void ShowBriefing();
extern void SetupObjects();
void SetupAIMines();
void SetupAIFactories();
void SetupAIDefense();

ExportLevelDetails("Plymouth Cold War", "eden03.map", "MULTITEK.TXT", Colony, 2);

// We must save all triggers and groups. The game does not call InitProc when the game is loaded.
// This way, we can recreate all the triggers/groups.
struct ScriptGlobal
{
	Trigger waitTrig;
	Trigger killTrig;
	Trigger aiStateTrig;
	Trigger hasSFTrig;
	Trigger failTrig;
	Trigger disasterTrig;
	MiningGroup aiMineGrp[3]; // AI mining
	BuildingGroup aiBuildGrp; // AI SF / VF
	FightGroup aiDefGrp[7]; // AI defense
	FightGroup aiMassGrp; // Group used to mass up the AI attack units
	FightGroup aiAttackGrp; // Group used to attack the player base
	int aiCount; // this counter is incremented by AIStateChange each run
	bool usePanthers;
	bool useTigers;
	int diffMultiplier; // difficulty multiplier used to change amounts based on diff. level
};
ScriptGlobal saveData;

// List of songs to play
SongIds songs[] = {
	songEden11,
	songEP41,
	songEP43,
	songEP52,
	songEP51,
	songEP62,
	songEP61,
	songEP63
};


Export int InitProc()
{
	// Show skinned briefing dialog box
	ShowBriefing();

	TethysGame::ForceMoraleGood(0);
	TethysGame::ForceMoraleGood(1);

	Player[0].GoEden();
	Player[0].CenterViewOn(38 + X_, 45 + Y_);

	SetDifficultyMultiplier();
	SetStartingResearch();

	Player[0].SetOre(2500 * saveData.diffMultiplier / 10);
	Player[0].SetWorkers(15 * saveData.diffMultiplier / 10);
	Player[0].SetScientists(7 * saveData.diffMultiplier / 10);
	Player[0].SetFoodStored(1200 * saveData.diffMultiplier / 10);

	Player[1].GoPlymouth();
	Player[1].GoAI();
	Player[1].SetOre(2000);
	Player[1].SetTechLevel(12);

	// Setup human player ore
	TethysGame::CreateBeacon(mapMiningBeacon, 33+31, 32-1, 0, 2, 0);
	TethysGame::CreateBeacon(mapMiningBeacon, 59+31, 27-1, 0, Player[0].Difficulty(), -1);
	TethysGame::CreateBeacon(mapMiningBeacon, 20+31, 56-1, 0, -1, -1);

	TethysGame::CreateBeacon(mapMiningBeacon, 31+31, 11-1, 1, Player[0].Difficulty(), -1);
	TethysGame::CreateBeacon(mapMiningBeacon, 83+31, 45-1, 1, -1, -1);

	SetupObjects(); // create OP2Mapper units

	SetupAIFactories();
	SetupAIMines();
	SetupAIDefense();

	// Set reinforce info
	saveData.aiBuildGrp.RecordVehReinforceGroup(saveData.aiDefGrp[0], 1);
	saveData.aiBuildGrp.RecordVehReinforceGroup(saveData.aiDefGrp[1], 5000); // Very high priority; it's the lab
	saveData.aiBuildGrp.RecordVehReinforceGroup(saveData.aiDefGrp[2], 5000); // Also high; the CC
	saveData.aiBuildGrp.RecordVehReinforceGroup(saveData.aiDefGrp[3], 5000); // Factories
	saveData.aiBuildGrp.RecordVehReinforceGroup(saveData.aiDefGrp[4], 1);
	saveData.aiBuildGrp.RecordVehReinforceGroup(saveData.aiDefGrp[5], 1);
	saveData.aiBuildGrp.RecordVehReinforceGroup(saveData.aiDefGrp[6], 1);

	for (int i = 0; i < 30; ++i) {
		Unit unit;
		TethysGame::CreateUnit(unit, map_id::mapLynx, LOCATION(50 + X_, 15 + Y_), 0, mapThorsHammer, 0);
	}

	// Turn on vehicle lights
	PlayerVehicleEnum vehEnum(0);
	Unit veh;
	while (vehEnum.GetNext(veh)) {
		veh.DoSetLights(1);
	}

	vehEnum = PlayerVehicleEnum(1);
	while (vehEnum.GetNext(veh)) {
		veh.DoSetLights(1);
	}

	// Map lighting settings
	TethysGame::SetDaylightMoves(1);
	TethysGame::SetDaylightEverywhere(0);

	// Setup trigger to send reinforcements once the player begins to accumulate ore
	saveData.waitTrig = CreateResourceTrigger(1, 1, resCommonOre, Player[0].Ore(), 0, cmpGreater, "InitialReinforce");
	CreateVictoryCondition(1, 0, saveData.waitTrig, "Establish a mining operation.");

	saveData.killTrig = CreateCountTrigger(1, 0, 1, mapAdvancedLab, mapAny, 0, cmpEqual, "NoResponseToTrigger");
	CreateVictoryCondition(1, 0, saveData.killTrig, "Destroy the Plymouth Advanced Lab.");

	saveData.failTrig = CreateOperationalTrigger(1, 1, 0, mapCommandCenter, 0, cmpEqual, "NoResponseToTrigger");
	CreateFailureCondition(1, 0, saveData.failTrig, "");

	// Start the offensive AI groups going
	saveData.aiMassGrp = CreateFightGroup(Player[1]);
	saveData.aiMassGrp.SetRect(MAP_RECT(70+31, 15-1, 95+31, 50-1));
	saveData.aiBuildGrp.RecordVehReinforceGroup(saveData.aiMassGrp, 3 - saveData.diffMultiplier * 15 / 100);
	saveData.aiMassGrp.SetTargCount(mapLynx, mapMicrowave, 1); // Set a modest one to start

	saveData.aiAttackGrp = CreateFightGroup(Player[1]);
	saveData.aiAttackGrp.SetRect(MAP_RECT(20+31, 10-1, 80+31, 60-1));

	// Start the periodic changes slowly.. once the player has a mine, start going faster ;)
	// and do some other processing, like teching up, etc
	saveData.aiCount = 0;
	saveData.usePanthers = false;
	saveData.useTigers = false;
	saveData.aiStateTrig = CreateTimeTrigger(1, 0, 1200 * saveData.diffMultiplier / 10, 1800 * saveData.diffMultiplier / 10, "AIStateChange");

	// Wait for the user to have capability to build weapons before increasing the chances of attack
	saveData.hasSFTrig = CreateOperationalTrigger(1, 1, 0, mapStructureFactory, 1, cmpGreaterEqual, "NoResponseToTrigger");

	// Set up some disasters! ;)
	saveData.disasterTrig = CreateTimeTrigger(1, 0, 1500 * saveData.diffMultiplier / 10, 2500 * saveData.diffMultiplier / 10, "Disasters");

	// Give some faster music
	TethysGame::SetMusicPlayList(6, 3, songs);
	return 1; // return 1 if OK; 0 on failure
}

void SetDifficultyMultiplier()
{
	switch (Player[0].Difficulty())
	{
	case DiffEasy: {
		saveData.diffMultiplier = 13;
		return; }
	case DiffNormal: {
		saveData.diffMultiplier = 10;
		return; }
	case DiffHard: {
		saveData.diffMultiplier = 7;
		return; }
	}
}

void SetStartingResearch()
{
	Player[0].MarkResearchComplete(techResearchTrainingPrograms);
	Player[0].MarkResearchComplete(techOffspringEnhancement);
	Player[0].MarkResearchComplete(techCyberneticTeleoperation);

	if (Player[0].Difficulty() != DiffHard)
	{
		Player[0].MarkResearchComplete(techLargeScaleOpticalResonators);
		Player[0].MarkResearchComplete(techHighTemperatureSuperconductivity);
		Player[0].MarkResearchComplete(techMobileWeaponsPlatform);
		Player[0].MarkResearchComplete(techMetallogeny);
	}

	if (Player[0].Difficulty() == DiffEasy)
	{
		Player[0].MarkResearchComplete(techExplosiveCharges);
		Player[0].MarkResearchComplete(techScoutClassDriveTrainRefit);
	}
}

Export void AIProc()
{
}

ExportSaveLoadData(saveData);

Export void NoResponseToTrigger()
{
}

Export void AIStateChange()
{
	map_id curType;
	// Increment a counter
	saveData.aiCount++;

	// Should AI attack?
	if (TethysGame::GetRand(saveData.hasSFTrig.HasFired(0) ? 2 : 3) == 0)
	{
		saveData.aiAttackGrp.TakeAllUnits(saveData.aiMassGrp);
		saveData.aiAttackGrp.DoAttackEnemy();
	}

	// Adjust what types of units to make based on the player's military strength
	if (Player[0].GetTotalPlayerStrength() >= 20 * saveData.diffMultiplier / 10)
	{
		saveData.useTigers = true;
		saveData.aiCount = 0;
	}
	else if (Player[0].GetTotalPlayerStrength() >= 10 * saveData.diffMultiplier / 10)
	{
		saveData.usePanthers = true;
		saveData.aiCount = 0;
	}

	// Decide what type to create
	curType = (saveData.useTigers ? mapTiger : (saveData.usePanthers ? mapPanther : mapLynx));

	// Check the count and increase the AI strength as needed
	if (saveData.aiCount == 10 * saveData.diffMultiplier / 10)
	{
		// Starflare time
		saveData.aiMassGrp.SetTargCount(curType, mapStarflare, 3 - saveData.diffMultiplier * 15 / 10);
	}
	else if (saveData.aiCount == 20 * saveData.diffMultiplier / 10)
	{
		// Stickyfoam time
		saveData.aiMassGrp.SetTargCount(curType, mapStickyfoam, 3 - saveData.diffMultiplier * 15 / 10);
	}
	else if (saveData.aiCount == 30 * saveData.diffMultiplier / 10)
	{
		// EMP time
		saveData.aiMassGrp.SetTargCount(curType, mapEMP, 3 - saveData.diffMultiplier * 15 / 10);
	}
	else if (saveData.aiCount == 45 * saveData.diffMultiplier / 10)
	{
		// RPG time
		saveData.aiMassGrp.SetTargCount(curType, mapRPG, 3 - saveData.diffMultiplier * 15 / 10);
	}
	else if (saveData.aiCount == 70 * saveData.diffMultiplier / 10)
	{
		// ESG time
		saveData.aiMassGrp.SetTargCount(curType, mapESG, 3 - saveData.diffMultiplier * 15 / 10);
	}
	else if (saveData.aiCount == 75 * saveData.diffMultiplier / 10)
	{
		// Nova time
		saveData.aiMassGrp.SetTargCount(curType, mapSupernova, 3 - saveData.diffMultiplier * 15 / 10);
	}
}

Export void InitialReinforce()
{
	// Give the player some stuff to expand their base with
	Unit unit;
	TethysGame::CreateUnit(unit, mapConVec, LOCATION(0+31, 44-1), 0, mapStructureFactory, 0);
	unit.DoSetLights(1);
	unit.DoMove(LOCATION(34+31, 34-1));

	if (Player[0].Difficulty() < 2)
	{
		TethysGame::CreateUnit(unit, mapEarthworker, LOCATION(0+31, 45-1), 0, mapNone, 0);
		unit.DoSetLights(1);
		unit.DoMove(LOCATION(36+31, 34-1));
		if (Player[0].Difficulty() < 1)
		{
			TethysGame::CreateUnit(unit, mapConVec, LOCATION(0+31, 46-1), 0, mapStandardLab, 0);
			unit.DoSetLights(1);
			unit.DoMove(LOCATION(35+31, 34-1));
		}
	}
	if (Player[0].Difficulty() > 0) { // unless the player has it on easy mode let the colonists revolt ;P
		TethysGame::FreeMoraleLevel(0);
	}

    // Start the attacks faster
    saveData.aiStateTrig.Destroy();
    saveData.aiStateTrig = CreateTimeTrigger(1, 0, 800 * saveData.diffMultiplier / 10, 1400 * saveData.diffMultiplier / 10, "AIStateChange");

	TethysGame::AddMessage(unit, "Reinforcements have arrived", 0, sndSavnt205);
}

Export void Disasters()
{
	// Set a disaster. This uses a weighted random. Picking a random number 0 to 100:
	// 0-19 = no action
	// 20-69 = meteor
	// 70-84 = quake
	// 85-94 = electrical storm
	// 95-100 = vortex
	// Note: the X coordinate in the GetRand calls is only 112, because we wouldn't want
	// a vortex to take out the lab or any other AI vitals for us. ;)

	int randNum = TethysGame::GetRand(100);

	if (20*saveData.diffMultiplier / 10 <= randNum && randNum < 70*saveData.diffMultiplier / 10)
	{
		// Meteor
		TethysGame::SetMeteor(TethysGame::GetRand(112)+31, TethysGame::GetRand(127)-1, TethysGame::GetRand(5-(5*saveData.diffMultiplier/10))+1);
	}
	else if (70*saveData.diffMultiplier / 10 <= randNum && randNum < 85*saveData.diffMultiplier / 10)
	{
		// Quake
		TethysGame::SetEarthquake(TethysGame::GetRand(112)+31, TethysGame::GetRand(127)-1, TethysGame::GetRand(3-(3*saveData.diffMultiplier/10))+1);
	}
	else if (85*saveData.diffMultiplier / 10 <= randNum && randNum < 95*saveData.diffMultiplier / 10)
	{
		// Lightning
		TethysGame::SetLightning(TethysGame::GetRand(112)+31, TethysGame::GetRand(127)-1, TethysGame::GetRand(20-saveData.diffMultiplier)+5, TethysGame::GetRand(112)+31, TethysGame::GetRand(127)-1);
	}
	else if (95*saveData.diffMultiplier / 10 <= randNum)
	{
		// Tornado
		TethysGame::SetTornado(TethysGame::GetRand(112)+31, TethysGame::GetRand(127)-1, TethysGame::GetRand(20-saveData.diffMultiplier)+5, TethysGame::GetRand(112)+31, TethysGame::GetRand(127)-1, 1);
	}
}

void SetupAIMines()
{
	// Setup AI mining units and routes
	Unit mine, smelter, truck;
	// Common Group 1
	saveData.aiMineGrp[0] = CreateMiningGroup(Player[1]);
	TethysGame::CreateUnit(mine, mapCommonOreMine, LOCATION(92+31, 53-1), 1, mapNone, 0);
	saveData.aiMineGrp[0].TakeUnit(mine);
	TethysGame::CreateUnit(smelter, mapCommonOreSmelter, LOCATION(107+31, 43-1), 1, mapNone, 0);
	saveData.aiMineGrp[0].TakeUnit(smelter);
	TethysGame::CreateUnit(truck, mapCargoTruck, LOCATION(102+31, 38-1), 1, mapNone, 0);
	saveData.aiMineGrp[0].TakeUnit(truck);
	TethysGame::CreateUnit(truck, mapCargoTruck, LOCATION(102+31, 40-1), 1, mapNone, 0);
	saveData.aiMineGrp[0].TakeUnit(truck);
	TethysGame::CreateUnit(truck, mapCargoTruck, LOCATION(101+31, 39-1), 1, mapNone, 0);
	saveData.aiMineGrp[0].TakeUnit(truck);
	saveData.aiMineGrp[0].Setup(mine, smelter, MAP_RECT(107-3+31, 43-2-1, 107+2+31, 43+2-1));

	// Common Group 2
	saveData.aiMineGrp[1] = CreateMiningGroup(Player[1]);
	TethysGame::CreateUnit(mine, mapCommonOreMine, LOCATION(87+31, 31-1), 1, mapNone, 0);
	saveData.aiMineGrp[1].TakeUnit(mine);
	TethysGame::CreateUnit(smelter, mapCommonOreSmelter, LOCATION(107+31, 39-1), 1, mapNone, 0);
	saveData.aiMineGrp[1].TakeUnit(smelter);
	TethysGame::CreateUnit(truck, mapCargoTruck, LOCATION(99+31, 38-1), 1, mapNone, 0);
	saveData.aiMineGrp[1].TakeUnit(truck);
	TethysGame::CreateUnit(truck, mapCargoTruck, LOCATION(98+31, 39-1), 1, mapNone, 0);
	saveData.aiMineGrp[1].TakeUnit(truck);
	TethysGame::CreateUnit(truck, mapCargoTruck, LOCATION(99+31, 40-1), 1, mapNone, 0);
	saveData.aiMineGrp[1].TakeUnit(truck);
	saveData.aiMineGrp[1].Setup(mine, smelter, MAP_RECT(107-3+31, 39-2-1, 107+2+31, 39+2-1));

	// Rare Group - north smelter
	saveData.aiMineGrp[2] = CreateMiningGroup(Player[1]);
	TethysGame::CreateUnit(mine, mapCommonOreMine, LOCATION(99+31, 19-1), 1, mapNone, 0);
	saveData.aiMineGrp[2].TakeUnit(mine);
	TethysGame::CreateUnit(smelter, mapRareOreSmelter, LOCATION(110+31, 26-1), 1, mapNone, 0);
	saveData.aiMineGrp[2].TakeUnit(smelter);
	TethysGame::CreateUnit(truck, mapCargoTruck, LOCATION(104+31, 21-1), 1, mapNone, 0);
	saveData.aiMineGrp[2].TakeUnit(truck);
	TethysGame::CreateUnit(truck, mapCargoTruck, LOCATION(102+31, 22-1), 1, mapNone, 0);
	saveData.aiMineGrp[2].TakeUnit(truck);
	TethysGame::CreateUnit(truck, mapCargoTruck, LOCATION(106+31, 22-1), 1, mapNone, 0);
	saveData.aiMineGrp[2].TakeUnit(truck);
	saveData.aiMineGrp[2].Setup(mine, smelter, MAP_RECT(110-3+31, 26-2-1, 110+2+31, 26+2-1));
}

void SetupAIFactories()
{
	// Setup AI factories, Convecs, Earthworker, building group
	Unit sf, vf, convec, ewc;
	saveData.aiBuildGrp = CreateBuildingGroup(Player[1]);

	TethysGame::CreateUnit(convec, mapConVec, LOCATION(100+31, 46-1), 1, mapNone, 0);
	saveData.aiBuildGrp.TakeUnit(convec);
	TethysGame::CreateUnit(convec, mapConVec, LOCATION(102+31, 46-1), 1, mapNone, 0);
	saveData.aiBuildGrp.TakeUnit(convec);
	TethysGame::CreateUnit(convec, mapConVec, LOCATION(104+31, 46-1), 1, mapNone, 0);
	saveData.aiBuildGrp.TakeUnit(convec);
	TethysGame::CreateUnit(ewc, mapEarthworker, LOCATION(106+31, 46-1), 1, mapNone, 0);
	saveData.aiBuildGrp.TakeUnit(ewc);

	TethysGame::CreateUnit(sf, mapStructureFactory, LOCATION(102+31, 43-1), 1, mapNone, 0);
	saveData.aiBuildGrp.TakeUnit(sf);

	TethysGame::CreateUnit(vf, mapVehicleFactory, LOCATION(118+31, 49-1), 1, mapNone, 0);
	saveData.aiBuildGrp.TakeUnit(vf);
	TethysGame::CreateUnit(vf, mapVehicleFactory, LOCATION(115+31, 45-1), 1, mapNone, 0);
	saveData.aiBuildGrp.TakeUnit(vf);

	saveData.aiBuildGrp.SetRect(MAP_RECT(98+31, 45-1, 110+31, 47-1));

	// Record everything so the AI will rebuild it
	saveData.aiBuildGrp.RecordTube(LOCATION(124+31, 55-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(124+31, 54-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(124+31, 53-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(123+31, 53-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(122+31, 53-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(121+31, 53-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(121+31, 52-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(121+31, 51-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(121+31, 50-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(121+31, 49-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(112+31, 46-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(111+31, 46-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(110+31, 46-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(110+31, 45-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(110+31, 44-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(110+31, 43-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(110+31, 42-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(110+31, 41-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(110+31, 40-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(110+31, 39-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(110+31, 38-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(110+31, 37-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(110+31, 36-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(110+31, 35-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(110+31, 34-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(110+31, 33-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(110+31, 32-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(111+31, 33-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(112+31, 33-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(113+31, 33-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(114+31, 33-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(115+31, 33-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(116+31, 33-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(117+31, 33-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(117+31, 34-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(118+31, 34-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(119+31, 34-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(120+31, 34-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(121+31, 34-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(122+31, 34-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(123+31, 34-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(125+31, 36-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(125+31, 37-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(118+31, 35-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(110+31, 29-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(110+31, 30-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(110+31, 31-1));
	saveData.aiBuildGrp.RecordTube(LOCATION(115+31, 48-1));
	saveData.aiBuildGrp.RecordBuilding(LOCATION(87+31, 59-1), mapTokamak, mapNone);
	saveData.aiBuildGrp.RecordBuilding(LOCATION(91+31, 59-1), mapTokamak, mapNone);
	saveData.aiBuildGrp.RecordBuilding(LOCATION(87+31, 4-1), mapTokamak, mapNone);
	saveData.aiBuildGrp.RecordBuilding(LOCATION(83+31, 4-1), mapTokamak, mapNone);
	saveData.aiBuildGrp.RecordBuilding(LOCATION(91+31, 4-1), mapTokamak, mapNone);
	saveData.aiBuildGrp.RecordBuilding(LOCATION(124+31, 60-1), mapCommandCenter, mapNone);
	saveData.aiBuildGrp.RecordBuilding(LOCATION(124+31, 57-1), mapAgridome, mapNone);
	saveData.aiBuildGrp.RecordBuilding(LOCATION(125+31, 34-1), mapStandardLab, mapNone);
	saveData.aiBuildGrp.RecordBuilding(LOCATION(125+31, 39-1), mapAdvancedLab, mapNone);
	saveData.aiBuildGrp.RecordBuilding(LOCATION(117+31, 31-1), mapGuardPost, mapStickyfoam);
	saveData.aiBuildGrp.RecordBuilding(LOCATION(118+31, 36-1), mapGuardPost, mapESG);
	saveData.aiBuildGrp.RecordBuilding(LOCATION(106+31, 27-1), mapGuardPost, mapStickyfoam);
	saveData.aiBuildGrp.RecordBuilding(LOCATION(98+31, 43-1), mapGuardPost, mapRPG);

	saveData.aiBuildGrp.RecordBuilding(LOCATION(92 + 31, 53 - 1), mapCommonOreMine, mapNone);
	saveData.aiBuildGrp.RecordBuilding(LOCATION(87 + 31, 31 - 1), mapCommonOreMine, mapNone);
	saveData.aiBuildGrp.RecordBuilding(LOCATION(99 + 31, 19 - 1), mapRareOreMine, mapNone);

	saveData.aiBuildGrp.RecordBuilding(LOCATION(107 + 31, 43 - 1), mapCommonOreSmelter, mapNone);
	saveData.aiBuildGrp.RecordBuilding(LOCATION(107 + 31, 39 - 1), mapCommonOreSmelter, mapNone);
	saveData.aiBuildGrp.RecordBuilding(LOCATION(110 + 31, 26 - 1), mapRareOreSmelter, mapNone);
}

void SetupAIDefense()
{
	Unit lynx;

	// Group 1 - Defends north smelter
	saveData.aiDefGrp[0] = CreateFightGroup(Player[1]);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(94+31, 15-1), 1, mapMicrowave, 0);
	saveData.aiDefGrp[0].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(97+31, 22-1), 1, mapMicrowave, 0);
	saveData.aiDefGrp[0].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(93+31, 19-1), 1, mapMicrowave, 0);
	saveData.aiDefGrp[0].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(90+31, 22-1), 1, mapRPG, 0);
	saveData.aiDefGrp[0].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(96+31, 26-1), 1, mapRPG, 0);
	saveData.aiDefGrp[0].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(102+31, 25-1), 1, mapMicrowave, 0);
	saveData.aiDefGrp[0].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(104+31, 28-1), 1, mapMicrowave, 0);
	saveData.aiDefGrp[0].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(108+31, 19-1), 1, mapMicrowave, 0);
	saveData.aiDefGrp[0].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(102+31, 16-1), 1, mapMicrowave, 0);
	saveData.aiDefGrp[0].TakeUnit(lynx);

	// Oddity in OP2: You must call DoGuardGroup before specifying the target group!
	saveData.aiDefGrp[0].DoGuardGroup();
	saveData.aiDefGrp[0].SetTargetGroup(saveData.aiMineGrp[2]);
	saveData.aiDefGrp[0].SetTargCount(mapLynx, mapMicrowave, 7);
	saveData.aiDefGrp[0].SetTargCount(mapLynx, mapRPG, 2);
	saveData.aiDefGrp[0].SetRect(MAP_RECT(90+31, 15-1, 112+31, 28-1));

	// Group 2 - Defends lab area
	saveData.aiDefGrp[1] = CreateFightGroup(Player[1]);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(107+31, 30-1), 1, mapMicrowave, 0);
	saveData.aiDefGrp[1].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(114+31, 31-1), 1, mapRPG, 0);
	saveData.aiDefGrp[1].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(112+31, 32-1), 1, mapRPG, 0);
	saveData.aiDefGrp[1].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(112+31, 34-1), 1, mapRPG, 0);
	saveData.aiDefGrp[1].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(112+31, 38-1), 1, mapMicrowave, 0);
	saveData.aiDefGrp[1].TakeUnit(lynx);
	saveData.aiDefGrp[1].AddGuardedRect(MAP_RECT(109+31, 31-1, 127+31, 33-1));
	saveData.aiDefGrp[1].DoGuardRect();
	saveData.aiDefGrp[1].SetTargCount(mapLynx, mapMicrowave, 2);
	saveData.aiDefGrp[1].SetTargCount(mapLynx, mapRPG, 3);
	saveData.aiDefGrp[1].SetRect(MAP_RECT(111+31, 29-1, 121+31, 37-1));

	// Group 3 - Defends CC
	saveData.aiDefGrp[2] = CreateFightGroup(Player[1]);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(124+31, 51-1), 1, mapRPG, 0);
	saveData.aiDefGrp[2].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(119+31, 52-1), 1, mapMicrowave, 0);
	saveData.aiDefGrp[2].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(127+31, 55-1), 1, mapRPG, 0);
	saveData.aiDefGrp[2].TakeUnit(lynx);
	saveData.aiDefGrp[2].AddGuardedRect(MAP_RECT(121+31, 54-1, 128+31, 64-1));
	saveData.aiDefGrp[2].DoGuardRect();
	saveData.aiDefGrp[2].SetTargCount(mapLynx, mapMicrowave, 1);
	saveData.aiDefGrp[2].SetTargCount(mapLynx, mapRPG, 2);
	saveData.aiDefGrp[2].SetRect(MAP_RECT(121+31, 54-1, 128+31, 64-1));

	// Group 4 - Defends Factories
	saveData.aiDefGrp[3] = CreateFightGroup(Player[1]);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(98+31, 36-1), 1, mapMicrowave, 0);
	saveData.aiDefGrp[3].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(95+31, 38-1), 1, mapMicrowave, 0);
	saveData.aiDefGrp[3].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(94+31, 41-1), 1, mapMicrowave, 0);
	saveData.aiDefGrp[3].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(94+31, 45-1), 1, mapMicrowave, 0);
	saveData.aiDefGrp[3].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(97+31, 46-1), 1, mapMicrowave, 0);
	saveData.aiDefGrp[3].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(102+31, 36-1), 1, mapMicrowave, 0);
	saveData.aiDefGrp[3].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(100+31, 49-1), 1, mapMicrowave, 0);
	saveData.aiDefGrp[3].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(94+31, 50-1), 1, mapRPG, 0);
	saveData.aiDefGrp[3].TakeUnit(lynx);
	saveData.aiDefGrp[3].AddGuardedRect(MAP_RECT(91+31, 34-1, 111+31, 50-1));
	saveData.aiDefGrp[3].DoGuardRect();
	saveData.aiDefGrp[3].SetTargCount(mapLynx, mapMicrowave, 7);
	saveData.aiDefGrp[3].SetTargCount(mapLynx, mapRPG, 1);
	saveData.aiDefGrp[3].SetRect(MAP_RECT(91+31, 34-1, 111+31, 50-1));

	// Group 5 - Defends mid mine
	saveData.aiDefGrp[4] = CreateFightGroup(Player[1]);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(85+31, 33-1), 1, mapRPG, 0);
	saveData.aiDefGrp[4].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(89+31, 28-1), 1, mapRPG, 0);
	saveData.aiDefGrp[4].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(82+31, 28-1), 1, mapRPG, 0);
	saveData.aiDefGrp[4].TakeUnit(lynx);
	saveData.aiDefGrp[4].AddGuardedRect(MAP_RECT(82+31, 27-1, 92+31, 36-1));
	saveData.aiDefGrp[4].DoGuardRect();
	saveData.aiDefGrp[4].SetTargCount(mapLynx, mapRPG, 3);
	saveData.aiDefGrp[4].SetRect(MAP_RECT(82+31, 27-1, 92+31, 36-1));

	// Group 6 - Defends lower mine and toks
	saveData.aiDefGrp[5] = CreateFightGroup(Player[1]);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(85+31, 51-1), 1, mapRPG, 0);
	saveData.aiDefGrp[5].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(88+31, 49-1), 1, mapMicrowave, 0);
	saveData.aiDefGrp[5].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(96+31, 55-1), 1, mapRPG, 0);
	saveData.aiDefGrp[5].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(88+31, 49-1), 1, mapMicrowave, 0);
	saveData.aiDefGrp[5].TakeUnit(lynx);
	saveData.aiDefGrp[5].AddGuardedRect(MAP_RECT(85+31, 48-1, 97+31, 61-1));
	saveData.aiDefGrp[5].DoGuardRect();
	saveData.aiDefGrp[5].SetTargCount(mapLynx, mapRPG, 2);
	saveData.aiDefGrp[5].SetTargCount(mapLynx, mapMicrowave, 2);
	saveData.aiDefGrp[5].SetRect(MAP_RECT(85+31, 48-1, 97+31, 64-1));

	// Group 7 - Defends north toks
	saveData.aiDefGrp[6] = CreateFightGroup(Player[1]);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(78+31, 3-1), 1, mapRPG, 0);
	saveData.aiDefGrp[6].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(86+31, 8-1), 1, mapMicrowave, 0);
	saveData.aiDefGrp[6].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(92+31, 8-1), 1, mapMicrowave, 0);
	saveData.aiDefGrp[6].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(77+31, 10-1), 1, mapMicrowave, 0);
	saveData.aiDefGrp[6].TakeUnit(lynx);
	TethysGame::CreateUnit(lynx, mapLynx, LOCATION(94+31, 2-1), 1, mapRPG, 0);
	saveData.aiDefGrp[6].TakeUnit(lynx);
	saveData.aiDefGrp[6].AddGuardedRect(MAP_RECT(75+31, 1-1, 98+31, 16-1));
	saveData.aiDefGrp[6].DoGuardRect();
	saveData.aiDefGrp[6].SetTargCount(mapLynx, mapRPG, 2);
	saveData.aiDefGrp[6].SetTargCount(mapLynx, mapMicrowave, 3);
	saveData.aiDefGrp[6].SetRect(MAP_RECT(75+31, 1-1, 98+31, 16-1));
}
