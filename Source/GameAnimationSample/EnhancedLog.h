#pragma once
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UObject/Interface.h"
#include "EnhancedLog.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API UEnhancedPlayerInfo : public UObject
{
	GENERATED_BODY()

public:
	static FString DefaultGetPlayerInfoString(const UObject* ContextObject);
};

UINTERFACE(Blueprintable)
class GAMEANIMATIONSAMPLE_API UUEnhancedPlayerInfoInterface : public UInterface
{
	GENERATED_BODY()

public:
};

class GAMEANIMATIONSAMPLE_API IUEnhancedPlayerInfoInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent)
	FString GetPlayerName();
	virtual FString GetPlayerName_Implementation();

	UFUNCTION(BlueprintNativeEvent)
	int64 GetPlayerId();
	virtual int64 GetPlayerId_Implementation();
};



// UE 5.8+: avoid private FStaticBasicLogRecord API; prefix player info via UE_LOG.
#define UE_LOG_ENHANCED(Category, Verbosity, ContextObject, Fmt, ...) \
	UE_LOG(Category, Verbosity, TEXT("%s") Fmt, *UEnhancedPlayerInfo::DefaultGetPlayerInfoString(ContextObject), ##__VA_ARGS__)

#define UE_LOG_GATED(GateVar, CategoryName, Verbosity, ContextObject, Fmt, ...) \
	do { if (GateVar) { UE_LOG_ENHANCED(CategoryName, Verbosity, ContextObject, Fmt, ##__VA_ARGS__); } } while (0)