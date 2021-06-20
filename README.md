# VoxelMultiplayer
Some modifications on the MP-code of the Voxel Plugin for Unreal Engine: https://www.unrealengine.com/marketplace/en-US/product/voxel-plugin-pro

What it does:

- Fix issue on simultaneous edits of value and material
- Make Server recognise and remove broken Sockets
- Send KeepAlive messages to clients on a regular basis (set KeepAlivePeriod in VoxelMultiplayerManager.h)
- Make Clients close their Socket when they did not receive anything from the Server for KeepAlivePeriod*1.5
- Add BlueprintNode "BindOnDisconnect" that allows to bind a delegate that get's called on the Client after it's Socket was closed

Fix for VoxelInvokers not working correctly on Server: (not included in this repo's code)
In file "VoxelDefaultLODManager.cpp" replace the Line (232 at the time I wrote this)
  InvokerSettings.bUseForLOD &= InvokerComponent->IsLocalInvoker();
by
  InvokerSettings.bUseForLOD &= InvokerComponent->IsLocalInvoker() || InvokerComponent->GetOwnerRole() == ENetRole::ROLE_Authority;

This is an ongoing effort to improve the Plugin's multiplayer. If you encounter issues please share them and feel free to contribute!
