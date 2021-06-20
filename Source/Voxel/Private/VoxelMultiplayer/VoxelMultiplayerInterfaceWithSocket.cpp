// Copyright 2021 Phyronnaz

#include "VoxelMultiplayer/VoxelMultiplayerInterfaceWithSocket.h"
#include "VoxelMultiplayer/VoxelMultiplayerUtilities.h"
#include "VoxelMinimal.h"

#include "Misc/MessageDialog.h"

bool FVoxelMultiplayerClientWithSocket::ReceiveDiffs(TArray<TVoxelChunkDiff<FVoxelValue>>& OutValueDiffs, TArray<TVoxelChunkDiff<FVoxelMaterial>>& OutMaterialDiffs)
{
	VOXEL_FUNCTION_COUNTER();

	check(IsValid());
	check(NextLoadType == EVoxelMultiplayerNextLoadType::Diffs);

	TArray<uint8> ReceivedData;
	if (TryToReceiveData(ExpectedSize, ReceivedData))
	{
		FVoxelMultiplayerUtilities::ReadDiffs(ReceivedData, OutValueDiffs, OutMaterialDiffs);
		ResetHeaders();
		return true;
	}
	else
	{
		return false;
	}
}

bool FVoxelMultiplayerClientWithSocket::ReceiveSave(FVoxelCompressedWorldSaveImpl& OutSave)
{
	VOXEL_FUNCTION_COUNTER();

	check(IsValid());
	check(NextLoadType == EVoxelMultiplayerNextLoadType::Save);

	TArray<uint8> ReceivedData;
	if (TryToReceiveData(ExpectedSize, ReceivedData))
	{
		FVoxelMultiplayerUtilities::ReadSave(ReceivedData, OutSave);
		ResetHeaders();
		return true;
	}
	else
	{
		return false;
	}
}

// addition by hein0r:
void FVoxelMultiplayerClientWithSocket::ReceiveKeepAlive()
{
	VOXEL_FUNCTION_COUNTER();

	check(IsValid());
	check(NextLoadType == EVoxelMultiplayerNextLoadType::KeepAlive);

	ResetHeaders();
}

EVoxelMultiplayerNextLoadType FVoxelMultiplayerClientWithSocket::GetNextLoadType()
{
	VOXEL_FUNCTION_COUNTER();

	check(IsValid());
	if (NextLoadType == EVoxelMultiplayerNextLoadType::Unknown)
	{
		TArray<uint8> ReceivedData;
		if (TryToReceiveData(FVoxelMultiplayerUtilities::HeaderBytes, ReceivedData))
		{
			if (!FVoxelMultiplayerUtilities::LoadHeader(ReceivedData, ExpectedSize, NextLoadType))
			{
				FMessageDialog::Open(EAppMsgType::Ok, EAppReturnType::Ok, FText::FromString("Voxel Multiplayer MAGIC error! The socket is sending corrupted data"));
				Destroy();
			}
		}
	}
	return NextLoadType;
}

bool FVoxelMultiplayerClientWithSocket::TryToReceiveData(uint32 Size, TArray<uint8>& OutData)
{
	VOXEL_FUNCTION_COUNTER();

	FetchPendingData();

	if (uint32(PendingData.Num()) >= Size)
	{
		OutData = TArray<uint8>(PendingData.GetData(), Size);
		PendingData.RemoveAt(0, Size, false);
		return true;
	}
	else
	{
		return false;
	}
}

void FVoxelMultiplayerClientWithSocket::ResetHeaders()
{
	ExpectedSize = 0;
	NextLoadType = EVoxelMultiplayerNextLoadType::Unknown;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMultiplayerServerWithSocket::SendDiffs(const TArray<TVoxelChunkDiff<FVoxelValue>>& ValueDiffs, const TArray<TVoxelChunkDiff<FVoxelMaterial>>& MaterialDiffs)
{
	VOXEL_FUNCTION_COUNTER();

	check(ValueDiffs.Num() > 0 || MaterialDiffs.Num() > 0);

	TArray<uint8> Data;
	FVoxelMultiplayerUtilities::WriteDiffs(Data, ValueDiffs, MaterialDiffs);
	
	TArray<uint8> Header;
	FVoxelMultiplayerUtilities::CreateHeader(Header, Data.Num(), EVoxelMultiplayerNextLoadType::Diffs);
	
	SendData(Header, ETarget::ExistingSockets);
	SendData(Data, ETarget::ExistingSockets);
}

void FVoxelMultiplayerServerWithSocket::SendSave(FVoxelCompressedWorldSaveImpl& Save, bool bForceLoad)
{
	VOXEL_FUNCTION_COUNTER();

	TArray<uint8> Data;
	FVoxelMultiplayerUtilities::WriteSave(Data, Save);
	
	TArray<uint8> Header;
	FVoxelMultiplayerUtilities::CreateHeader(Header, Data.Num(), EVoxelMultiplayerNextLoadType::Save);

	if (bForceLoad)
	{
		SendData(Header, ETarget::ExistingSockets);
		SendData(Data, ETarget::ExistingSockets);
	}

	SendData(Header, ETarget::NewSockets);
	SendData(Data, ETarget::NewSockets);

	ClearNewSockets();
}

// addition by hein0r:
void FVoxelMultiplayerServerWithSocket::SendKeepAlive()
{
	VOXEL_FUNCTION_COUNTER();

	TArray<uint8> Header;
	FVoxelMultiplayerUtilities::CreateHeader(Header, 0, EVoxelMultiplayerNextLoadType::KeepAlive);

	SendData(Header, ETarget::ExistingSockets);
}
//

