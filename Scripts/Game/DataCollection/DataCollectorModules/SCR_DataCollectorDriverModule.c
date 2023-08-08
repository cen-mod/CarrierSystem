class SCR_DataCollectorDriverModuleContext : Managed
{
	IEntity m_Player;
	IEntity m_Vehicle;
	bool m_bPilot;

	void SCR_DataCollectorDriverModuleContext(notnull IEntity player, notnull IEntity vehicle, bool pilot)
	{
		m_Player = player;
		m_Vehicle = vehicle;
		m_bPilot = pilot;
	}
};

[BaseContainerProps()]
class SCR_DataCollectorDriverModule : SCR_DataCollectorModule
{
	protected ref map<int, ref SCR_DataCollectorDriverModuleContext> m_mTrackedPlayersInVehicles = new map<int, ref SCR_DataCollectorDriverModuleContext>();

	//------------------------------------------------------------------------------------------------
	protected override void AddInvokers(IEntity player)
	{
		super.AddInvokers(player);
		if (!player)
			return;

		SCR_CompartmentAccessComponent compartmentAccessComponent = SCR_CompartmentAccessComponent.Cast(player.FindComponent(SCR_CompartmentAccessComponent));

		if (!compartmentAccessComponent)
			return;

		compartmentAccessComponent.GetOnCompartmentEntered().Insert(OnCompartmentEntered);
		compartmentAccessComponent.GetOnCompartmentLeft().Insert(OnCompartmentLeft);
	}

	//------------------------------------------------------------------------------------------------
	protected override void RemoveInvokers(IEntity player)
	{
		super.RemoveInvokers(player);
		if (!player)
			return;

		SCR_CompartmentAccessComponent compartmentAccessComponent = SCR_CompartmentAccessComponent.Cast(player.FindComponent(SCR_CompartmentAccessComponent));

		if (!compartmentAccessComponent)
			return;

		compartmentAccessComponent.GetOnCompartmentEntered().Remove(OnCompartmentEntered);
		compartmentAccessComponent.GetOnCompartmentLeft().Remove(OnCompartmentLeft);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCompartmentEntered(IEntity targetEntity, BaseCompartmentManagerComponent manager, int mgrID, int slotID, bool move)
	{
		if (!targetEntity || !manager)
		{
			Print("ERROR IN DATACOLLECTOR DRIVER MODULE: TARGETENTITY OR MANAGER ARE EMPTY.", LogLevel.ERROR);
			return;
		}

		BaseCompartmentSlot compartment = manager.FindCompartment(slotID, mgrID);
		if (!compartment)
			return;

		IEntity playerEntity = compartment.GetOccupant();
		if (!playerEntity)
			return;

		int playerID = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(playerEntity);
		if (playerID == 0) // Non-player character
			return;

		m_mTrackedPlayersInVehicles.Insert(playerID, new SCR_DataCollectorDriverModuleContext(playerEntity, targetEntity, SCR_CompartmentAccessComponent.GetCompartmentType(compartment) == ECompartmentType.Pilot));
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCompartmentLeft(IEntity targetEntity, BaseCompartmentManagerComponent manager, int mgrID, int slotID, bool move)
	{
		BaseCompartmentSlot compartment = manager.FindCompartment(slotID, mgrID);
		if (!compartment)
			return;

		int playerID = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(compartment.GetOccupant());
		if (playerID == 0) // Non-player character
			return;

		//Is the player inside a vehicle?
		SCR_DataCollectorDriverModuleContext playerContext = m_mTrackedPlayersInVehicles.Get(playerID);
		if (!playerContext)
			return;

		//If the player died, blame the driver of that vehicle
		//QUESTION: WHAT IF THE PLAYER SUICIDES IN A VEHICLE
		ChimeraCharacter playerChimeraCharacter = ChimeraCharacter.Cast(playerContext.m_Player);
		if (playerChimeraCharacter && playerChimeraCharacter.GetDamageManager().GetState() == EDamageState.DESTROYED)
			PlayerDied(playerID, playerContext);

		m_mTrackedPlayersInVehicles.Remove(playerID);
	}

/*
TODO: REMOVE THIS, REPLACE WITH SENDING THROUGH RPL THE STATS FROM THE SERVER RECURRENTLY, AS IT'S ONLY FOR DEBUGGING PURPOSES
#ifdef ENABLE_DIAG
	//------------------------------------------------------------------------------------------------
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);

		if (to)
		{
			int playerID = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(to);
			m_mTrackedPlayersInVehicles.Insert(playerID, to);
		}
		else if (from)
		{
			int playerID = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(from);
			m_mTrackedPlayersInVehicles.Remove(playerID);
		}

	}
#endif
*/

	//------------------------------------------------------------------------------------------------
	//We call this method when a player dies and its ejected from a vehicle dead
	protected void PlayerDied(int PlayerID, notnull SCR_DataCollectorDriverModuleContext playerContext)
	{
		//TODO: Replace this using a C++ implemented method to get the count of occupants of the vehicle
		/**********************************************************************************************/
		SCR_BaseCompartmentManagerComponent compartmentManager = SCR_BaseCompartmentManagerComponent.Cast(playerContext.m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!compartmentManager)
			return;

		array<IEntity> occupants = {};
		compartmentManager.GetOccupants(occupants);

		if (occupants.Count() <= 1) //player was alone in vehicle. Do nothing
			return;
		/**********************************************************************************************/

		if (playerContext.m_bPilot)
		{
			array<IEntity> checkPilot = {};
			compartmentManager.GetOccupantsOfType(checkPilot, ECompartmentType.Pilot);

			if (checkPilot.IsEmpty())
			{
				Print("DataCollectorDriveModule: Pilot died but according to the compartmentManager there's no pilot. !!!", LogLevel.ERROR);
			}
			//check if pilot is pilot
			else if (playerContext.m_Player != checkPilot.Get(0))
			{
				Print("DataCollectorDriveModule: The pilot from the context is not the pilot from the compartments. !!", LogLevel.ERROR);
			}

			//The driver was killed.
			//All players from the vehicle are in danger now because of the pilot's death, so we act as if they died
			GetGame().GetDataCollector().GetPlayerData(PlayerID).AddStat(SCR_EDataStats.PLAYERS_DIED_IN_VEHICLE, occupants.Count()-1);
			return;
		}

		//Find if there's a pilot and their ID
		array<IEntity> pilot = {};
		compartmentManager.GetOccupantsOfType(pilot, ECompartmentType.Pilot);

		//If there's no pilot it's none's fault.
		if (pilot.IsEmpty())
			return;

		//Let's assume there's only one pilot, or if there are multiple, let's assume the main one is in the position 0
		int pilotID = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pilot.Get(0));

		if (pilotID == 0)
			return; //Pilot is an AI

		//A player died and it was not the pilot. So the pilot has someone dying on their vehicle
		GetGame().GetDataCollector().GetPlayerData(PlayerID).AddStat(SCR_EDataStats.PLAYERS_DIED_IN_VEHICLE, 1);
		//TODO: Make sure it is not possible to add this kill multiple times to the pilot if all players die simultaneously
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerKilled(int playerID, IEntity player, IEntity killer)
	{
		//We are only looking for roadkills here. That's why we don't call super.OnPlayerKilled. This behaviour is very specific

		//if there's a null or the player killed themselves by using the respawn feature, do nothing
		if (!player || !killer || (player == killer))
			return;

		int killerID = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(killer);

		//If the killer is AI count no roadKill
		if (killerID == 0)
			return;

		SCR_DataCollectorDriverModuleContext killerContext = m_mTrackedPlayersInVehicles.Get(killerID);
		//If the killer is not tracked as a driver, then this was no roadkill
		if (!killerContext || !killerContext.m_bPilot)
			return;

		//Now we know the killer is not an AI and they are a driver. Add roadkill!

		SCR_PlayerData killerData = GetGame().GetDataCollector().GetPlayerData(killerID);

		FactionAffiliationComponent victimAffiliation = FactionAffiliationComponent.Cast(player.FindComponent(FactionAffiliationComponent));
		if (!victimAffiliation)
			return;

		FactionAffiliationComponent killerAffiliation = FactionAffiliationComponent.Cast(killer.FindComponent(FactionAffiliationComponent));
		if (!killerAffiliation)
			return;

		Faction victimFaction = victimAffiliation.GetAffiliatedFaction();
		Faction killerFaction = killerAffiliation.GetAffiliatedFaction();

		//Add a kill. Find if friendly or unfriendly
		if (killerFaction && victimFaction)
		{
			if (killerFaction.IsFactionFriendly(victimFaction))
				killerData.AddStat(SCR_EDataStats.FRIENDLY_ROADKILLS);
			else
				killerData.AddStat(SCR_EDataStats.ROADKILLS);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnAIKilled(IEntity AI, IEntity killer)
	{
		//We are only looking for roadkills here. That's why we don't call super.OnAIKilled. This behaviour is very specific

		//This code has many similarities with OnPlayerKilled.
		//It would be nice to have only one method for OnCharacterKilled instead of having this duplicity

		if (!AI || !killer)
			return;

		int killerID = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(killer);

		if (killerID == 0)
			return;

		SCR_DataCollectorDriverModuleContext killerContext = m_mTrackedPlayersInVehicles.Get(killerID);
		//If the killer is not tracked as a driver, then this was no roadkill
		if (!killerContext || !killerContext.m_bPilot)
			return;

		//Now we know the killer is not an AI and they are a driver. Add roadkill!
		SCR_PlayerData killerData = GetGame().GetDataCollector().GetPlayerData(killerID);

		FactionAffiliationComponent victimAffiliation = FactionAffiliationComponent.Cast(AI.FindComponent(FactionAffiliationComponent));
		if (!victimAffiliation)
			return;

		FactionAffiliationComponent killerAffiliation = FactionAffiliationComponent.Cast(killer.FindComponent(FactionAffiliationComponent));
		if (!killerAffiliation)
			return;

		Faction victimFaction = victimAffiliation.GetAffiliatedFaction();
		Faction killerFaction = killerAffiliation.GetAffiliatedFaction();

		//Add an AI kill. Find if friendly or unfriendly
		if (killerFaction && victimFaction)
		{
			if (killerFaction.IsFactionFriendly(victimFaction))
				killerData.AddStat(SCR_EDataStats.FRIENDLY_AI_ROADKILLS);
			else
				killerData.AddStat(SCR_EDataStats.AI_ROADKILLS);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerDisconnected(int playerID, IEntity controlledEntity = null)
	{
		if (!controlledEntity)
			controlledEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerID);
		super.OnPlayerDisconnected(playerID, controlledEntity);

		m_mTrackedPlayersInVehicles.Remove(playerID);
	}

	//------------------------------------------------------------------------------------------------
	override void Update(float timeTick)
	{
		//If there are no players tracked, do nothing
		if (m_mTrackedPlayersInVehicles.IsEmpty())
			return;

		m_fTimeSinceUpdate += timeTick;

		if (m_fTimeSinceUpdate < m_fUpdatePeriod)
			return;

		for (int i = 0, count = m_mTrackedPlayersInVehicles.Count(); i < count; i++)
		{
			SCR_DataCollectorDriverModuleContext playerContext = m_mTrackedPlayersInVehicles.GetElement(i);
			if (!playerContext.m_Player || !playerContext.m_Vehicle)
			{
				Print("DataCollectorDriverModule:Update: this context's player or vehicle is null. Removing it from the list", LogLevel.WARNING);
				m_mTrackedPlayersInVehicles.RemoveElement(i);
				continue;
			}

			Physics physics = playerContext.m_Vehicle.GetPhysics();
			if (!physics)
			{
				//------------------------------------------------------------------------------------------------
				//!	MCS hard override: Remove the no physics warning spam
				// Print("DataCollectorDriverModule:Update: Couldn't find the vehicle's physics. Player ID: " + m_mTrackedPlayersInVehicles.GetKey(i) + ". Player is a pilot: " + playerContext.m_bPilot, LogLevel.WARNING);
				//------------------------------------------------------------------------------------------------
				continue;
			}

			float distanceTraveled = physics.GetVelocity().Length() * m_fTimeSinceUpdate;
			if (distanceTraveled < 1)
				continue;

			SCR_PlayerData playerData = GetGame().GetDataCollector().GetPlayerData(m_mTrackedPlayersInVehicles.GetKey(i));

			//If player is driver we give some points, if not we give others
			if (playerContext.m_bPilot)
			{
				playerData.AddStat(SCR_EDataStats.DISTANCE_DRIVEN, distanceTraveled);

				//Need to find the number of players this pilot is driving around
				//TODO: Replace this using a C++ implemented method to get the count of occupants of the vehicle
				SCR_BaseCompartmentManagerComponent compartmentManagerComponent = SCR_BaseCompartmentManagerComponent.Cast(playerContext.m_Vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
				if (!compartmentManagerComponent)
				{
					Print("DataCollectorDriverModule:Update: Could not add POINTS_AS_DRIVER_OF_PLAYERS because could find this vehicle's compartmentManagerComponent", LogLevel.WARNING);
					continue;
				}
				
				array<IEntity> occupants = {};
				compartmentManagerComponent.GetOccupants(occupants);

				//Give points to driver for driving distanceTraveled meters. Not counting the driver as occupant for points calculation
				playerData.AddStat(SCR_EDataStats.POINTS_AS_DRIVER_OF_PLAYERS, distanceTraveled * (occupants.Count() - 1) * playerData.GetConfigs().MODIFIER_DRIVER_OF_PLAYERS);
			}
			else
			{
				//Add distanceTraveled meters as occupant of a vehicle
				playerData.AddStat(SCR_EDataStats.DISTANCE_AS_OCCUPANT, distanceTraveled);
			}

			//DEBUG display
#ifdef ENABLE_DIAG
			if (m_StatsVisualization)
			{
				m_StatsVisualization.Get(EDriverModuleStats.MetersDriven).SetText(playerData.GetStat(SCR_EDataStats.DISTANCE_DRIVEN).ToString());
				m_StatsVisualization.Get(EDriverModuleStats.MetersAsOccupant).SetText(playerData.GetStat(SCR_EDataStats.DISTANCE_AS_OCCUPANT).ToString());
				m_StatsVisualization.Get(EDriverModuleStats.PointsAsDriver).SetText(playerData.GetStat(SCR_EDataStats.POINTS_AS_DRIVER_OF_PLAYERS).ToString());
				m_StatsVisualization.Get(EDriverModuleStats.RoadKills).SetText(playerData.GetStat(SCR_EDataStats.ROADKILLS).ToString());
				m_StatsVisualization.Get(EDriverModuleStats.AIRoadKills).SetText(playerData.GetStat(SCR_EDataStats.AI_ROADKILLS).ToString());
				m_StatsVisualization.Get(EDriverModuleStats.FriendlyRoadKills).SetText(playerData.GetStat(SCR_EDataStats.FRIENDLY_ROADKILLS).ToString());
				m_StatsVisualization.Get(EDriverModuleStats.FriendlyAIRoadKills).SetText(playerData.GetStat(SCR_EDataStats.FRIENDLY_AI_KILLS).ToString());
				m_StatsVisualization.Get(EDriverModuleStats.PlayersDiedInVehicle).SetText(playerData.GetStat(SCR_EDataStats.PLAYERS_DIED_IN_VEHICLE).ToString());
			}
#endif
		}
		m_fTimeSinceUpdate = 0;
	}

#ifdef ENABLE_DIAG
	//------------------------------------------------------------------------------------------------
	override void CreateVisualization()
	{
		super.CreateVisualization();
		if (!m_StatsVisualization)
			return;

		CreateEntry("Meters driven: ", 0, EDriverModuleStats.MetersDriven);
		CreateEntry("Meters as Occupant: ", 0, EDriverModuleStats.MetersAsOccupant);
		CreateEntry("Points as Driver of ppl: ", 0, EDriverModuleStats.PointsAsDriver);
		CreateEntry("RoadKills: ", 0, EDriverModuleStats.RoadKills);
		CreateEntry("AI RoadKills: ", 0, EDriverModuleStats.AIRoadKills);
		CreateEntry("Friendly RoadKills: ", 0, EDriverModuleStats.FriendlyRoadKills);
		CreateEntry("Friendly AI RoadKills: ", 0, EDriverModuleStats.FriendlyAIRoadKills);
		CreateEntry("DeadPlayers at ur Vehicle: ", 0, EDriverModuleStats.PlayersDiedInVehicle);
	}
#endif
};

#ifdef ENABLE_DIAG
enum EDriverModuleStats
{
	MetersDriven,
	MetersAsOccupant,
	PointsAsDriver,
	RoadKills,
	AIRoadKills,
	FriendlyRoadKills,
	FriendlyAIRoadKills,
	PlayersDiedInVehicle
};
#endif