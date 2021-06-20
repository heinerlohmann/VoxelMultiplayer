// Copyright 2021 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelValue.h"
#include "VoxelMinimal.h"
#include "VoxelDiff.h"
#include "UObject/Object.h"
#include "VoxelMultiplayerInterface.generated.h"

struct FVoxelMaterial;
struct FVoxelCompressedWorldSaveImpl;
class IVoxelMultiplayerClient;
class IVoxelMultiplayerServer;

// addition by hein0r
DECLARE_DYNAMIC_DELEGATE(FOnDisconnectDelegate);

UCLASS(Abstract, BlueprintType)
class VOXEL_API UVoxelMultiplayerInterface : public UObject
{
	GENERATED_BODY()

public:
	//~ Begin UVoxelMultiplayerInterface Interface

	// additions by hein0r
	FOnDisconnectDelegate OnDisconnectDelegate;

	virtual bool IsServer() const { unimplemented(); return false; }
	virtual TVoxelSharedPtr<IVoxelMultiplayerClient> CreateClient() const { unimplemented(); return nullptr; }
	virtual TVoxelSharedPtr<IVoxelMultiplayerServer> CreateServer() const { unimplemented(); return nullptr; }
	//~ End UVoxelMultiplayerInterface Interface
};

enum class EVoxelMultiplayerNextLoadType : uint8
{
	Save  = 0,
	Diffs = 1,
	KeepAlive = 2, // added by hein0r
	Unknown = 3
};

class IVoxelMultiplayerClient : public TVoxelSharedFromThis<IVoxelMultiplayerClient>
{
public:
	IVoxelMultiplayerClient() = default;
	virtual ~IVoxelMultiplayerClient() = default;

	//~ Begin IVoxelMultiplayerClient Interface
	virtual bool IsValid() const = 0;
	virtual void Destroy() = 0;

	virtual bool ReceiveDiffs(TArray<TVoxelChunkDiff<FVoxelValue>>& OutValueDiffs, TArray<TVoxelChunkDiff<FVoxelMaterial>>& OutMaterialDiffs) = 0;
	virtual bool ReceiveSave(FVoxelCompressedWorldSaveImpl& OutSave) = 0;
	// addition by hein0r:
	virtual void ReceiveKeepAlive() = 0;

	virtual EVoxelMultiplayerNextLoadType GetNextLoadType() = 0;
	//~ End IVoxelMultiplayerClient Interface
};

class IVoxelMultiplayerServer : public TVoxelSharedFromThis<IVoxelMultiplayerServer>
{
public:
	// Trigger this when a new player connects
	// Must be run on the game thread
	FSimpleDelegate OnConnection;
	
	IVoxelMultiplayerServer() = default;
	virtual ~IVoxelMultiplayerServer() = default;

	//~ Begin IVoxelMultiplayerServer Interface
	virtual bool IsValid() const = 0;
	virtual void Destroy() = 0;

	virtual void SendDiffs(const TArray<TVoxelChunkDiff<FVoxelValue>>& ValueDiffs, const TArray<TVoxelChunkDiff<FVoxelMaterial>>& MaterialDiffs) = 0;
	virtual void SendSave(FVoxelCompressedWorldSaveImpl& Save, bool bForceLoad) = 0;
	// addition by hein0r:
	virtual void SendKeepAlive() = 0;
	//~ End IVoxelMultiplayerServer Interface
};
