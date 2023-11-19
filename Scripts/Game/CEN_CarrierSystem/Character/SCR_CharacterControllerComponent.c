//------------------------------------------------------------------------------------------------
modded class SCR_CharacterControllerComponent : CharacterControllerComponent
{
	//------------------------------------------------------------------------------------------------
	//! Release carried player if the carrier falls unconscious
	override protected void OnConsciousnessChanged(bool conscious)
	{
		super.OnConsciousnessChanged(conscious);
		
		ChimeraCharacter char = GetCharacter();
		if (!char || !CEN_CarrierSystem_Helper.IsCarrier(char))
			return;
		
		CEN_CarrierSystem_Helper.ReleaseFromCarrier(char);
	}
}
