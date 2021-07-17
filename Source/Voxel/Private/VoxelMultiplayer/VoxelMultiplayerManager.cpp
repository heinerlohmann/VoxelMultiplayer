// Copyright 2021 Phyronnaz

#include "VoxelMultiplayer/VoxelMultiplayerManager.h"
#include "VoxelMultiplayer/VoxelMultiplayerInterface.h"
#include "VoxelData/VoxelDataIncludes.h"
#include "VoxelData/VoxelSaveUtilities.h"
#include "VoxelDebug/VoxelDebugManager.h"
#include "VoxelTools/VoxelDataTools.h"
#include "VoxelRender/IVoxelLODManager.h"
#include "VoxelWorld.h"
#include "VoxelMessages.h"
#include "VoxelUtilities/VoxelThreadingUtilities.h"

DEFINE_VOXEL_SUBSYSTEM_PROXY(UVoxelMultiplayerSubsystemProxy);

bool UVoxelMultiplayerSubsystemProxy::ShouldCreateSubsystem(const FVoxelRuntime& Runtime, const FVoxelRuntimeSettings& Settings) const
{
	return Settings.bEnableMultiplayer;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMultiplayerManager::Create()
{
	Super::Create();

	if (!ensure(Settings.VoxelWorld.IsValid()))
	{
		return;
	}
	
	auto* MultiplayerInterfaceInstance = Settings.VoxelWorld->GetMultiplayerInterfaceInstance();
	if (MultiplayerInterfaceInstance)
	{
		if (MultiplayerInterfaceInstance->IsServer())
		{
			Server = MultiplayerInterfaceInstance->CreateServer();
		}
		else
		{
			Client = MultiplayerInterfaceInstance->CreateClient();
		}
	}
	else
	{
		FVoxelMessages::Error(
			"bEnableMultiplayer = true, but the multiplayer instance is not created! "
			"You need to call CreateMultiplayerInterfaceInstance before creating the voxel world.",
			Settings.Owner.Get());
	}

	if (Server.IsValid())
	{
		Server->OnConnection.BindThreadSafeSP(this, &FVoxelMultiplayerManager::OnConnection);
	}
	// addition by hein0r
	if (Client.IsValid())
	{
		LastReceiveTime = FPlatformTime::Seconds();
	}
}

void FVoxelMultiplayerManager::Destroy()
{
	Super::Destroy();
	
	StopTicking();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMultiplayerManager::Tick(float DeltaTime)
{
	const double Time = FPlatformTime::Seconds();
	if (Server.IsValid() && Time - LastSyncTime > 1. / Settings.MultiplayerSyncRate)
	{
		LastSyncTime = Time;
		SendData();
	}
	if (Client.IsValid())
	{
		if (Client->IsValid())
			ReceiveData();
		else
			LastReceiveTime = Time;
			//LOG_VOXEL(Log, TEXT("socket invalid"));
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMultiplayerManager::ReceiveData()
{
	VOXEL_FUNCTION_COUNTER();

	check(Client.IsValid());
	if (!Client->IsValid()) return;

	if (!ensure(Settings.VoxelWorld.IsValid())) return;

	const EVoxelMultiplayerNextLoadType NextLoadType = Client->GetNextLoadType();
	if (!Client->IsValid()) return;

	const double Time = FPlatformTime::Seconds();

	switch (NextLoadType)
	{
	case EVoxelMultiplayerNextLoadType::Save:
	{
		FVoxelCompressedWorldSaveImpl Save;
		if (Client->ReceiveSave(Save))
		{
			LastReceiveTime = Time;
			FVoxelUncompressedWorldSaveImpl DecompressedSave;
			UVoxelSaveUtilities::DecompressVoxelSave(Save, DecompressedSave);

			UVoxelDataTools::LoadFromSave(Settings.VoxelWorld.Get(), DecompressedSave, {});

			auto* MultiplayerInterfaceInstance = Settings.VoxelWorld->GetMultiplayerInterfaceInstance();
			if (MultiplayerInterfaceInstance)
			{
				MultiplayerInterfaceInstance->OnLoadRemoteSaveDelegate.ExecuteIfBound();
			}
		}
		break;
	}
	case EVoxelMultiplayerNextLoadType::Diffs:
	{
		TArray<TVoxelChunkDiff<FVoxelValue>> ValueDiffs;
		TArray<TVoxelChunkDiff<FVoxelMaterial>> MaterialDiffs;
		if (Client->ReceiveDiffs(ValueDiffs, MaterialDiffs))
		{
			LastReceiveTime = Time;
			TArray<FVoxelIntBox> ModifiedBounds;
			// TODO Async?
			GetSubsystemChecked<FVoxelData>().LoadFromDiffs(ValueDiffs, MaterialDiffs, ModifiedBounds);

			GetSubsystemChecked<IVoxelLODManager>().UpdateBounds(ModifiedBounds);
			GetSubsystemChecked<FVoxelDebugManager>().ReportMultiplayerSyncedChunks([&]() { return ModifiedBounds; });
		}
		break;
	}
	// addition by hein0r:
	case EVoxelMultiplayerNextLoadType::KeepAlive:
	{
		LOG_VOXEL(Verbose, TEXT("Received keep alive message!"));
		LastReceiveTime = Time;
		Client->ReceiveKeepAlive();
		break;
	}
	// 
	case EVoxelMultiplayerNextLoadType::Unknown:
		break;
	default:
		check(false);
		break;
	}
	// addition by hein0r:
	if (Time - LastReceiveTime > KeepAlivePeriod * 1.5)
	{
		LOG_VOXEL(Log, TEXT("VoxelTcp connection lost"));
		Client->Destroy();
		LastReceiveTime = Time;
		auto* MultiplayerInterfaceInstance = Settings.VoxelWorld->GetMultiplayerInterfaceInstance();
		if (MultiplayerInterfaceInstance)
		{
			MultiplayerInterfaceInstance->OnDisconnectDelegate.ExecuteIfBound();
		}
	}
	//
}

void FVoxelMultiplayerManager::SendData()
{
	VOXEL_FUNCTION_COUNTER();

	check(Server.IsValid());
	if (!Server->IsValid()) return;
	
	TArray<TVoxelChunkDiff<FVoxelValue>> ValueDiffs;
	TArray<TVoxelChunkDiff<FVoxelMaterial>> MaterialDiffs;
	GetSubsystemChecked<FVoxelData>().GetDiffs(ValueDiffs, MaterialDiffs);

	// original code:
	//if (ValueDiffs.Num() > 0 || MaterialDiffs.Num() > 0)
	//{
	//	Server->SendDiffs(ValueDiffs, MaterialDiffs);
	//}
	// modification by hein0r:
	const double Time = FPlatformTime::Seconds();
	if (MaterialDiffs.Num() > 0)
	{
		Server->SendDiffs(TArray<TVoxelChunkDiff<FVoxelValue>>(), MaterialDiffs);
		LastSendTime = Time;
	}
	if (ValueDiffs.Num() > 0)
	{
		Server->SendDiffs(ValueDiffs, TArray<TVoxelChunkDiff<FVoxelMaterial>>());
		LastSendTime = Time;
	}
	if (Time - LastSendTime > KeepAlivePeriod)
	{
		LOG_VOXEL(Verbose, TEXT("Sending keep alive message to clients"))
		Server->SendKeepAlive();
		LastSendTime = Time;
	}
	//
}

void FVoxelMultiplayerManager::OnConnection()
{
	VOXEL_FUNCTION_COUNTER();

	check(Server.IsValid());
	if (!Server->IsValid()) return;

	LOG_VOXEL(Log, TEXT("Sending world to clients"));

	FVoxelUncompressedWorldSaveImpl Save;
	TArray<FVoxelObjectArchiveEntry> Objects;
	GetSubsystemChecked<FVoxelData>().GetSave(Save, Objects);
	ensureMsgf(Objects.Num() == 0, TEXT("Placeable items are not supported in multiplayer"));
	
	FVoxelCompressedWorldSaveImpl CompressedSave;
	UVoxelSaveUtilities::CompressVoxelSave(Save, CompressedSave);

	Server->SendSave(CompressedSave, false);

	OnClientConnection.Broadcast();
}

