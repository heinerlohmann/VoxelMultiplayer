// Copyright 2021 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "VoxelMultiplayer/VoxelMultiplayerInterfaceWithSocket.h"
#include "VoxelMultiplayerTcp.generated.h"

class FSocket;
class FTcpListener;
struct FIPv4Endpoint;
class FVoxelMultiplayerTcpServer;
class FVoxelMultiplayerTcpClient;

// TCP interface, only accepts IPv4
UCLASS()
class VOXEL_API UVoxelMultiplayerTcpInterface : public UVoxelMultiplayerInterface
{
	GENERATED_BODY()
	
public:

	// addition by hein0r
	/**
	* Bind Event to execute on Disconnect (called on Client only).
	* @param	OnDisconnect	Will be executed if connection timed out.
	*/
	UFUNCTION(BlueprintCallable, Category = "Voxel|Multiplayer|Tcp")
	void BindOnDisconnect(const FOnDisconnectDelegate& OnDisconnect);

	// addition by hein0r
	/**
	* Bind Event to execute on Disconnect (called on Client only).
	* @param	OnDisconnect	Will be executed if connection timed out.
	*/
	UFUNCTION(BlueprintCallable, Category = "Voxel|Multiplayer|Tcp")
	void BindOnLoadRemoteSave(const FOnLoadRemoteSaveDelegate& OnLoadRemoteSave);

	/**
	 * Connect to a TCP server
	 * @param	Ip		The IPv4 of the server
	 * @param	Port	The port of the server
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Multiplayer|Tcp")
	bool ConnectToServer(FString& OutError, const FString& Ip = TEXT("127.0.0.1"), int32 Port = 10000);
	
	/**
	 * Start a TCP server
	 * @param	Ip		The IPv4 to accept connection on. 0.0.0.0 to accept all
	 * @param	Port	The port of the server
	 */
	UFUNCTION(BlueprintCallable, Category = "Voxel|Multiplayer|Tcp")
	bool StartServer(FString& OutError, const FString& Ip = TEXT("0.0.0.0"), int32 Port = 10000);

public:

	//~ Begin UVoxelMultiplayerInterface Interface
	virtual bool IsServer() const override;
	virtual TVoxelSharedPtr<IVoxelMultiplayerClient> CreateClient() const override;
	virtual TVoxelSharedPtr<IVoxelMultiplayerServer> CreateServer() const override;

	//~ End UVoxelMultiplayerInterface Interface

private:
	TVoxelSharedPtr<FVoxelMultiplayerTcpClient> Client;
	TVoxelSharedPtr<FVoxelMultiplayerTcpServer> Server;
};

class FVoxelMultiplayerTcpClient : public FVoxelMultiplayerClientWithSocket
{
public:
	using FVoxelMultiplayerClientWithSocket::FVoxelMultiplayerClientWithSocket;

	bool Connect(const FString& Ip, int32 Port, FString& OutError);

	//~ Begin IVoxelMultiplayerClient Interface
	virtual bool IsValid() const override final;
	virtual void Destroy() override final;
	//~ End IVoxelMultiplayerClient Interface

protected:
	//~ Begin FVoxelMultiplayerClientWithSocket Interface
	virtual void FetchPendingData() override final;
	//~ End FVoxelMultiplayerClientWithSocket Interface

private:
	FSocket* Socket = nullptr;
};

class FVoxelMultiplayerTcpServer : public FVoxelMultiplayerServerWithSocket
{
public:
	FVoxelMultiplayerTcpServer();
	~FVoxelMultiplayerTcpServer();

	bool Start(const FString& Ip, int32 Port, FString& OutError);

	//~ Begin IVoxelMultiplayerServer Interface
	virtual bool IsValid() const override final;
	virtual void Destroy() override final;
	//~ End IVoxelMultiplayerServer Interface
	
protected:
	//~ Begin FVoxelMultiplayerServerWithSocket Interface
	virtual void SendData(const TArray<uint8>& Data, ETarget Target) override;
	virtual void ClearNewSockets() override;
	//~ Begin FVoxelMultiplayerServerWithSocket Interface

private:
	TUniquePtr<FTcpListener> TcpListener;

	TArray<FSocket*> Sockets;

	// Sockets that haven't received a save yet
	FCriticalSection NewSocketsSection;
	TArray<FSocket*> NewSockets;

	bool Accept(FSocket* NewSocket, const FIPv4Endpoint& Endpoint);
};
