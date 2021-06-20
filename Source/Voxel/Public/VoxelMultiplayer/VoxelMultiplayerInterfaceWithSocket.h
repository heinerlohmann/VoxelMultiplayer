// Copyright 2021 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelMultiplayer/VoxelMultiplayerInterface.h"

class VOXEL_API FVoxelMultiplayerClientWithSocket : public IVoxelMultiplayerClient
{
public:
	using IVoxelMultiplayerClient::IVoxelMultiplayerClient;
	
	//~ Begin IVoxelMultiplayerClient Interface
	virtual bool ReceiveDiffs(TArray<TVoxelChunkDiff<FVoxelValue>>& OutValueDiffs, TArray<TVoxelChunkDiff<FVoxelMaterial>>& OutMaterialDiffs) override final;
	virtual bool ReceiveSave(FVoxelCompressedWorldSaveImpl& OutSave) override final;
	
	// addition by hein0r
	virtual void ReceiveKeepAlive() override final;

	virtual EVoxelMultiplayerNextLoadType GetNextLoadType() override final;
	//~ End IVoxelMultiplayerClient Interface

protected:
	TArray<uint8> PendingData;

	//~ Begin FVoxelMultiplayerClientWithSocket Interface
	virtual void FetchPendingData() = 0;
	//~ End FVoxelMultiplayerClientWithSocket Interface

private:
	uint32 ExpectedSize = 0;
	EVoxelMultiplayerNextLoadType NextLoadType = EVoxelMultiplayerNextLoadType::Unknown;
	
	bool TryToReceiveData(uint32 Size, TArray<uint8>& OutData);
	void ResetHeaders();
};

class VOXEL_API FVoxelMultiplayerServerWithSocket : public IVoxelMultiplayerServer
{
public:
	using IVoxelMultiplayerServer::IVoxelMultiplayerServer;

	//~ Begin IVoxelMultiplayerServer Interface
	virtual void SendDiffs(const TArray<TVoxelChunkDiff<FVoxelValue>>& ValueDiffs, const TArray<TVoxelChunkDiff<FVoxelMaterial>>& MaterialDiffs) override final;
	virtual void SendSave(FVoxelCompressedWorldSaveImpl& Save, bool bForceLoad) override final;
	
	// addition by hein0r
	virtual void SendKeepAlive() override final;

	//~ End IVoxelMultiplayerServer Interface

protected:
	enum class ETarget : uint8
	{
		NewSockets,
		ExistingSockets
	};

	//~ Begin FVoxelMultiplayerServerWithSocket Interface
	virtual void SendData(const TArray<uint8>& Data, ETarget Target) = 0;
	virtual void ClearNewSockets() = 0;
	//~ End FVoxelMultiplayerServerWithSocket Interface

};
