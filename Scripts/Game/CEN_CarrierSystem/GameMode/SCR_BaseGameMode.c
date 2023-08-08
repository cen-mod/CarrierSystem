modded class SCR_BaseGameMode : BaseGameMode
{
	protected override void OnPlayerKilled(int playerId, IEntity player, IEntity killer)
	{
		CEN_CarrierSystem_Helper.ReleaseFromCarrier(player);
		CEN_CarrierSystem_Helper.ReleaseCarried(player);
		super.OnPlayerKilled(playerId, player, killer);
	};
	
	protected override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		CEN_CarrierSystem_Helper.ReleaseFromCarrier(player);
		CEN_CarrierSystem_Helper.ReleaseCarried(player);
		super.OnPlayerDisconnected(playerId, cause, timeout);
	};
};
