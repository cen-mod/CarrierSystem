modded class SCR_GetInUserAction : SCR_CompartmentUserAction
{
	override bool CanBePerformedScript(IEntity user)
	{
		if (CEN_CarrierSystem_Helper.GetCarried(user))
		{
			SetCannotPerformReason("#CEN_CarrierSystem-UserAction_Carrying");
			return false;
		};
		
		return super.CanBePerformedScript(user);
	}	
};
