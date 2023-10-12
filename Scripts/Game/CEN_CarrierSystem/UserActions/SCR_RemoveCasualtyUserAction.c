enum CEN_CarrierSystem_CompartmentUserActionMode
{
	NONE,
	REMOVE_CASUALTY,
	ADD_CASUALTY
};

modded class SCR_RemoveCasualtyUserAction : SCR_CompartmentUserAction
{
	const string m_sCannotPerformHostile = "#AR-UserAction_SeatHostile";
	const string m_sCannotPerformObstructed = "#AR-UserAction_SeatObstructed";
	CEN_CarrierSystem_CompartmentUserActionMode m_iMode = CEN_CarrierSystem_CompartmentUserActionMode.NONE;
	
	IEntity CEN_CarrierSystem_GetOccupant()
	{
		BaseCompartmentSlot compartment = GetCompartmentSlot();
		if (!compartment)
			return null;

		return compartment.GetOccupant();
	}
	
	protected void CEN_CarrierSystem_UpdateActionMode(IEntity user)
	{
		ChimeraCharacter targetCharacter = ChimeraCharacter.Cast(CEN_CarrierSystem_GetOccupant());
		m_iMode = CEN_CarrierSystem_CompartmentUserActionMode.NONE;
		
		if (targetCharacter)
		{
			SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(targetCharacter.GetCharacterController());
			if (!controller)
				return;
			
			if (controller.GetLifeState() != ECharacterLifeState.ALIVE)
				m_iMode = CEN_CarrierSystem_CompartmentUserActionMode.REMOVE_CASUALTY;
		}
		else if (CEN_CarrierSystem_Helper.IsCarrier(user))
		{
			m_iMode = CEN_CarrierSystem_CompartmentUserActionMode.ADD_CASUALTY;
		};
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity)
			return;
				
		if (m_iMode == CEN_CarrierSystem_CompartmentUserActionMode.REMOVE_CASUALTY)
		{
			ChimeraCharacter targetCharacter = ChimeraCharacter.Cast(CEN_CarrierSystem_GetOccupant());
			if (!targetCharacter)
				return;
	
			CompartmentAccessComponent casualtyCompartmentAccess = targetCharacter.GetCompartmentAccessComponent();
			if (!casualtyCompartmentAccess)
				return;
	
			CEN_CarrierSystem_Helper.Carry(pUserEntity, targetCharacter);
		}
		else if (m_iMode == CEN_CarrierSystem_CompartmentUserActionMode.ADD_CASUALTY)
		{
			ChimeraCharacter targetCharacter = ChimeraCharacter.Cast(CEN_CarrierSystem_Helper.GetCarried(pUserEntity));
			if (!targetCharacter)
				return;
			
			CEN_CarrierSystem_Helper.ReleaseFromCarrier(pUserEntity);
			
			CompartmentAccessComponent casualtyCompartmentAccess = targetCharacter.GetCompartmentAccessComponent();
			if (!casualtyCompartmentAccess)
				return;
			
			BaseCompartmentSlot targetCompartment = GetCompartmentSlot();
			if (!targetCompartment)
				return;
			
			casualtyCompartmentAccess.MoveInVehicle(pOwnerEntity, targetCompartment);
		}
		else
		{
			return;
		};
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		CEN_CarrierSystem_UpdateActionMode(user);
		BaseCompartmentSlot compartment = GetCompartmentSlot();
		if (!compartment)
			return false;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(user);
		if (!character)
			return false;

		CompartmentAccessComponent compartmentAccess = character.GetCompartmentAccessComponent();
		if (!compartmentAccess)
			return false;

		// Make sure vehicle can be enter via provided door, if not, set reason.
		if (!character.IsInVehicle() && !compartmentAccess.CanGetInVehicleViaDoor(GetOwner(), compartment, GetRelevantDoorIndex(user)))
		{
			SetCannotPerformReason(m_sCannotPerformObstructed);
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		CEN_CarrierSystem_UpdateActionMode(user);
		if (m_iMode == CEN_CarrierSystem_CompartmentUserActionMode.NONE)
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (!character)
			return false;

		CompartmentAccessComponent compartmentAccess = character.GetCompartmentAccessComponent();
		if (!compartmentAccess)
			return false;

		if (compartmentAccess.IsGettingIn() || compartmentAccess.IsGettingOut())
			return false;

		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		BaseCompartmentSlot compartment = GetCompartmentSlot();
		if (!compartment)
			return false;
		
		UIInfo compartmentInfo = compartment.GetUIInfo();
		if (!compartmentInfo)
			return false;
		
		string selfName = "";
		if (m_iMode == CEN_CarrierSystem_CompartmentUserActionMode.REMOVE_CASUALTY)
		{
			selfName = "#AR-UserAction_RemoveCasualty";
		}
		else if (m_iMode == CEN_CarrierSystem_CompartmentUserActionMode.ADD_CASUALTY)
		{
			selfName = "#CEN_CarrierSystem-UserAction_AddCasualty";
		};
			
		outName = selfName + "%CTX_HACK%" + compartmentInfo.GetName();
		
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}
};
