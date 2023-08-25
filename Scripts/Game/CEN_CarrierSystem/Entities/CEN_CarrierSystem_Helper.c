class CEN_CarrierSystem_HelperClass : GenericEntityClass
{
};

class CEN_CarrierSystem_Helper : GenericEntity
{
	protected IEntity m_eCarrier = null;
	protected IEntity m_eCarried = null;
	protected SCR_CharacterControllerComponent m_CarrierCharCtrl;
	protected bool m_bMarkedForDeletion = false;
	protected static EPhysicsLayerPresets m_iPhysicsLayerPreset = -1;
	private static const ResourceName HELPER_PREFAB_NAME = "{FF78613C1DAFF28F}Prefabs/Helpers/CEN_CarrierSystem_Helper.et";
	private static const int SEARCH_POS_RADIUS = 5; // m
	private static const float PRONE_CHECK_TIMEOUT = 100; // ms
	private static const float CLEANUP_TIMEOUT = 1000; // ms
	
	static void Carry(IEntity carrier, IEntity carried)
	{
		if (!carrier || !carried)
			return;
		
		CEN_CarrierSystem_Helper helper = CEN_CarrierSystem_Helper.Cast(GetGame().SpawnEntityPrefab(Resource.Load(HELPER_PREFAB_NAME), null, EntitySpawnParams()));
		helper.m_eCarrier = carrier;
		helper.m_eCarried = carried;
		
		PlayerManager playerManager = GetGame().GetPlayerManager();
		SCR_PlayerController carrierCtrl = SCR_PlayerController.Cast(playerManager.GetPlayerController(playerManager.GetPlayerIdFromControlledEntity(carrier)));
		RplComponent helperRpl = RplComponent.Cast(helper.FindComponent(RplComponent));
		RplId carrierCtrlId = carrierCtrl.GetRplIdentity();
		helperRpl.Give(carrierCtrlId);
						
		carrier.AddChild(helper, carrier.GetBoneIndex("Spine5"));

		RplId carriedId = RplComponent.Cast(carried.FindComponent(RplComponent)).Id();
		helper.Rpc(helper.RpcDo_Owner_Carry, carriedId);
		SCR_CompartmentAccessComponent compartmentAccessComponent = SCR_CompartmentAccessComponent.Cast(carried.FindComponent(SCR_CompartmentAccessComponent));
		compartmentAccessComponent.MoveInVehicle(helper, ECompartmentType.Cargo);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Owner_Carry(RplId carriedId)
	{
		GetGame().GetInputManager().AddActionListener("CEN_CarrierSystem_Release", EActionTrigger.DOWN, ActionReleaseCallback);
		IEntity carried = RplComponent.Cast(Replication.FindItem(carriedId)).GetEntity();
		Physics carriedPhys = carried.GetPhysics();
		
		if (m_iPhysicsLayerPreset < 0)
			m_iPhysicsLayerPreset = carriedPhys.GetInteractionLayer();
		
		carriedPhys.SetInteractionLayer(EPhysicsLayerPresets.FireGeo);
		SCR_PlayerController carrierCtrl = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		ChimeraCharacter carrier = ChimeraCharacter.Cast(carrierCtrl.GetControlledEntity());
		m_CarrierCharCtrl = SCR_CharacterControllerComponent.Cast(carrier.GetCharacterController());
		GetGame().GetCallqueue().CallLater(PreventProneCarrier, PRONE_CHECK_TIMEOUT, true);
	}
	
	protected void PreventProneCarrier()
	{
		if (m_CarrierCharCtrl.GetStance() == ECharacterStance.PRONE)
			m_CarrierCharCtrl.SetStanceChange(ECharacterStanceChange.STANCECHANGE_TOCROUCH);
	}

	static void ReleaseFromCarrier(IEntity carrier)
	{
		CEN_CarrierSystem_Helper helper = GetHelperFromCarrier(carrier);
		
		if (!helper)
			return;
		
		helper.Release();
	}

	static void ReleaseCarried(IEntity carried)
	{
		CEN_CarrierSystem_Helper helper = GetHelperFromCarried(carried);
		
		if (!helper)
			return;
		
		helper.Release();
	}
	
	void Release()
	{
		if (m_bMarkedForDeletion)
			return;
		
		RplId carriedId = Replication.INVALID_ID;
		if (m_eCarried)
		{
			carriedId = RplComponent.Cast(m_eCarried.FindComponent(RplComponent)).Id();
			
			SCR_CompartmentAccessComponent compartmentAccessComponent = SCR_CompartmentAccessComponent.Cast(m_eCarried.FindComponent(SCR_CompartmentAccessComponent));
			vector target_pos;
			vector target_transform[4];
			m_eCarrier.GetTransform(target_transform);
			SCR_WorldTools.FindEmptyTerrainPosition(target_pos, target_transform[3] + target_transform[2], SEARCH_POS_RADIUS);
			target_transform[3] = target_pos;
			compartmentAccessComponent.MoveOutVehicle(-1, target_transform);
		};
		
		Rpc(RpcDo_Owner_Release, carriedId);
		m_eCarrier = null;
		m_eCarried = null;
		m_bMarkedForDeletion = true;
		// Clean up later since otherwise the carried player gets deleted as well...
		GetGame().GetCallqueue().CallLater(SCR_EntityHelper.DeleteEntityAndChildren, CLEANUP_TIMEOUT, false, this);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Owner_Release(RplId carriedId)
	{
		GetGame().GetInputManager().RemoveActionListener("CEN_CarrierSystem_Release", EActionTrigger.DOWN, ActionReleaseCallback);
		
		RplComponent carriedRpl = RplComponent.Cast(Replication.FindItem(carriedId));
		if (carriedRpl)
		{
			IEntity carried = carriedRpl.GetEntity();
			carried.GetPhysics().SetInteractionLayer(m_iPhysicsLayerPreset);
		};

		GetGame().GetCallqueue().Remove(PreventProneCarrier);
	}
	
	protected void ActionReleaseCallback()
	{
		Rpc(ActionReleaseServer);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void ActionReleaseServer()
	{
		Release();
	}
	
	static bool IsCarrier(IEntity carrier)
	{
		return GetHelperFromCarrier(carrier);
	}
	
	static bool IsCarried(IEntity carried)
	{
		return GetHelperFromCarried(carried);
	}

	static IEntity GetCarried(IEntity carrier)
	{
		CEN_CarrierSystem_Helper helper = GetHelperFromCarrier(carrier);
		if (!helper)
			return null;
		
		return helper.m_eCarried;
	}
	
	static IEntity GetCarrier(IEntity carried)
	{
		CEN_CarrierSystem_Helper helper = GetHelperFromCarried(carried);
		if (!helper)
			return null;
		
		return helper.m_eCarrier;
	}

	protected static CEN_CarrierSystem_Helper GetHelperFromCarrier(IEntity carrier)
	{
		CEN_CarrierSystem_Helper helper = null;
		IEntity child = carrier.GetChildren();
		
		while (child)
		{
			helper = CEN_CarrierSystem_Helper.Cast(child);
			
			if (helper)
				break;
			
			child = child.GetSibling();
		};
		
		if (!helper || helper.m_bMarkedForDeletion)
			return null;
		
		return helper;
	}

	protected static CEN_CarrierSystem_Helper GetHelperFromCarried(IEntity carried)
	{
		CEN_CarrierSystem_Helper helper = CEN_CarrierSystem_Helper.Cast(carried.GetParent());
		
		if (!helper || helper.m_bMarkedForDeletion)
			return null;
		
		return helper;
	}
}
