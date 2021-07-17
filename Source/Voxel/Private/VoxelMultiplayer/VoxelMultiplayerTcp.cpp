// Copyright 2021 Phyronnaz

#include "VoxelMultiplayer/VoxelMultiplayerTcp.h"
#include "VoxelMessages.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Common/TcpSocketBuilder.h"
#include "Common/TcpListener.h"
#include "Async/Async.h"

#define TCP_MAX_PACKET_SIZE 128000

void UVoxelMultiplayerTcpInterface::BindOnDisconnect(const FOnDisconnectDelegate& OnDisconnect)
{
	OnDisconnectDelegate = OnDisconnect;
}

void UVoxelMultiplayerTcpInterface::BindOnLoadRemoteSave(const FOnLoadRemoteSaveDelegate& OnLoadRemoteSave)
{
	OnLoadRemoteSaveDelegate = OnLoadRemoteSave;
}

// original code:
bool UVoxelMultiplayerTcpInterface::ConnectToServer(FString& OutError, const FString& Ip, int32 Port)
{
	// commented out by hein0r
	//if (Client.IsValid())
	//{
	//	FVoxelMessages::Error(FUNCTION_ERROR("Client already created!"), this);
	//	OutError = "Client already created";
	//	return false;
	//}
	if (Server.IsValid())
	{
		FVoxelMessages::Error(FUNCTION_ERROR("Server already created!"), this);
		OutError = "Server already created";
		return false;
	}
	// modified by hein0r:
	if (!Client.IsValid())
		Client = MakeVoxelShared<FVoxelMultiplayerTcpClient>();
	else if (Client->IsValid())
		return false;
	//
	if (!Client->Connect(Ip, Port, OutError))
	{
		Client.Reset();
		FVoxelMessages::Error(FUNCTION_ERROR("Connect Error: " + OutError), this);
		return false;
	}
	return true;
}

bool UVoxelMultiplayerTcpInterface::StartServer(FString& OutError, const FString& Ip, int32 Port)
{
	if (Client.IsValid())
	{
		FVoxelMessages::Error(FUNCTION_ERROR("Client already created!"), this);
		OutError = "Client already created";
		return false;
	}
	if (Server.IsValid())
	{
		FVoxelMessages::Error(FUNCTION_ERROR("Server already created!"), this);
		OutError = "Server already created";
		return false;
	}

	Server = MakeVoxelShared<FVoxelMultiplayerTcpServer>();
	if (!Server->Start(Ip, Port, OutError))
	{
		Server.Reset();
		FVoxelMessages::Error(FUNCTION_ERROR("Start Error: " + OutError), this);
		return false;
	}
	return true;
}

bool UVoxelMultiplayerTcpInterface::IsServer() const
{
	if (!Client.IsValid() && !Server.IsValid())
	{
		FVoxelMessages::Error(FUNCTION_ERROR("Neither Client nor Server is created! You need to call ConnectToServer or StartServer before creating the voxel world"), this);
		return false;
	}
	ensure(!Client.IsValid() || !Server.IsValid());
	return Server.IsValid();
}

TVoxelSharedPtr<IVoxelMultiplayerClient> UVoxelMultiplayerTcpInterface::CreateClient() const
{
	return Client;
}

TVoxelSharedPtr<IVoxelMultiplayerServer> UVoxelMultiplayerTcpInterface::CreateServer() const
{
	return Server;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#define CHECK_ERROR(Test, Error) if (!(Test)) { OutError = Error; return false; }

bool FVoxelMultiplayerTcpClient::Connect(const FString& Ip, int32 Port, FString& OutError)
{
	VOXEL_FUNCTION_COUNTER();

	check(!IsValid());

	FIPv4Address Address;
	if (!FIPv4Address::Parse(Ip, Address))
	{
		OutError = "Invalid IP address";
		return false;
	}
	const FIPv4Endpoint Endpoint(Address, Port);

	Socket = FTcpSocketBuilder(TEXT("RemoteConnection")).AsBlocking();

	int32 BufferSize = TCP_MAX_PACKET_SIZE;
	int32 NewSize;
	Socket->SetReceiveBufferSize(BufferSize, NewSize);
	if (!ensureAlwaysMsgf(BufferSize == NewSize, TEXT("Invalid buffer size: %d"), BufferSize))
	{
		OutError = "Invalid buffer size";
		Destroy();
		return false;
	}

	if (!Socket->Connect(*Endpoint.ToInternetAddr()))
	{
		OutError = "Couldn't connect";
		Destroy();
		return false;
	}

	LOG_VOXEL(Log, TEXT("TCP Client: Connected! %s:%d"), *Ip, Port);

	return true;
}

bool FVoxelMultiplayerTcpClient::IsValid() const
{
	return Socket != nullptr;
}

void FVoxelMultiplayerTcpClient::Destroy()
{
	VOXEL_FUNCTION_COUNTER();
	
	if (Socket)
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}
}

void FVoxelMultiplayerTcpClient::FetchPendingData()
{
	VOXEL_FUNCTION_COUNTER();

	uint32 PendingDataSize;
	if (Socket->HasPendingData(PendingDataSize))
	{
		const int32 CurrentPos = PendingData.AddUninitialized(PendingDataSize);

		int32 BytesRead = 0;
		ensureAlwaysMsgf(Socket->Recv(PendingData.GetData() + CurrentPos, PendingDataSize, BytesRead), TEXT("Receive data: invalid socket!"));
		if (!ensureAlwaysMsgf(BytesRead == PendingDataSize, TEXT("Received %d instead of %d"), BytesRead, PendingDataSize))
		{
			PendingData.Reset();
			Destroy();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelMultiplayerTcpServer::FVoxelMultiplayerTcpServer() 
{
	// For unique ptr forward decl
}

FVoxelMultiplayerTcpServer::~FVoxelMultiplayerTcpServer()
{
	// For unique ptr forward decl
}

bool FVoxelMultiplayerTcpServer::Start(const FString& Ip, int32 Port, FString& OutError)
{
	VOXEL_FUNCTION_COUNTER();

	if (IsValid())
	{
		OutError = "Already started";
		return false;
	}

	FIPv4Address Address;
	if (!FIPv4Address::Parse(Ip, Address))
	{
		OutError = "Invalid IP address";
		return false;
	}
	FIPv4Endpoint Endpoint(Address, Port);

	TcpListener = MakeUnique<FTcpListener>(Endpoint);
	TcpListener->OnConnectionAccepted().BindRaw(this, &FVoxelMultiplayerTcpServer::Accept);

	LOG_VOXEL(Log, TEXT("TCP Server: Started! %s:%d"), *Ip, Port);
	
	return true;
}

bool FVoxelMultiplayerTcpServer::IsValid() const
{
	return TcpListener.IsValid();
}

void FVoxelMultiplayerTcpServer::Destroy()
{
	VOXEL_FUNCTION_COUNTER();
	
	TcpListener.Reset();
}

void FVoxelMultiplayerTcpServer::SendData(const TArray<uint8>& Data, ETarget Target)
{
	VOXEL_FUNCTION_COUNTER();
	TArray<FSocket*> SocketsToRemove;
	if (Target == ETarget::NewSockets)
	{
		NewSocketsSection.Lock();
	}
	for (FSocket* Socket : (Target == ETarget::NewSockets ? NewSockets : Sockets))
	{
		for (int32 Offset = 0; Offset < Data.Num(); Offset += TCP_MAX_PACKET_SIZE)
		{
			int32 BytesToSend = FMath::Min<int32>(TCP_MAX_PACKET_SIZE, Data.Num() - Offset);
			int32 BytesSent = 0;
			// original code:
			//ensureAlwaysMsgf(Socket->Send(Data.GetData() + Offset, BytesToSend, BytesSent), TEXT("SendData: invalid socket!"));
			//ensureAlwaysMsgf(BytesSent == BytesToSend, TEXT("%d bytes sent instead of %d!"), BytesSent, BytesToSend);
			// modification by hein0r:
			if (Socket != nullptr)
			{
				if (!Socket->Send(Data.GetData() + Offset, BytesToSend, BytesSent))
				{
					ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
					SocketsToRemove.Add(Socket);
				}
			}
			else
				SocketsToRemove.Add(Socket);
			//
		}
	}
	if (Target == ETarget::NewSockets)
	{
		NewSocketsSection.Unlock();
	}
	for (FSocket* Socket : SocketsToRemove)
	{
		(Target == ETarget::NewSockets ? NewSockets : Sockets).Remove(Socket);
	}
}

void FVoxelMultiplayerTcpServer::ClearNewSockets()
{
	VOXEL_FUNCTION_COUNTER();

	FScopeLock Lock(&NewSocketsSection);
	Sockets.Append(NewSockets);
	NewSockets.Reset();
}

bool FVoxelMultiplayerTcpServer::Accept(FSocket* NewSocket, const FIPv4Endpoint& Endpoint)
{
	VOXEL_ASYNC_FUNCTION_COUNTER();

	LOG_VOXEL(Log, TEXT("TCP Server: Client connected! %s"), *Endpoint.ToString());

	AsyncTask(ENamedThreads::GameThread,  MakeVoxelWeakPtrLambda(this, [=]() { OnConnection.ExecuteIfBound(); }));

	FScopeLock Lock(&NewSocketsSection);
	NewSockets.Add(NewSocket);

	return true;
}
