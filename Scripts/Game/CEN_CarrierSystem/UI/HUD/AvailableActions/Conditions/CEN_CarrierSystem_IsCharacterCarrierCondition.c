[BaseContainerProps()]
class CEN_CarrierSystem_IsCharacterCarrierCondition : SCR_AvailableActionCondition
{
	override bool IsAvailable(SCR_AvailableActionsConditionData data)
	{		
		if (!data)
			return false;
		
		return GetReturnResult(data.CEN_CarrierSystem_GetIsCharacterCarrier());
	}
}