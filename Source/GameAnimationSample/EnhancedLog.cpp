#include "EnhancedLog.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"

#define MAX_RECURSIVE_COUNT 10

static FORCEINLINE_DEBUGGABLE const APlayerState* GetPlayerState(const APawn* Pawn)
{
	return Pawn ? Pawn->GetPlayerState() : nullptr;
}

static FORCEINLINE_DEBUGGABLE const APlayerState* GetPlayerState(const AActor* Actor)
{
	uint32 IterCount = 0;
	while (IterCount < MAX_RECURSIVE_COUNT && Actor)
	{
		IterCount++;
		if (const APlayerState* PlayerState = GetPlayerState(Cast<APawn>(Actor)))
		{
			return PlayerState;
		}
		Actor = Actor->GetOwner();
	}
	return nullptr;
}

static FORCEINLINE_DEBUGGABLE const APlayerState* GetPlayerState(const UActorComponent* ActorComponent)
{
	return ActorComponent ? GetPlayerState(ActorComponent->GetOwner()) : nullptr;
}


static FORCEINLINE_DEBUGGABLE const APlayerState* GetPlayerState(const APlayerController* PlayerController)
{
	return PlayerController ? PlayerController->GetPlayerState<APlayerState>() : nullptr;
}

static FORCEINLINE_DEBUGGABLE const APlayerState* GetPlayerState(const UObject* Object)
{
	uint32 IterCount = 0;
	while (IterCount < MAX_RECURSIVE_COUNT && Object)
	{
		IterCount++;
		if (const APlayerState* PlayerState = GetPlayerState(Cast<APawn>(Object)))
		{
			return PlayerState;
		}
		if (const APlayerState* PlayerState = GetPlayerState(Cast<AActor>(Object)))
		{
			return PlayerState;
		}
		if (const APlayerState* PlayerState = GetPlayerState(Cast<UActorComponent>(Object)))
		{
			return PlayerState;
		}
		if (const APlayerState* PlayerState = GetPlayerState(Cast<APlayerController>(Object)))
		{
			return PlayerState;
		}
		Object = Object->GetOuter();
	}
	return nullptr;
}
#undef MAX_RECURSIVE_COUNT

FString UEnhancedPlayerInfo::DefaultGetPlayerInfoString(const UObject* ContextObject)
{
	FString PlayerName;
	FString PlayerID;
	FString NetMode(" ");
	FString NetRole("ROLE_None");
	if (ContextObject && ContextObject->GetClass() && ContextObject->GetClass()->ImplementsInterface(UUEnhancedPlayerInfoInterface::StaticClass()))
	{
		if (IUEnhancedPlayerInfoInterface* Interface = Cast<IUEnhancedPlayerInfoInterface>(const_cast<UObject*>(ContextObject)))
		{
			PlayerName = Interface->GetPlayerName_Implementation();
			PlayerID = FString::FromInt(Interface->GetPlayerId_Implementation());
		}
	}
	else
	{
		const APlayerState* PlayerState = GetPlayerState(ContextObject);
		NetRole = PlayerState && PlayerState->GetPawn() ? UEnum::GetValueAsString(PlayerState->GetPawn()->GetLocalRole()) : TEXT("ROLE_None");
		PlayerName = PlayerState ? PlayerState->GetPlayerName() : TEXT("None");
		PlayerID = PlayerState ? FString::FromInt(PlayerState->GetPlayerId()) : TEXT("None");
	}
	
	if (UWorld const* World = GEngine->GetWorldFromContextObject(ContextObject, EGetWorldErrorMode::ReturnNull))
	{
		if(World->IsPlayInEditor() || World->IsPreviewWorld())
		{
			switch(World->GetNetMode())
			{
				case NM_DedicatedServer:   NetMode = TEXT("NM_DedicatedServer"); break;
				case NM_ListenServer:	   NetMode = TEXT("NM_ListenServer"); break;
				case NM_Client:            NetMode = FString::Printf(TEXT("NM_Client_%d"), static_cast<int32>(GPlayInEditorID)); break;
				case NM_Standalone:        NetMode = TEXT("NM_Standalone"); break;
				default:
					break;
			}
		}
		else if (World->IsGameWorld())
		{
			switch(World->GetNetMode())
			{
				case NM_DedicatedServer:   NetMode = TEXT("NM_DedicatedServer"); break;
				case NM_ListenServer:      NetMode = TEXT("NM_ListenServer"); break;
				case NM_Standalone:        NetMode = TEXT("NM_Standalone"); break;
				case NM_Client:            NetMode = TEXT("NM_Client"); break;
				default:
					break;
			}
		}
		else if (World->IsEditorWorld())
		{
			NetMode = TEXT("Editor");
		}
	}
	
	return FString::Format(
		TEXT("[{0}][{1}-{2}][{3}][{4}][{5}] "),
		{
			GFrameCounter % 1000,
			PlayerID,
			PlayerName,
			NetMode,
			NetRole,
			ContextObject ? ContextObject->GetName() : TEXT("Unknown"),
		}
	);
}

FString IUEnhancedPlayerInfoInterface::GetPlayerName_Implementation()
{
	return FString();
}

int64 IUEnhancedPlayerInfoInterface::GetPlayerId_Implementation()
{
	return 0;
}
