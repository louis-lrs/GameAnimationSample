// Fill out your copyright notice in the Description page of Project Settings.


#include "GeCharacterMovementComponent.h"
#include "GeCharacterMovementReplication.h"

#include "EnhancedLog.h"
#include "GameAnimationSample.h"
#include "KismetTraceUtils.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/ScopedMovementUpdate.h"
#include "EngineLogs.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#include "Camera/PlayerCameraManager.h"
#include "DrawDebugLibrary.h"
#endif

static TAutoConsoleVariable<int32> CVarAnimSkillMovement_DebugDynamicCapsule(TEXT("a.AnimSkill.Movement.DebugDynamicCapsule"),0,TEXT("0: Disable, 1: Autonomous, 2: Client, 3: DedicatedServer, 4: Simulated Proxy, 5: All"));
static TAutoConsoleVariable<int32> CVarAnimSkillMovement_DebugMovement(TEXT("a.AnimSkill.Movement.DebugMovement"),0,TEXT("0: Disable, 1: Autonomous, 2: Client, 3: DedicatedServer, 4: Simulated Proxy, 5: All"));
static TAutoConsoleVariable<int32> CVarAnimSkillMovement_DebugClientID(TEXT("a.AnimSkill.Movement.DebugClientID"),-1,TEXT(""));
static TAutoConsoleVariable<int32> CVarAnimSkillMovement_DebugStanceCollision(TEXT("a.AnimSkill.Movement.DebugStanceCollision"),0,TEXT("0: Disable, 1: Enable"));

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
// Sub-toggles for DisplayDebugForGame, only evaluated when a.AnimSkill.Movement.DebugMovement is active
static TAutoConsoleVariable<bool> CVarAnimSkillMovement_DebugMovementShapes(
	TEXT("a.AnimSkill.Movement.DebugMovement.Shapes"), true,
	TEXT("Draw the rotation ring (control/actor/desired), velocity/acceleration/input arrows, capsule and floor normal"));
static TAutoConsoleVariable<bool> CVarAnimSkillMovement_DebugMovementPanel(
	TEXT("a.AnimSkill.Movement.DebugMovement.Panel"), true,
	TEXT("Draw the camera-facing movement state text panel"));
static TAutoConsoleVariable<bool> CVarAnimSkillMovement_DebugMovementBars(
	TEXT("a.AnimSkill.Movement.DebugMovement.Bars"), true,
	TEXT("Draw normalized speed/acceleration/jump apex progress bars"));
static TAutoConsoleVariable<int32> CVarAnimSkillMovement_DebugMovementHistory(
	TEXT("a.AnimSkill.Movement.DebugMovement.History"), 200,
	TEXT("Number of points kept in the movement trail. 0: Disable. Locally controlled characters only"));
static TAutoConsoleVariable<bool> CVarAnimSkillMovement_DebugMovementGraph(
	TEXT("a.AnimSkill.Movement.DebugMovement.Graph"), true,
	TEXT("Draw the 2D speed history graph above the character. Locally controlled characters only"));
#endif

static bool GDisplayLogCapsule = false;

namespace GeCharacterMovementCVars
{
	/** Whether to enable dynamic capsule size adjustment while jumping. */
	static bool bEnableDynamicCapusle = true;
	FAutoConsoleVariableRef CVarEnableDynamicCapusle(
		TEXT("a.AnimSkill.Movement.EnableDynamicCapusle"),
		bEnableDynamicCapusle,
		TEXT("Enable dynamic capsule size adjustment. 1 = enabled (default), 0 = disabled."));
    	
	/** Whether to enable capsule size change logging for debug purposes. */
	FAutoConsoleVariableRef CVarEnableLogCapsule(
		TEXT("a.AnimSkill.Movement.EnableLogCapsule"),
		GDisplayLogCapsule,
		TEXT("Enable verbose logging of capsule size changes. 1 = enabled, 0 = disabled (default)."));

	/** Controls the branch of SlideAlongSurface. */
	static int32 SlideNormalZFix = 0;
	FAutoConsoleVariableRef CVarSlideNormalZFix(
		TEXT("p.Ge.SlideNormalZFix"),
		SlideNormalZFix,
		TEXT("SlideAlongSurface NormalZ<0 fix mode:\n"
		     "  0 = Original (always ProjectToGravityFloor)\n"
		     "  1 = ConditionalProject (replace with floor normal when opposed, skip flatten; else flatten)\n"
		     "  2 = SkipFixup (skip NormalZ<0 fixup block entirely, no SlideDelta override)\n"
		     "  3 = TwoWallSlide (skip NormalZ<0 fixup block + override SlideDelta at concave corners)\n"
		     "  4 = TwoWallSlide (same as 3, enabled separately)"));

	/**
	 * 模式能力查询函数 —— 扩展新模式时只需修改这里，调用处无需改动。
	 *
	 * SlideMode_SkipsFixup        : 是否跳过 NormalZ<0 fixup 块
	 * SlideMode_UsesTwoWall       : 是否启用 TwoWallSlide SlideDelta 修正
	 * SlideMode_PreservesHorizSpeed: 是否在 TwoWallSlide 中保留水平速度
	 */
	static bool SlideMode_SkipsFixup(int32 Mode)
	{
		return Mode == 2 || Mode == 3 || Mode == 4;
	}

	static bool SlideMode_UsesTwoWall(int32 Mode)
	{
		return Mode == 3 || Mode == 4;
	}

	static bool SlideMode_PreservesHorizSpeed(int32 Mode)
	{
		return Mode == 4;
	}
}

// Helper function to check if debug should be enabled based on CVAR value and role
static bool ShouldEnableDebugForRole(int32 DebugMode, const AActor* Actor)
{
	if (DebugMode <= 0 || !Actor)
	{
		return false;
	}
	
	// Check client ID filter
	const auto ClientID = CVarAnimSkillMovement_DebugClientID.GetValueOnAnyThread();
	if (ClientID > 0)
	{
		if (UE::GetPlayInEditorID() != ClientID)
		{
			return false;
		}
	}
	
	const UWorld* World = Actor->GetWorld();
	if (!World)
	{
		return false;
	}
	
	const ENetRole LocalRole = Actor->GetLocalRole();
	const ENetMode NetMode = World->GetNetMode();
	
	// Filter by role based on debug mode
	if (DebugMode == 1)
	{
		// Autonomous/Authority only
		return (LocalRole == ROLE_AutonomousProxy && NetMode == NM_Client)
			|| (LocalRole == ROLE_Authority && NetMode == NM_Standalone);
	}
	else if (DebugMode == 2)
	{
		// Client/Standalone only
		return NetMode == NM_Client || NetMode == NM_Standalone;
	}
	else if (DebugMode == 3)
	{
		// DedicatedServer only
		return NetMode == NM_DedicatedServer;
	}
	else if (DebugMode == 4)
	{
		// SimulatedProxy only
		return LocalRole == ROLE_SimulatedProxy;
	}
	
	// Mode 5 or any other value: enable for all
	return true;
}

// Sets default values for this component's properties
UGeCharacterMovementComponent::UGeCharacterMovementComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	
	// Set custom network move data container for syncing AccumulatedJumpTime
	SetNetworkMoveDataContainer(GeNetworkMoveDataContainer);
}

// Called when the game starts
void UGeCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!HasValidData())
	{
		return;
	}
	CharacterOwner->OnReachedJumpApex.AddUniqueDynamic(this, &ThisClass::OnReachedJumpApex);
	CharacterOwner->LandedDelegate.AddUniqueDynamic(this, &ThisClass::OnLandedCallback);
}

// Called every frame
void UGeCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (!HasValidData())
	{
		return;
	}
	
	// Debug movement info
	const int32 theDebugMovement = CVarAnimSkillMovement_DebugMovement.GetValueOnAnyThread();
	if (ShouldEnableDebugForRole(theDebugMovement, CharacterOwner))
	{
		DisplayDebugForGame(DeltaTime);
	}
	
	// Debug dynamic capsule info
	const int32 theDebugDynamicCapsule = CVarAnimSkillMovement_DebugDynamicCapsule.GetValueOnAnyThread();
	if (ShouldEnableDebugForRole(theDebugDynamicCapsule, CharacterOwner))
	{
		const FString DebugInfo = GetDynamicCapsuleDebugInfo();
		// Use unique key by appending suffix to avoid conflict with DisplayDebugForGame
		const FString theObjectHash = FString::Printf(TEXT("%u_DynamicCapsule"), GetTypeHash(FObjectKey{this}));
		UKismetSystemLibrary::PrintString(this, DebugInfo, true, false, FLinearColor::White, 0.f, FName(*theObjectHash));
	}
	
	// if (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	// {
	// 	InterpMeshOffset(DeltaTime);
	// }
}

void UGeCharacterMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// ServerCapsuleStage replication settings:
	// - COND_SkipOwner: Don't replicate to owning client (they predict it)
	// - REPNOTIFY_OnChanged: Trigger OnRep only when value actually changes
	// - bIsPushBased = true: Support PushModel for efficient replication
	FDoRepLifetimeParams CapsuleStageParams;
	CapsuleStageParams.Condition = COND_SkipOwner;
	CapsuleStageParams.RepNotifyCondition = REPNOTIFY_OnChanged;
	CapsuleStageParams.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(UGeCharacterMovementComponent, ServerCapsuleStage, CapsuleStageParams);
}

FNetworkPredictionData_Client* UGeCharacterMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UGeCharacterMovementComponent* MutableThis = const_cast<UGeCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_GeCharacter(*this);
	}
	
	return ClientPredictionData;
}

FNetworkPredictionData_Server* UGeCharacterMovementComponent::GetPredictionData_Server() const
{
	if (ServerPredictionData == nullptr)
	{
		UGeCharacterMovementComponent* MutableThis = const_cast<UGeCharacterMovementComponent*>(this);
		MutableThis->ServerPredictionData = new FNetworkPredictionData_Server_GeCharacter(*this);
	}
	
	return ServerPredictionData;
}

bool UGeCharacterMovementComponent::CanDelaySendingMove(const FSavedMovePtr& NewMovePtr)
{
	// Call parent implementation first
	if (!Super::CanDelaySendingMove(NewMovePtr))
	{
		return false;
	}
	
	// Don't delay moves that change capsule stage over the course of the move
	if (const FSavedMove_GeCharacter* GeNewMove = static_cast<const FSavedMove_GeCharacter*>(NewMovePtr.Get()))
	{
		if (GeNewMove->StartCapsuleStage != GeNewMove->EndCapsuleStage)
		{
			return false;
		}
	}
	
	return true;
}

void UGeCharacterMovementComponent::ServerMove_PerformMovement(const FCharacterNetworkMoveData& MoveData)
{
	if (!HasValidData() || !IsActive())
	{
		return;
	}

	const float ClientTimeStamp = MoveData.TimeStamp;

	FVector ClientAccel = MoveData.Acceleration;

	// Convert the move's acceleration to worldspace if necessary
	if (MovementBaseUtility::IsDynamicBase(MoveData.MovementBase))
	{
		MovementBaseUtility::TransformDirectionToWorld(MoveData.MovementBase, MoveData.MovementBaseBoneName, MoveData.Acceleration, ClientAccel);
	}

	const uint8 ClientMoveFlags = MoveData.CompressedMoveFlags;
	const FRotator ClientControlRotation = MoveData.ControlRotation;

	FNetworkPredictionData_Server_Character* ServerData = GetPredictionData_Server_Character();
	check(ServerData);

	if (!VerifyClientTimeStamp(ClientTimeStamp, *ServerData))
	{
		const float ServerTimeStamp = ServerData->CurrentClientTimeStamp;
		// This is more severe if the timestamp has a large discrepancy and hasn't been recently reset.
		static constexpr float NetServerMoveTimestampExpiredWarningThreshold = 1.0f;
		if (ServerTimeStamp > 1.0f && FMath::Abs(ServerTimeStamp - ClientTimeStamp) > NetServerMoveTimestampExpiredWarningThreshold)
		{
			UE_LOG(LogNetPlayerMovement, Warning, TEXT("ServerMove: TimeStamp expired: %f, CurrentTimeStamp: %f, Character: %s"), ClientTimeStamp, ServerTimeStamp, *GetNameSafe(CharacterOwner));
		}
		else
		{
			UE_LOG(LogNetPlayerMovement, Log, TEXT("ServerMove: TimeStamp expired: %f, CurrentTimeStamp: %f, Character: %s"), ClientTimeStamp, ServerTimeStamp, *GetNameSafe(CharacterOwner));
		}
		return;
	}

	bool bServerReadyForClient = true;
	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	if (PC)
	{
		bServerReadyForClient = PC->NotifyServerReceivedClientData(CharacterOwner, ClientTimeStamp);
		if (!bServerReadyForClient)
		{
			ClientAccel = FVector::ZeroVector;
		}
	}

	const UWorld* MyWorld = GetWorld();
	const float DeltaTime = ServerData->GetServerMoveDeltaTime(ClientTimeStamp, CharacterOwner->GetActorTimeDilation(*MyWorld));

	// Defer all mesh child updates until all movement completes.
	FScopedMovementUpdate ScopedMeshUpdate(CharacterOwner->GetMesh(), EScopedUpdate::DeferredUpdates);

	if (DeltaTime > 0.f)
	{
		ServerData->CurrentClientTimeStamp = ClientTimeStamp;
		ServerData->ServerAccumulatedClientTimeStamp += DeltaTime;
		ServerData->ServerTimeStamp = MyWorld->GetTimeSeconds();
		ServerData->ServerTimeStampLastServerMove = ServerData->ServerTimeStamp;

		if (AController* CharacterController = Cast<AController>(CharacterOwner->GetController()))
		{
			CharacterController->SetControlRotation(ClientControlRotation);
		}

		if (!bServerReadyForClient)
		{
			return;
		}

		// Perform actual movement
		if ((MyWorld->GetWorldSettings()->GetPauserPlayerState() == nullptr))
		{
			if (const FGeCharacterNetworkMoveData* GeMoveData = static_cast<const FGeCharacterNetworkMoveData*>(&MoveData))
			{
				OnApplyJumpTimeData(*GeMoveData);
			}
			
			FScopedMovementUpdate ScopedCapsuleUpdate(bEnableScopedMovementUpdates ? UpdatedComponent : nullptr, EScopedUpdate::DeferredUpdates);
			if (PC)
			{
				PC->UpdateRotation(DeltaTime);
			}

			MoveAutonomous(ClientTimeStamp, DeltaTime, ClientMoveFlags, ClientAccel);
		}

		UE_CLOG(CharacterOwner && UpdatedComponent, LogNetPlayerMovement, VeryVerbose, TEXT("ServerMove Time %f Acceleration %s Velocity %s Position %s Rotation %s DeltaTime %f Mode %s MovementBase %s.%s (Dynamic:%d)"),
			ClientTimeStamp, *ClientAccel.ToString(), *Velocity.ToString(), *UpdatedComponent->GetComponentLocation().ToString(), *UpdatedComponent->GetComponentRotation().ToCompactString(), DeltaTime, *GetMovementName(),
			*GetNameSafe(GetMovementBase()), *CharacterOwner->GetBasedMovement().BoneName.ToString(), MovementBaseUtility::IsDynamicBase(GetMovementBase()) ? 1 : 0);
	}

	// Validate move only after old and first dual portion, after all moves are completed.
	if (MoveData.NetworkMoveType == FCharacterNetworkMoveData::ENetworkMoveType::NewMove)
	{
		ServerMoveHandleClientError(ClientTimeStamp, DeltaTime, ClientAccel, MoveData.Location, MoveData.MovementBase, MoveData.MovementBaseBoneName, MoveData.MovementMode);
	}
}

void UGeCharacterMovementComponent::OnApplyJumpTimeData(const FGeCharacterNetworkMoveData& GeMoveData)
{
	// 从 FFloat16 直接获取浮点值
	const float ReceivedActualJumpApexTime = GeMoveData.SavedActualJumpApexTime.GetFloat();
	const float OldActualJumpApexTime = ActualJumpApexTime;
	if (ReceivedActualJumpApexTime > UE_KINDA_SMALL_NUMBER)
	{
		if (ActualJumpApexTime <= UE_KINDA_SMALL_NUMBER || 
			FMath::Abs(ActualJumpApexTime - ReceivedActualJumpApexTime) > UE_KINDA_SMALL_NUMBER)
		{
			SetActualJumpApexTime(ReceivedActualJumpApexTime);
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Warning, this,
						 TEXT("[DynamicCapsule] ServerMove: Applied client ActualJumpApexTime: %.4f -> %.4f, Diff=%.4f"), 
						 OldActualJumpApexTime, ActualJumpApexTime, ActualJumpApexTime - OldActualJumpApexTime);
		}
	}

	if (FNetworkPredictionData_Server_GeCharacter* GeServerData = static_cast<FNetworkPredictionData_Server_GeCharacter*>(GetPredictionData_Server()))
	{
		const float ReceivedAccumulatedJumpTime = GeMoveData.SavedAccumulatedJumpTime.GetFloat();
		const float OldAccumulatedJumpTime = AccumulatedJumpTime;
		const float RealAccumulatedJumpTime = GeServerData->GetServerAccumulatedJumpTime(
			ReceivedAccumulatedJumpTime, 
			AccumulatedJumpTime);
		SetAccumulatedJumpTime(RealAccumulatedJumpTime);
		
		const float TimeDiff = FMath::Abs(RealAccumulatedJumpTime - ReceivedAccumulatedJumpTime);
		if (TimeDiff > 0.01f)
		{
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Warning, this,
				TEXT("[DynamicCapsule] ServerMove: AccumulatedJumpTime Sync: Client=%.4f, ServerOld=%.4f, ServerNew=%.4f, Diff=%.4f, CurrentStage=%s"),
				ReceivedAccumulatedJumpTime, OldAccumulatedJumpTime, RealAccumulatedJumpTime, TimeDiff,
				*UEnum::GetValueAsString(CurrentCapsuleStage));
		}
	}
}

void UGeCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	
	// If is not falling movement mode, interrupt with mode determined by target movement mode
	if (MovementMode != MOVE_Falling)
	{
		bNotifyApex = false;
		if (bIsDynamicCapsuleActive)
		{
			const bool bRestoreCapsule = ShouldRestoreCapsuleOnMovementModeChange(MovementMode, CustomMovementMode);
			InterruptDynamicCapsule(bRestoreCapsule);	
		}
	}
}

void UGeCharacterMovementComponent::AdjustFloorHeight()
{
	Super::AdjustFloorHeight();
}

bool UGeCharacterMovementComponent::DoJump(bool bReplayingMoves)
{
	bool bResult = Super::DoJump(bReplayingMoves);
	if (bResult)
	{
		bNotifyApex = true;
		OnDynamicCapsuleBegin();
	}
	return bResult;
}

void UGeCharacterMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	Super::ProcessLanded(Hit, remainingTime, Iterations);
	
	OnDynamicCapsuleEnd();
}

void UGeCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	
	UpdateDynamicCapsule(DeltaSeconds);
}

void UGeCharacterMovementComponent::OnReachedJumpApex()
{	
	ActualJumpApexTime = AccumulatedJumpTime;
	const float TimeDiff = ActualJumpApexTime - ExpectedJumpApexTime;
	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Warning, this,
		TEXT("[DynamicCapsule] %hs ExpectedJumpApexTime=%.4f, ActualJumpApexTime=%.4f, Diff=%.4f"),
		__FUNCTION__, ExpectedJumpApexTime, ActualJumpApexTime, TimeDiff);
}

void UGeCharacterMovementComponent::OnLandedCallback(const FHitResult& Hit)
{
}

#pragma region DynamicCapsule

float UGeCharacterMovementComponent::GetDefaultMeshZ() const
{
	constexpr float DefaultValue = -90.f;
	
	if (!HasValidData())
	{
		return DefaultValue;
	}
	
	// Retrieve the default relative Z location from the CDO's Mesh component
	if (ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>())
	{
		if (USkeletalMeshComponent* DefaultMesh = DefaultCharacter->GetMesh())
		{
			return DefaultMesh->GetRelativeLocation().Z;
		}
	}
	
	return DefaultValue;
}

float UGeCharacterMovementComponent::GetDefaultCapsuleHalfHeight() const
{
	constexpr float DefaultValue = 90.f;
	
	if (!HasValidData())
	{
		return DefaultValue;
	}
	
	// Retrieve the default unscaled half-height from the CDO's Capsule component
	if (ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>())
	{
		if (UCapsuleComponent* DefaultCapsule = DefaultCharacter->GetCapsuleComponent())
		{
			return DefaultCapsule->GetUnscaledCapsuleHalfHeight();
		}
	}
	return DefaultValue;
}

void UGeCharacterMovementComponent::OnRep_ServerCapsuleStage()
{
	// Simulated proxy receives capsule stage from server
	// Apply the stage change, which will handle mesh offset interpolation
	if (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		// Check if capsule was externally modified, skip this OnRep if so
		if (CheckAndInterruptIfExternallyModified())
		{
			return;
		}
		
		UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this, TEXT("[DynamicCapsule] %hs Received ServerCapsuleStage=%s"), 
			__FUNCTION__, *UEnum::GetValueAsString(ServerCapsuleStage));
		SetCapsuleStage(ServerCapsuleStage);
	}
}

FJumpStageConfig UGeCharacterMovementComponent::GetStageParams(EJumpCapsuleStage Stage) const
{
	// Return default "FullSize" config if no specific stage match
	FJumpStageConfig Config(0.f, 1.f, 0.f);

	switch (Stage)
	{
	case EJumpCapsuleStage::Stage1:
		Config = Stage1Config;
		break;
	case EJumpCapsuleStage::Stage2:
		Config = Stage2Config;
		break;
	case EJumpCapsuleStage::FullSize:
	default:
		break;
	}
	return Config;
}

EJumpCapsuleStage UGeCharacterMovementComponent::CalculateDesiredStage()
{
	EJumpCapsuleStage TargetStage = EJumpCapsuleStage::FullSize;
	if (Velocity.Z > 0.f)
	{
		// Safety check: prevent division by zero
		if (ExpectedJumpApexTime <= UE_KINDA_SMALL_NUMBER)
		{
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this, 
				TEXT("[DynamicCapsule] %hs Invalid ExpectedJumpApexTime=%.4f, returning FullSize"), __FUNCTION__, ExpectedJumpApexTime);
			return EJumpCapsuleStage::FullSize;
		}
		
		const float Progress = AccumulatedJumpTime / ExpectedJumpApexTime;
		if (bEnableStage2 && Progress >= Stage2Config.Threshold)
		{
			TargetStage = EJumpCapsuleStage::Stage2;
		}
		else if (bEnableStage1 && Progress >= Stage1Config.Threshold)
		{
			TargetStage = EJumpCapsuleStage::Stage1;
		}
		
		// Track max stage
		if (TargetStage > MaxReachedStage)
		{
			MaxReachedStage = TargetStage;
		}
		UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this, 
			TEXT("[DynamicCapsule] %hs Ascending: Progress=%.4f (Accumulated=%.4f, ExpectedJumpApex=%.4f), Vel=%s, Target=%s, MaxReached=%s"),
			__FUNCTION__, Progress, AccumulatedJumpTime, ExpectedJumpApexTime, *Velocity.ToCompactString(), *UEnum::GetValueAsString(TargetStage), *UEnum::GetValueAsString(MaxReachedStage));
	}
	else
	{
		// Don't enter a shrunk stage if we didn't reach it during ascent.
		if (MaxReachedStage == EJumpCapsuleStage::FullSize)
		{
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this,
				TEXT("[DynamicCapsule] %hs MaxReachedStage=FullSize, returning FullSize"), __FUNCTION__);
			return EJumpCapsuleStage::FullSize;
		}
		
		// Safety check: prevent division by zero
		if (ActualJumpApexTime <= UE_KINDA_SMALL_NUMBER)
		{
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this,
				TEXT("[DynamicCapsule] %hs Invalid ActualJumpApexTime=%.4f, returning FullSize"), __FUNCTION__, ActualJumpApexTime);
			return EJumpCapsuleStage::FullSize;
		}
		
		// Calculate falling thresholds symmetrically
		const float SymmetricStage2 = FMath::Max(1.f - Stage2Config.Threshold, 0.f);
		const float SymmetricStage1 = FMath::Max(1.f - Stage1Config.Threshold, 0.f);
		
		const float FallingProgress = FMath::Max((AccumulatedJumpTime - ActualJumpApexTime) / ActualJumpApexTime, 0.f);
		if (bEnableStage2 && FallingProgress < SymmetricStage2)
		{
			TargetStage = EJumpCapsuleStage::Stage2;
		}
		else if (bEnableStage1 && FallingProgress < SymmetricStage1)
		{
			TargetStage = EJumpCapsuleStage::Stage1;
		}
		
		// Clamp max stage (Don't enter Stage if we never reached it)
		if (TargetStage > MaxReachedStage)
		{
			TargetStage = MaxReachedStage;
		}
		UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this, 
			TEXT("[DynamicCapsule] %hs Descending: FallingProgress=%.4f (Accumulated=%.4f, ActualJumpApex=%.4f), Vel=%s, Target=%s, MaxReached=%s"), 
			__FUNCTION__, FallingProgress, AccumulatedJumpTime, ActualJumpApexTime, *Velocity.ToCompactString(), *UEnum::GetValueAsString(TargetStage), *UEnum::GetValueAsString(MaxReachedStage));
	}
	return TargetStage;
}

bool UGeCharacterMovementComponent::SetCapsuleStage(EJumpCapsuleStage NewCapsuleStage)
{
	if (!HasValidData())
	{
		return false;
	}
	
	if (CurrentCapsuleStage == NewCapsuleStage)
	{
		return true;
	}

	const UWorld* MyWorld = GetWorld();
	if (MyWorld == nullptr) { return false; }

	UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	if (Capsule == nullptr) { return false; }
	
	USkeletalMeshComponent* CharacterMesh = CharacterOwner->GetMesh();
	if (CharacterMesh == nullptr) { return false; }
	
	/* ------------------------ Setup Parameters ------------------------ */
	const FJumpStageConfig TargetConfig = GetStageParams(NewCapsuleStage);
	const FJumpStageConfig CurrentConfig = GetStageParams(CurrentCapsuleStage);
	
	// Calculate the delta to adjust the mesh visual position
	const float DefaultHalfHeight = GetDefaultCapsuleHalfHeight();
	const float NewHalfHeight = DefaultHalfHeight * TargetConfig.ShrinkRatio;
	const float OldHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	const float ComponentScale = Capsule->GetShapeScale();
	
	// Precision check to avoid unnecessary physics updates
	if (FMath::IsNearlyZero(NewHalfHeight) || FMath::IsNearlyZero(ComponentScale))
	{
		return false;
	}
	
	// Keep Top Fixed. If shrinking (Old > New), we move UP (+) by the difference.
	const float HalfHeightAdjust = OldHalfHeight - NewHalfHeight;
	// Transitioning from CurrentCapsuleStage to NewCapsuleStage.
	const float ManualOffsetDelta = TargetConfig.CapsuleOffset - CurrentConfig.CapsuleOffset;
	// Add OffsetDelta. Positive moves the whole capsule UP.
	const float TotalZDelta = HalfHeightAdjust + ManualOffsetDelta;
	
	// Precision check to avoid unnecessary physics updates
	if (FMath::IsNearlyZero(HalfHeightAdjust) && FMath::IsNearlyZero(TotalZDelta))
	{
		CurrentCapsuleStage = NewCapsuleStage;
		return true;
	}

	/* ------------------------ Physics Probe ------------------------ */
	const float ScaledTotalZDelta = TotalZDelta * ComponentScale;
	const float ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;
	const float ScaledNewHalfHeight = NewHalfHeight * ComponentScale;
	const float ScaledOldHalfHeight = OldHalfHeight * ComponentScale;

	const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
	const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();
	FVector ProposedLocation = PawnLocation + FVector(0.f, 0.f, ScaledTotalZDelta);
	
	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this, 
		TEXT("[DynamicCapsule] %hs Pending: %s -> %s, OldHalfHeight=%.2f, NewHalfHeight=%.2f, AccumulatedJumpTime=%.4f"), __FUNCTION__,
		*UEnum::GetValueAsString(CurrentCapsuleStage), *UEnum::GetValueAsString(NewCapsuleStage), OldHalfHeight, NewHalfHeight, AccumulatedJumpTime);
	
    const bool bIsSimulatedProxy = (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy);
    if (!bIsSimulatedProxy)
    {
	    UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this,
	    	TEXT("[DynamicCapsule] %hs CollisionCheck: PawnLocation=%s, ProposedLocation=%s, ScaledTotalZDelta=%.4f"),
		    __FUNCTION__, *PawnLocation.ToCompactString(), *ProposedLocation.ToCompactString(), ScaledTotalZDelta);
	    
	    constexpr float SweepInflation = UE_KINDA_SMALL_NUMBER * 10.f; // Preventing precision error

    	// Use current capsule for Sweeping
    	const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
    	const FCollisionShape CurrentCapsuleShape = Capsule->GetCollisionShape();
    	// Shrink radius by SWEEP_EDGE_REJECT_DISTANCE to ignore edge-grazing lateral walls
    	const float ShrunkRadius = FMath::Max(0.f, CurrentCapsuleShape.GetCapsuleRadius() - SWEEP_EDGE_REJECT_DISTANCE);
    	const FCollisionShape SweepCapsuleShape = FCollisionShape::MakeCapsule(ShrunkRadius, CurrentCapsuleShape.GetCapsuleHalfHeight());
    	const FCollisionShape TargetCapsuleShape = FCollisionShape::MakeCapsule(ShrunkRadius, ScaledNewHalfHeight);
    	
	    FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(DynamicCapsuleTrace), false, CharacterOwner);
	    FCollisionResponseParams ResponseParam;
	    InitCollisionParams(CapsuleParams, ResponseParam);
    	
    	// Check for floor collision when moving capsule downwards
	    const float CurrentBottomZ = PawnLocation.Z - ScaledOldHalfHeight;
	    const float ProposedBottomZ = ProposedLocation.Z - ScaledNewHalfHeight;
	    const float BottomDropDist = CurrentBottomZ - ProposedBottomZ;
	    if (BottomDropDist > KINDA_SMALL_NUMBER)
	    {
		    FHitResult FloorHit;
		    FVector Start = PawnLocation;
		    FVector End = Start - FVector(0.f, 0.f, BottomDropDist + SweepInflation);
		    const bool bHitResult = MyWorld->SweepSingleByChannel(FloorHit, Start, End, FQuat::Identity, CollisionChannel, SweepCapsuleShape, CapsuleParams, ResponseParam);

#if ENABLE_DRAW_DEBUG
		    if (CVarAnimSkillMovement_DebugStanceCollision.GetValueOnAnyThread() > 0)
		    {
		    	DrawDebugCapsuleTraceSingle(MyWorld, Start, End, SweepCapsuleShape.GetCapsuleRadius(), SweepCapsuleShape.GetCapsuleHalfHeight(),
		    		EDrawDebugTrace::Type::ForDuration, bHitResult, FloorHit, FLinearColor::Green, FLinearColor::Red, 3.f);
		    }
#endif

		    if (bHitResult)
		    {
		    	// Adjust proposed location to avoid floor collision
			    ProposedLocation.Z = FloorHit.Location.Z - ScaledHalfHeightAdjust + MAX_FLOOR_DIST;
		    	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Error, this, TEXT("[DynamicCapsule] %hs Floor Hit! Adjusted ProposedLocation=%s, ImpactPoint=%s, BottomDropDist=%.4f, HitActor=%s, HitComponent=%s"), 
			    	__FUNCTION__, *ProposedLocation.ToCompactString(), *FloorHit.ImpactPoint.ToCompactString(), BottomDropDist, *GetNameSafe(FloorHit.GetActor()), *GetNameSafe(FloorHit.GetComponent()));
		    }
	    }

    	// Check for ceiling collision when moving capsule upwards
	    const float CurrentTopZAtProposed = ProposedLocation.Z + ScaledOldHalfHeight;
	    const float ProposedTopZ = ProposedLocation.Z + ScaledNewHalfHeight;
	    const float TopLiftDist = ProposedTopZ - CurrentTopZAtProposed;
	    if (TopLiftDist > KINDA_SMALL_NUMBER)
	    {
		    FHitResult CeilingHit;
		    FVector Start = ProposedLocation;
		    FVector End = Start + FVector(0.f, 0.f, TopLiftDist + SweepInflation);
		    const bool bHitResult = MyWorld->SweepSingleByChannel(CeilingHit, Start, End, FQuat::Identity, CollisionChannel, SweepCapsuleShape, CapsuleParams, ResponseParam);

#if ENABLE_DRAW_DEBUG
		    if (CVarAnimSkillMovement_DebugStanceCollision.GetValueOnAnyThread() > 0)
		    {
		    	DrawDebugCapsuleTraceSingle(MyWorld, Start, End, SweepCapsuleShape.GetCapsuleRadius(), SweepCapsuleShape.GetCapsuleHalfHeight(),
		    		EDrawDebugTrace::Type::ForDuration, bHitResult, CeilingHit, FLinearColor::Green, FLinearColor::Red, 3.f);
		    }
#endif

		    if (bHitResult)
		    {
		    	// Adjust proposed location to avoid ceiling collision
			    ProposedLocation.Z = CeilingHit.Location.Z + ScaledHalfHeightAdjust - MAX_FLOOR_DIST;
			    UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Error, this, TEXT("[DynamicCapsule] %hs CeilingHit Hit! Adjusted ProposedLocation=%s, ImpactPoint=%s, TopLiftDist=%.4f, HitActor=%s, HitComponent=%s"), 
				    __FUNCTION__, *ProposedLocation.ToCompactString(), *CeilingHit.ImpactPoint.ToCompactString(), TopLiftDist, *GetNameSafe(CeilingHit.GetActor()), *GetNameSafe(CeilingHit.GetComponent()));
		    }
	    }

    	// Use target capsule for proposed location blocking test
	    FCollisionQueryParams VerifyParams(SCENE_QUERY_STAT(DynamicCapsuleVerify), false, CharacterOwner);
	    FCollisionResponseParams VerifyResponse;
	    InitCollisionParams(VerifyParams, VerifyResponse);
	    const bool bEncroached = MyWorld->OverlapBlockingTestByChannel(ProposedLocation, PawnRotation, CollisionChannel, TargetCapsuleShape, VerifyParams, VerifyResponse);

#if ENABLE_DRAW_DEBUG
	    if (CVarAnimSkillMovement_DebugStanceCollision.GetValueOnAnyThread() > 0)
	    {
		    const FTransform ActorTransform = GetActorTransform();
		    const FVector LocalOffset = ActorTransform.TransformVectorNoScale(FVector(0, 50, 0));
	    	const FVector FinalLocation = ProposedLocation + LocalOffset;
		    FHitResult theHitResult;
		    theHitResult.bBlockingHit = bEncroached;
		    theHitResult.Location = FinalLocation;
	    	DrawDebugCapsuleTraceSingle(MyWorld, FinalLocation, FinalLocation, TargetCapsuleShape.GetCapsuleRadius(), TargetCapsuleShape.GetCapsuleHalfHeight()
				 , EDrawDebugTrace::ForDuration, bEncroached, theHitResult, FLinearColor::Blue, FLinearColor::Red, 3.f);
	    }
#endif

	    if (bEncroached)
	    {
	    	// Do not perform capsule change if encorached eventually
		    UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Warning, this, TEXT("[DynamicCapsule] %hs FAILED: Encroached (Ceilling/Floor) at ProposedLocation=%s"), __FUNCTION__, *ProposedLocation.ToCompactString());
		    return false;
	    }
	    
	    UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this, TEXT("[DynamicCapsule] %hs CollisionCheck PASSED: ProposedLocation=%s"), __FUNCTION__, *ProposedLocation.ToCompactString());
    }

	/* ------------------------ Commit Changes ------------------------ */
	CurrentCapsuleStage = NewCapsuleStage;
	if (CharacterOwner->HasAuthority())
	{
		ServerCapsuleStage = NewCapsuleStage;
		
		// Support PushModel: Mark property dirty to trigger replication
		MARK_PROPERTY_DIRTY_FROM_NAME(UGeCharacterMovementComponent, ServerCapsuleStage, this);
		
		// Force network update to prevent replication delay causing visual desync
		CharacterOwner->ForceNetUpdate();
		
		UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this, TEXT("[DynamicCapsule] %hs SUCCEED (Server): %s, AccumulatedJumpTime=%.4f"), 
			__FUNCTION__, *UEnum::GetValueAsString(NewCapsuleStage), AccumulatedJumpTime);
	}
	else
	{
		UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this, TEXT("[DynamicCapsule] %hs SUCCEED (Client): %s, AccumulatedJumpTime=%.4f"), 
			__FUNCTION__, *UEnum::GetValueAsString(NewCapsuleStage), AccumulatedJumpTime);
	}

	// 1. Record state BEFORE changes for Visual Compensation
	const float PreActionCapsuleZ = UpdatedComponent->GetComponentLocation().Z;
	const float PreActionMeshRelZ = CharacterMesh->GetRelativeLocation().Z;
	
	if (bIsSimulatedProxy)
	{
		// 2.1 Apply Size Change
		Capsule->SetCapsuleHalfHeight(NewHalfHeight, true);
		bShrinkProxyCapsule = true;
	}
	else if (!bIsSimulatedProxy)
	{
		// 2.2 Apply Size Change
		Capsule->SetCapsuleHalfHeight(NewHalfHeight, true);
	
		// 3. Move Capsule
		const FVector MoveDelta = ProposedLocation - PawnLocation;
		const bool bMoveResult = UpdatedComponent->MoveComponent(MoveDelta, PawnRotation, false, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
		
		if (!bMoveResult)
		{
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Error, this, TEXT("[DynamicCapsule] %hs MoveComponent FAILED: MoveDelta=%s, ProposedLocation=%s, PawnLocation=%s"),
				__FUNCTION__, *MoveDelta.ToCompactString(), *ProposedLocation.ToCompactString(), *PawnLocation.ToCompactString());
		}
		
		// Force update mesh transform in case not propagated because of IsDeferringMovementUpdates
		UpdatedComponent->UpdateChildTransforms();
		
		const FVector PostMoveLocation = UpdatedComponent->GetComponentLocation();
		if (!PostMoveLocation.Equals(ProposedLocation, 1.0f))
		{
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Error, this, TEXT("[DynamicCapsule] %hs Location Mismatch: Proposed=%s, Actual=%s, Delta=%s"),
				__FUNCTION__, *ProposedLocation.ToCompactString(), *PostMoveLocation.ToCompactString(), *(PostMoveLocation - ProposedLocation).ToCompactString());
		}
	}
	
	bForceNextFloorCheck = true;
	AdjustProxyCapsuleSize();
	
	// Record expected capsule half height for detecting external modifications
	ExpectedCapsuleHalfHeight = NewHalfHeight;

	// 4. Record state AFTER changes for Visual Compensation
	const float PostActionCapsuleZ = UpdatedComponent->GetComponentLocation().Z;
	const float WorldMoveDelta = PostActionCapsuleZ - PreActionCapsuleZ;
	
	// 5. Visual Compensation
	const float DefaultMeshZ = GetDefaultMeshZ();
	const float TotalShrinkAmount = DefaultHalfHeight - NewHalfHeight;
	const float TotalCenterShiftUp = TotalShrinkAmount + TargetConfig.CapsuleOffset;
	const float IdealMeshZ = DefaultMeshZ - TotalCenterShiftUp;

	FVector MeshRelativeLocation = CharacterMesh->GetRelativeLocation();
	
	// For simulated proxies, capsule position is controlled by server replication
	// WorldMoveDelta should be 0 or minimal, so we directly calculate mesh offset
	float CompensatedMeshZ;
	if (bIsSimulatedProxy)
	{
		// Simulated proxy: capsule position is synced from server, no world movement
		// Directly calculate mesh offset based on capsule size change
		CompensatedMeshZ = PreActionMeshRelZ;
	}
	else
	{
		// Authority/Autonomous: compensate for world movement
		CompensatedMeshZ = PreActionMeshRelZ - WorldMoveDelta;
	}
	
	if (FMath::IsNearlyEqual(CompensatedMeshZ, IdealMeshZ, 0.1f))
	{
		MeshRelativeLocation.Z = IdealMeshZ;
		CharacterMesh->SetRelativeLocation(MeshRelativeLocation);
		TargetMeshZOffset.Reset();
	}
	else
	{
		MeshRelativeLocation.Z = CompensatedMeshZ;
		CharacterMesh->SetRelativeLocation(MeshRelativeLocation);
		TargetMeshZOffset = IdealMeshZ;
	}
	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this, TEXT("[DynamicCapsule] %hs SUCCEED: %s, MeshLoc=%s, IdealMeshZ=%.2f, CompensatedMeshZ=%.2f"),
		__FUNCTION__, *UEnum::GetValueAsString(NewCapsuleStage), *CharacterMesh->GetRelativeLocation().ToCompactString(), IdealMeshZ, CompensatedMeshZ);
	
	if (FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character())
	{
		// Don't smooth this change in mesh position
		// if (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy || (IsNetMode(NM_ListenServer) && CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy))
		// {
		// 	ClientData->MeshTranslationOffset += ScaledHalfHeightAdjust * -GetGravityDirection();
		// 	ClientData->OriginalMeshTranslationOffset = ClientData->MeshTranslationOffset;
		// }
		
		// Don't combine this change
		if (CharacterOwner->IsLocallyControlled())
		{
			if (FSavedMove_Character* const PendingMove = ClientData->PendingMove.Get())
			{
				PendingMove->bForceNoCombine = true;
			}
		}
	}
	return true;
}

void UGeCharacterMovementComponent::InterpMeshOffset(float DeltaTime)
{
	if (!HasValidData())
	{
		return;
	}
	
	if (!TargetMeshZOffset.IsSet())
	{
		return;
	}
	
	USkeletalMeshComponent* CharacterMesh = CharacterOwner->GetMesh();
	if (CharacterMesh == nullptr) { return; }

	FVector MeshRelativeLocation = CharacterMesh->GetRelativeLocation();
	const float CurrentZ = MeshRelativeLocation.Z;
	const float DesiredZ = TargetMeshZOffset.GetValue();
	
	float NewMeshZ = FMath::FInterpTo(CurrentZ, DesiredZ, DeltaTime, InterpMeshSpeed);
	if (FMath::IsNearlyEqual(NewMeshZ, DesiredZ, 0.1f))
	{
		NewMeshZ = DesiredZ;
		TargetMeshZOffset.Reset();
	}
	MeshRelativeLocation.Z = NewMeshZ;
	CharacterMesh->SetRelativeLocation(MeshRelativeLocation);
	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this, TEXT("[DynamicCapsule] %hs MeshLoc=%s, DesiredZ=%.2f, DeltaTime=%.6f"),
		__FUNCTION__, *CharacterMesh->GetRelativeLocation().ToCompactString(), DesiredZ, DeltaTime);
}

void UGeCharacterMovementComponent::UpdateDynamicCapsule(float DeltaSeconds)
{
	if (!(bEnableDynamicCapsule && GeCharacterMovementCVars::bEnableDynamicCapusle))
	{
		return;
	}
	
	if (!HasValidData())
	{
		return;
	}

	// Get actual frame delta time instead of potentially combined move delta
	const float ActualDeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : DeltaSeconds;
	
	ON_SCOPE_EXIT 
	{ 
		InterpMeshOffset(ActualDeltaTime); 
	};
	
	const bool bIsSimulatedProxy = (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy);
	
	// Simulated proxies only handle mesh offset interpolation
	// Capsule stage changes are handled via OnRep_ServerCapsuleStage
	if (bIsSimulatedProxy)
	{
		return;
	}
	
	// Handle pending capsule restoration
	if (bPendingCapsuleRestore)
	{
		const bool bRestoreResult = SetCapsuleStage(EJumpCapsuleStage::FullSize);
		if (bRestoreResult)
		{
			bPendingCapsuleRestore = false;
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this, TEXT("[DynamicCapsule] %hs Successfully restored capsule"), __FUNCTION__);
		}
		return;
	}
	
	if (!bIsDynamicCapsuleActive)
	{
		return;
	}
	
	// Check for external capsule modifications
	if (CheckAndInterruptIfExternallyModified())
	{
		return;
	}
	
	// Handle root motion: use restore mode
	if (HasRootMotionSources())
	{
		InterruptDynamicCapsule(true);
		return;
	}
	
	if (IsFalling())
	{
		const float OldAccumulatedJumpTime = AccumulatedJumpTime;
		AccumulatedJumpTime += ActualDeltaTime;
		const EJumpCapsuleStage DesiredStage = CalculateDesiredStage();
		const bool bSetStageResult = SetCapsuleStage(DesiredStage);
		
		UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Verbose, this,
				TEXT("[DynamicCapsule] %hs AccumulatedJumpTime=%.4f->%.4f, ActualDeltaTime=%.4f (ParamDeltaTime=%.4f)"),
				__FUNCTION__, OldAccumulatedJumpTime, AccumulatedJumpTime, ActualDeltaTime, DeltaSeconds);
		
		if (!bSetStageResult && DesiredStage != CurrentCapsuleStage)
		{
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Warning, this,
				TEXT("[DynamicCapsule] %hs SetCapsuleStage FAILED: DesiredStage=%s, CurrentStage=%s, AccumulatedJumpTime=%.4f->%.4f, Location=%s"),
				__FUNCTION__, *UEnum::GetValueAsString(DesiredStage), *UEnum::GetValueAsString(CurrentCapsuleStage),
				OldAccumulatedJumpTime, AccumulatedJumpTime, *UpdatedComponent->GetComponentLocation().ToCompactString());
		}
	}
	else
	{
		ResetDynamicCapsule();
	}
}

void UGeCharacterMovementComponent::ResetDynamicCapsule()
{
	if (!(bEnableDynamicCapsule && GeCharacterMovementCVars::bEnableDynamicCapusle))
	{
		return;
	}
	
	if (!HasValidData())
	{
		return;
	}
	
	// Only reset if active or has pending restore, otherwise already interrupted
	if (!bIsDynamicCapsuleActive && !bPendingCapsuleRestore)
	{
		return;
	}
	
	const bool bResetResult = SetCapsuleStage(EJumpCapsuleStage::FullSize);
	if (bResetResult)
	{
		SetDynamicCapsuleActive(false);
		return;
	}
	
	if (!bResetResult && CharacterOwner->IsLocallyControlled() && IsMovingOnGround())
	{
		CharacterOwner->Crouch(false);
		// UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(CharacterOwner.Get(), GetDefault<US1GameTagSettings>()->InputCrouchTag, FGameplayEventData());	
	}
	
	if (IsCrouching())
	{
		SetDynamicCapsuleActive(false);
	}
}

void UGeCharacterMovementComponent::ClearDynamicCapsuleState()
{
	AccumulatedJumpTime = 0.f;
	ActualJumpApexTime = 0.f;
	ExpectedJumpApexTime = 0.f;
    
	// Reset stage trackers
	MaxReachedStage = EJumpCapsuleStage::FullSize;
	
	// Clear pending restore flag and expected capsule size
	bPendingCapsuleRestore = false;
	ExpectedCapsuleHalfHeight = 0.f;
}

void UGeCharacterMovementComponent::OnDynamicCapsuleBegin()
{
	if (!(bEnableDynamicCapsule && GeCharacterMovementCVars::bEnableDynamicCapusle))
	{
		return;
	}
	
	if (!HasValidData())
	{
		return;
	}
	
	// Reset capsule stage to FullSize before starting new jump
	// This ensures clean state even if previous jump was interrupted
	if (CurrentCapsuleStage != EJumpCapsuleStage::FullSize)
	{
		UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Warning, this, TEXT("[DynamicCapsule] %hs Resetting from %s to FullSize"), 
			__FUNCTION__, *UEnum::GetValueAsString(CurrentCapsuleStage));
		CurrentCapsuleStage = EJumpCapsuleStage::FullSize;
	}
	
	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Warning, this, TEXT("[DynamicCapsule] %hs"), __FUNCTION__);
	
	SetDynamicCapsuleActive(true);
	
	// Initialize expected capsule half height
	UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	if (Capsule != nullptr)
	{
		ExpectedCapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	}

	const float theGravityZ = GetGravityZ();
	if (theGravityZ < 0.f)
	{
		const float ActualInitialVelocityZ = Velocity.Z;
		const float InitialVelocityZ = (ActualInitialVelocityZ > UE_KINDA_SMALL_NUMBER)  ? ActualInitialVelocityZ : JumpZVelocity;
		ExpectedJumpApexTime = FMath::Abs(InitialVelocityZ / theGravityZ);
	}
	else
	{
		ExpectedJumpApexTime = 0.f;
	}
}

void UGeCharacterMovementComponent::OnDynamicCapsuleEnd()
{
	if (!(bEnableDynamicCapsule && GeCharacterMovementCVars::bEnableDynamicCapusle))
	{
		return;
	}
	
	if (!HasValidData())
	{
		return;
	}
	
	ResetDynamicCapsule();
	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Warning, this, TEXT("[DynamicCapsule] %hs"), __FUNCTION__);
}

void UGeCharacterMovementComponent::SetDynamicCapsuleActive(bool bActive)
{
	if (bIsDynamicCapsuleActive == bActive)
	{
		return;
	}
	bIsDynamicCapsuleActive = bActive;
	UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this, TEXT("[DynamicCapsule] %hs bActive=%d"), __FUNCTION__, bActive);
	
	ClearDynamicCapsuleState();
}

void UGeCharacterMovementComponent::InterruptDynamicCapsule(bool bRestoreCapsule)
{
	if (!bIsDynamicCapsuleActive && !bPendingCapsuleRestore)
	{
		// If bRestoreCapsule is false, clear TargetMeshZOffset even if early returning
		if (!bRestoreCapsule)
		{
			TargetMeshZOffset.Reset();
		}
		return;
	}
	
	// Disable dynamic capsule adjustment
	SetDynamicCapsuleActive(false);
	
	if (bRestoreCapsule)
	{
		// Attempt to restore capsule immediately
		const bool bRestoreResult = SetCapsuleStage(EJumpCapsuleStage::FullSize);
		if (bRestoreResult)
		{
			// Successfully restored, clear the pending flag
			bPendingCapsuleRestore = false;
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this, TEXT("[DynamicCapsule] %hs Interrupted and restored capsule immediately"), __FUNCTION__);
		}
		else
		{
			// Failed to restore, set flag to try every frame
			bPendingCapsuleRestore = true;
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Warning, this, TEXT("[DynamicCapsule] %hs Interrupted but failed to restore capsule, will retry every frame"), __FUNCTION__);
		}
	}
	else
	{
		// Clear pending restore flag
		bPendingCapsuleRestore = false;
		// Clear mesh offset target to stop interpolation in clear-only mode
		// Always clear TargetMeshZOffset when bRestoreCapsule is false
		TargetMeshZOffset.Reset();
		UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this, TEXT("[DynamicCapsule] %hs Interrupted and cleared runtime data"), __FUNCTION__);
	}
}

bool UGeCharacterMovementComponent::CheckAndInterruptIfExternallyModified()
{
	UCapsuleComponent* Capsule = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
	if (Capsule != nullptr)
	{
		const float CurrentHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
		if (!FMath::IsNearlyEqual(CurrentHalfHeight, ExpectedCapsuleHalfHeight, 0.01f))
		{
			// External modification detected, interrupt DynamicCapsule and notify caller to skip
			InterruptDynamicCapsule(false);
			UE_LOG_GATED(GDisplayLogCapsule, LogGeCharacterMovement, Log, this,
				TEXT("[DynamicCapsule] %hs External capsule modification detected (Current: %.2f, Expected: %.2f), interrupting"),
				__FUNCTION__, CurrentHalfHeight, ExpectedCapsuleHalfHeight);
			return true;
		}
	}
	
	return false;
}

FString UGeCharacterMovementComponent::GetDynamicCapsuleDebugInfo() const
{
	// Early return if no valid data
	if (!HasValidData())
	{
		return FString();
	}
	
	// Pre-allocate string to avoid multiple reallocations
	FString DebugInfo;
	DebugInfo.Reserve(1024);
	
	// Get runtime environment info
	const int32 FrameNumber = GFrameNumber % 1000;
	const ENetRole LocalRole = CharacterOwner->GetLocalRole();
	const ENetRole RemoteRole = CharacterOwner->GetRemoteRole();
	const FString NetModeStr = GetNetMode() == NM_Standalone ? TEXT("Standalone") :
	                           GetNetMode() == NM_Client ? TEXT("Client") :
	                           GetNetMode() == NM_DedicatedServer ? TEXT("DedicatedServer") :
	                           GetNetMode() == NM_ListenServer ? TEXT("ListenServer") : TEXT("Unknown");
	
	// Build debug info using Appendf for better performance
	DebugInfo.Appendf(TEXT("=== Dynamic Capsule Debug Info ===\n"));
	DebugInfo.Appendf(TEXT("Frame: %d | NetMode: %s | LocalRole: %s | RemoteRole: %s\n"), 
		FrameNumber, *NetModeStr, *UEnum::GetValueAsString(LocalRole), *UEnum::GetValueAsString(RemoteRole));
	DebugInfo.Appendf(TEXT("Enabled: %s | Active: %s | Pending Restore: %s\n"), 
		bEnableDynamicCapsule ? TEXT("Yes") : TEXT("No"),
		bIsDynamicCapsuleActive ? TEXT("Yes") : TEXT("No"),
		bPendingCapsuleRestore ? TEXT("Yes") : TEXT("No"));
	
	DebugInfo.Appendf(TEXT("\n--- Stage Info ---\n"));
	DebugInfo.Appendf(TEXT("Current: %s | Max Reached: %s | Server: %s\n"), 
		*UEnum::GetValueAsString(CurrentCapsuleStage),
		*UEnum::GetValueAsString(MaxReachedStage),
		*UEnum::GetValueAsString(ServerCapsuleStage));
	
	DebugInfo.Appendf(TEXT("\n--- Timing Info ---\n"));
	DebugInfo.Appendf(TEXT("Accumulated: %.4f | Expected Apex: %.4f | Actual Apex: %.4f\n"), 
		AccumulatedJumpTime, ExpectedJumpApexTime, ActualJumpApexTime);
	
	DebugInfo.Appendf(TEXT("\n--- Capsule Info ---\n"));
	UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	if (Capsule != nullptr)
	{
		const float CurrentHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
		const float DefaultHalfHeight = GetDefaultCapsuleHalfHeight();
		const float Ratio = DefaultHalfHeight > 0.f ? (CurrentHalfHeight / DefaultHalfHeight * 100.f) : 0.f;
		DebugInfo.Appendf(TEXT("Current: %.2f | Default: %.2f | Expected: %.2f | Ratio: %.2f%%\n"), 
			CurrentHalfHeight, DefaultHalfHeight, ExpectedCapsuleHalfHeight, Ratio);
	}
	
	DebugInfo.Appendf(TEXT("\n--- Mesh Offset Info ---\n"));
	if (TargetMeshZOffset.IsSet())
	{
		const float DefaultMeshZ = GetDefaultMeshZ();
		const float TargetZ = TargetMeshZOffset.GetValue();
		if (CharacterOwner->GetMesh())
		{
			const float CurrentMeshZ = CharacterOwner->GetMesh()->GetRelativeLocation().Z;
			DebugInfo.Appendf(TEXT("Target: %.2f | Default: %.2f | Current: %.2f | Delta: %.2f\n"), 
				TargetZ, DefaultMeshZ, CurrentMeshZ, TargetZ - CurrentMeshZ);
		}
		else
		{
			DebugInfo.Appendf(TEXT("Target: %.2f | Default: %.2f\n"), TargetZ, DefaultMeshZ);
		}
	}
	else
	{
		DebugInfo.Append(TEXT("No Target Mesh Offset\n"));
	}
	
	DebugInfo.Appendf(TEXT("\n--- Movement Info ---\n"));
	DebugInfo.Appendf(TEXT("Mode: %s | Custom: %d | Falling: %s | Root Motion: %s\n"), 
		*UEnum::GetValueAsString(MovementMode),
		CustomMovementMode,
		IsFalling() ? TEXT("Yes") : TEXT("No"),
		HasRootMotionSources() ? TEXT("Yes") : TEXT("No"));
	
	DebugInfo.Appendf(TEXT("\n--- Configuration ---\n"));
	DebugInfo.Appendf(TEXT("Stage1: %s"), bEnableStage1 ? TEXT("Yes") : TEXT("No"));
	if (bEnableStage1)
	{
		DebugInfo.Appendf(TEXT(" (T:%.2f R:%.2f O:%.2f)"), 
			Stage1Config.Threshold, Stage1Config.ShrinkRatio, Stage1Config.CapsuleOffset);
	}
	DebugInfo.Append(TEXT("\n"));
	DebugInfo.Appendf(TEXT("Stage2: %s"), bEnableStage2 ? TEXT("Yes") : TEXT("No"));
	if (bEnableStage2)
	{
		DebugInfo.Appendf(TEXT(" (T:%.2f R:%.2f O:%.2f)"), 
			Stage2Config.Threshold, Stage2Config.ShrinkRatio, Stage2Config.CapsuleOffset);
	}
	DebugInfo.Appendf(TEXT("\nInterp Speed: %.2f\n"), InterpMeshSpeed);
	
	DebugInfo.Append(TEXT("========================\n"));
	
	return DebugInfo;
}

FGameplayTag UGeCharacterMovementComponent::GetMovementModeTag(EMovementMode InMovementMode, uint8 InCustomMode) const
{
	// Default implementation: convert movement mode to GameplayTag
	// Override this function to provide custom mapping
	
	FString TagName;
	
	switch (InMovementMode)
	{
	case MOVE_None:
		TagName = TEXT("MovementMode.None");
		break;
	case MOVE_Walking:
		TagName = TEXT("MovementMode.Walking");
		break;
	case MOVE_NavWalking:
		TagName = TEXT("MovementMode.NavWalking");
		break;
	case MOVE_Falling:
		TagName = TEXT("MovementMode.Falling");
		break;
	case MOVE_Swimming:
		TagName = TEXT("MovementMode.Swimming");
		break;
	case MOVE_Flying:
		TagName = TEXT("MovementMode.Flying");
		break;
	case MOVE_Custom:
		// For custom modes, you may want to use CustomMode value or override this function
		TagName = FString::Printf(TEXT("MovementMode.Custom.%d"), InCustomMode);
		break;
	case MOVE_MAX:
	default:
		TagName = TEXT("MovementMode.Unknown");
		break;
	}
	
	return FGameplayTag::RequestGameplayTag(FName(*TagName), false);
}

bool UGeCharacterMovementComponent::ShouldRestoreCapsuleOnMovementModeChange(EMovementMode NewMovementMode, uint8 NewCustomMode) const
{
	// Get the GameplayTag for the new movement mode
	const FGameplayTag MovementTag = GetMovementModeTag(NewMovementMode, NewCustomMode);
	
	// If tag is in skip restore container, use clear data only mode (1.2)
	if (MovementModeTagsSkipRestore.HasTag(MovementTag))
	{
		return false;
	}
	
	// Default behavior: restore capsule (1.1)
	return true;
}

#pragma endregion

#pragma region DebugDraw

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

// Returns the view rotation used to organize the camera-facing debug layout.
// Prefers the local viewer's final camera rotation so labels always face the screen.
static FRotator GetMovementDebugViewRotation(const ACharacter* Character)
{
	if (!Character)
	{
		return FRotator::ZeroRotator;
	}

	if (const APlayerController* PlayerController = Character->GetController<APlayerController>();
		PlayerController && PlayerController->IsLocalController() && PlayerController->PlayerCameraManager)
	{
		return PlayerController->PlayerCameraManager->GetCameraRotation();
	}

	// Remote/simulated characters: fall back to the first local player's camera
	if (const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(Character->GetWorld(), 0))
	{
		return CameraManager->GetCameraRotation();
	}

	return Character->GetBaseAimRotation();
}

void UGeCharacterMovementComponent::DisplayDebugForGame(float DeltaTime, bool bPrintToScreen, bool bPrintToLog)
{
	if (!HasValidData())
	{
		return;
	}

	// Compact one-line summary works everywhere, including dedicated servers (log parity)
	DrawMovementSummaryText(bPrintToScreen, bPrintToLog);

	// World-space drawing is meaningless without a local viewport
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// Merged drawer: real-time world drawing + Visual Logger recording for offline replay
	const FDebugDrawer WorldDrawer = FDebugDrawer::MakeDebugDrawer(World);
	const FDebugDrawer VLogDrawer = FDebugDrawer::MakeVisualLoggerDebugDrawer(
		this, LogGeCharacterMovement, ELogVerbosity::Verbose,
		/*bDrawToScene*/ false, /*bDrawToSceneWhileRecording*/ false);
	const FDebugDrawer DebugDrawer = FDebugDrawer::MakeMergedDebugDrawer({WorldDrawer, VLogDrawer});

	// All panels and labels are laid out in view space so they always face the local camera
	const FRotator ViewRotation = GetMovementDebugViewRotation(CharacterOwner);

	if (CVarAnimSkillMovement_DebugMovementShapes.GetValueOnGameThread())
	{
		DrawMovementRotationRing(DebugDrawer, ViewRotation, DeltaTime);
	}

	if (CVarAnimSkillMovement_DebugMovementPanel.GetValueOnGameThread())
	{
		DrawMovementStatePanel(DebugDrawer, ViewRotation);
	}

	if (CVarAnimSkillMovement_DebugMovementBars.GetValueOnGameThread())
	{
		DrawMovementBars(DebugDrawer, ViewRotation);
	}

	// History-based visualizations only track the locally controlled character:
	// server-side proxies and simulated proxies are skipped
	if (CharacterOwner->IsLocallyControlled())
	{
		CollectMovementDebugHistory();

		DrawMovementHistoryTrail(DebugDrawer);

		if (CVarAnimSkillMovement_DebugMovementGraph.GetValueOnGameThread())
		{
			DrawMovementSpeedGraph(DebugDrawer, ViewRotation);
		}
	}
}

void UGeCharacterMovementComponent::CollectMovementDebugHistory()
{
	// Distance-gated sampling: standing still must NOT push new samples, otherwise the
	// ring buffer evicts the existing path and the trail appears to vanish.
	// Note: TAutoConsoleVariable defaults do not refresh under Live Coding — set the CVar
	// explicitly (or restart the editor) after changing the registered default.
	const int32 TrailCount = CVarAnimSkillMovement_DebugMovementHistory.GetValueOnGameThread();
	if (TrailCount > 0)
	{
		constexpr float MinSampleDistance = 5.f;
		const FVector FeetLocation = GetActorFeetLocation();
		if (DebugPositionHistory.IsEmpty()
			|| FVector::DistSquared(DebugPositionHistory.Last(), FeetLocation) > FMath::Square(MinSampleDistance))
		{
			DebugPositionHistory.Add(FeetLocation);
			while (DebugPositionHistory.Num() > TrailCount)
			{
				DebugPositionHistory.RemoveAt(0);
			}
		}
	}
	else if (!DebugPositionHistory.IsEmpty())
	{
		DebugPositionHistory.Reset();
	}

	// Speed samples for the history graph
	if (CVarAnimSkillMovement_DebugMovementGraph.GetValueOnGameThread())
	{
		UDrawDebugLibrary::AddToFloatHistoryArray(DebugSpeedHistory, GetCurrentVelocity().Size2D(), 200);
	}
	else if (!DebugSpeedHistory.IsEmpty())
	{
		DebugSpeedHistory.Reset();
	}
}

void UGeCharacterMovementComponent::DrawMovementSummaryText(bool bPrintToScreen, bool bPrintToLog) const
{
	USkeletalMeshComponent* CharacterMesh = CharacterOwner->GetMesh();
	if (!CharacterMesh)
	{
		return;
	}

	const FString theLocalRole = UEnum::GetValueAsString(GetOwnerRole());
	const FString theObjectHash = FString::Printf(TEXT("%u"), GetTypeHash(FObjectKey{this}));

	const FTransform ActorTransform = GetActorTransform();
	const FVector LocalVelocity = UKismetMathLibrary::InverseTransformDirection(ActorTransform, GetCurrentVelocity());
	const FVector LocalAcceleration = UKismetMathLibrary::InverseTransformDirection(ActorTransform, GetCurrentAcceleration());
	const FString theDebugString = FString::Printf(
		TEXT("[%s] [Frame:%d] MaxSpeed=%.2f, Speed2D=%.2f, Speed=%.2f, Acc=%.2f, Loc(%s), Rot(%s), MeshRot:%s, MeshLoc:%s, LocalVel:%s, LocalAcc:%s")
		, *theLocalRole
		, GFrameNumber % 1000
		, GetMaxSpeed()
		, GetCurrentVelocity().Size2D()
		, GetCurrentVelocity().Size()
		, GetCurrentAcceleration().Size()
		, *ActorTransform.GetLocation().ToCompactString()
		, *ActorTransform.GetRotation().Rotator().ToCompactString()
		, *CharacterMesh->GetComponentRotation().ToCompactString()
		, *CharacterMesh->GetComponentLocation().ToCompactString()
		, *LocalVelocity.ToCompactString()
		, *LocalAcceleration.ToCompactString());

	UKismetSystemLibrary::PrintString(this, theDebugString, bPrintToScreen, bPrintToLog, FLinearColor::White, 0.f, FName(*theObjectHash));
}

void UGeCharacterMovementComponent::DrawMovementRotationRing(const FDebugDrawer& Drawer, const FRotator& ViewRotation, float DeltaTime) const
{
	const FVector CapsuleLocation = UpdatedComponent->GetComponentLocation();
	const FRotator CapsuleRotation = UpdatedComponent->GetComponentRotation();
	const FVector FeetLocation = GetActorFeetLocation();

	float CapsuleRadius = 0.f;
	float CapsuleHalfHeight = 0.f;
	CharacterOwner->GetSimpleCollisionCylinder(CapsuleRadius, CapsuleHalfHeight);

	const FRotator ControlRotation = CharacterOwner->GetControlRotation();
	const FRotator ControlYaw(0.f, ControlRotation.Yaw, 0.f);
	const FRotator ActorYaw(0.f, CapsuleRotation.Yaw, 0.f);

	// Desired rotation mirrors the target selection in PhysicsRotation()
	FRotator DesiredRotation = CapsuleRotation;
	if (bOrientRotationToMovement)
	{
		FRotator DeltaRotation = GetDeltaRotation(DeltaTime);
		DesiredRotation = ComputeOrientToMovementRotation(CapsuleRotation, DeltaTime, DeltaRotation);
	}
	else if (CharacterOwner->Controller && bUseControllerDesiredRotation)
	{
		DesiredRotation = CharacterOwner->Controller->GetDesiredRotation();
	}
	const FRotator DesiredYaw(0.f, DesiredRotation.Yaw, 0.f);

	// Classic capsule wireframe via primitive draws (round hemispheres, no lattice).
	// DrawDebugCapsule densifies into a mesh as Segments rise; this matches the old look.
	{
		FDrawDebugLineStyle CapsuleStyle;
		CapsuleStyle.Thickness = 0.5f;
		CapsuleStyle.Color = FLinearColor::Black;

		const float CapsuleCylinderHalfLength = FMath::Max(CapsuleHalfHeight - CapsuleRadius, 0.f);
		const FQuat CapsuleQuat = CapsuleRotation.Quaternion();
		const FVector CapsuleUp = CapsuleQuat.GetAxisZ();
		const FVector CapsuleForward = CapsuleQuat.GetAxisX();
		const FVector CapsuleRight = CapsuleQuat.GetAxisY();
		const FVector TopCenter = CapsuleLocation + CapsuleUp * CapsuleCylinderHalfLength;
		const FVector BottomCenter = CapsuleLocation - CapsuleUp * CapsuleCylinderHalfLength;
		constexpr int32 CapsuleCircleSegments = 16;

		// Cylinder end caps (horizontal circles in capsule XY)
		UDrawDebugLibrary::DrawDebugCircle(Drawer, TopCenter, CapsuleRotation, CapsuleStyle, true,
			CapsuleRadius, CapsuleCircleSegments);
		UDrawDebugLibrary::DrawDebugCircle(Drawer, BottomCenter, CapsuleRotation, CapsuleStyle, true,
			CapsuleRadius, CapsuleCircleSegments);

		// Four verticals at the cardinal angles
		for (int32 AxisIndex = 0; AxisIndex < 4; ++AxisIndex)
		{
			const float AngleRad = AxisIndex * (UE_HALF_PI);
			const FVector Radial = CapsuleQuat.RotateVector(
				FVector(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.f) * CapsuleRadius);
			UDrawDebugLibrary::DrawDebugLine(Drawer, TopCenter + Radial, BottomCenter + Radial,
				CapsuleStyle, true);
		}

		// Hemispheres: two orthogonal 180° arcs at each end (XZ / YZ of the capsule)
		const FRotator TopXZ = FRotationMatrix::MakeFromXY(CapsuleForward, CapsuleUp).Rotator();
		const FRotator TopYZ = FRotationMatrix::MakeFromXY(CapsuleRight, CapsuleUp).Rotator();
		const FRotator BottomXZ = FRotationMatrix::MakeFromXY(CapsuleForward, -CapsuleUp).Rotator();
		const FRotator BottomYZ = FRotationMatrix::MakeFromXY(CapsuleRight, -CapsuleUp).Rotator();
		UDrawDebugLibrary::DrawDebugArc(Drawer, TopCenter, TopXZ, 180.f, CapsuleStyle, true,
			CapsuleRadius, CapsuleCircleSegments);
		UDrawDebugLibrary::DrawDebugArc(Drawer, TopCenter, TopYZ, 180.f, CapsuleStyle, true,
			CapsuleRadius, CapsuleCircleSegments);
		UDrawDebugLibrary::DrawDebugArc(Drawer, BottomCenter, BottomXZ, 180.f, CapsuleStyle, true,
			CapsuleRadius, CapsuleCircleSegments);
		UDrawDebugLibrary::DrawDebugArc(Drawer, BottomCenter, BottomYZ, 180.f, CapsuleStyle, true,
			CapsuleRadius, CapsuleCircleSegments);

		// Match the old DrawDebugConeInDegrees look: Length=50, half-angle=30, Segments=8
		FDrawDebugLineStyle ConeStyle;
		ConeStyle.Thickness = 0.5f;
		ConeStyle.Color = FLinearColor::White;
		UDrawDebugLibrary::DrawDebugConeLookAt(Drawer, CharacterOwner->GetPawnViewLocation(),
			ControlRotation.Vector().GetSafeNormal(), ConeStyle, true, 50.f, 30.f, 8);
	}

	// Reference circle with world cardinal ticks at the capsule base
	{
		FDrawDebugLineStyle CircleStyle;
		CircleStyle.Thickness = 1.f;
		CircleStyle.Color = FLinearColor::Black;
		UDrawDebugLibrary::DrawDebugCircle(Drawer, FeetLocation, FRotator::ZeroRotator, CircleStyle, true, 40.f, 36);

		const TArray<float> CardinalAngles = { 0.f, 90.f, 180.f, 270.f };
		UDrawDebugLibrary::DrawDebugCircleTicks(Drawer, FeetLocation, FRotator::ZeroRotator, CardinalAngles,
			CircleStyle, true, 40.f, 4.f);
	}

	// Shared settings for the yaw ring arrows
	FDrawDebugArrowSettings CircleArrowSettings;
	CircleArrowSettings.bArrowheadOnEnd = true;
	CircleArrowSettings.bArrowLineEndsAtEndHead = true;
	CircleArrowSettings.ArrowHeadEndSize = 10.f;
	CircleArrowSettings.ArrowHeadEndType = EDrawDebugArrowHead::Triangle;

	// Yaw ring: one circle arrow per rotation, plus a delta arc back to the control rotation
	auto DrawYawArrow = [&](const FRotator& YawRotation, const FLinearColor& Color, float Radius, float Length, bool bDrawDeltaArc)
	{
		FDrawDebugLineStyle ArrowStyle;
		ArrowStyle.Thickness = 1.f;
		ArrowStyle.Color = Color;
		UDrawDebugLibrary::DrawDebugCircleArrow(Drawer, FeetLocation, YawRotation, 0.f, ArrowStyle, true,
			Radius, Length, CircleArrowSettings);

		if (bDrawDeltaArc)
		{
			const float DeltaYaw = UKismetMathLibrary::NormalizedDeltaRotator(ControlYaw, YawRotation).Yaw;
			UDrawDebugLibrary::DrawDebugArc(Drawer, FeetLocation, YawRotation, DeltaYaw, ArrowStyle, true, Radius, 32);
		}
	};

	const FLinearColor ControlColor = FLinearColor::Blue;
	const FLinearColor ActorColor = FLinearColor::Green;
	const FLinearColor DesiredColor = FLinearColor::Yellow;
	const FLinearColor VelocityColor(0.f, 1.f, 1.f);
	const FLinearColor AccelColor(1.f, 0.5f, 0.f);
	const FLinearColor InputColor = FLinearColor::Gray;

	DrawYawArrow(ControlYaw, ControlColor, 55.f, 60.f, false);
	DrawYawArrow(ActorYaw, ActorColor, 70.f, 25.f, true);
	DrawYawArrow(DesiredYaw, DesiredColor, 85.f, 25.f, true);

	// Straight arrows: velocity / acceleration / input, length scaled by the normalized magnitude
	const float ArrowScaleLength = CapsuleRadius * 3.f;
	FDrawDebugArrowSettings StraightArrowSettings;
	StraightArrowSettings.ArrowHeadEndSize = 8.f;
	StraightArrowSettings.ArrowHeadEndType = EDrawDebugArrowHead::Simple;

	auto DrawScaledArrow = [&](const FVector& Direction, float Magnitude, float MaxMagnitude, const FLinearColor& Color, float ZBias)
	{
		if (Direction.IsNearlyZero())
		{
			return;
		}
		const float Ratio = MaxMagnitude > 0.f ? FMath::Clamp(Magnitude / MaxMagnitude, 0.f, 1.5f) : 1.f;
		FDrawDebugLineStyle ArrowStyle;
		ArrowStyle.Thickness = 2.f;
		ArrowStyle.Color = Color;
		const FVector Start = FeetLocation + FVector(0.f, 0.f, ZBias);
		UDrawDebugLibrary::DrawDebugArrow(Drawer, Start, Start + Direction.GetSafeNormal() * ArrowScaleLength * Ratio,
			ArrowStyle, true, StraightArrowSettings);
	};

	const FVector CurrentVelocity = GetCurrentVelocity();
	const FVector CurrentAcceleration = GetCurrentAcceleration();
	const FVector InputVector = GetLastInputVector();

	DrawScaledArrow(CurrentVelocity, CurrentVelocity.Size(), GetMaxSpeed(), VelocityColor, 2.f);
	DrawScaledArrow(CurrentAcceleration, CurrentAcceleration.Size(), GetMaxAcceleration(), AccelColor, 10.f);
	DrawScaledArrow(InputVector, 1.f, 1.f, InputColor, 18.f);

	// Floor impact normal at the contact point
	if (CurrentFloor.bBlockingHit)
	{
		FDrawDebugLineStyle FloorStyle;
		FloorStyle.Thickness = 1.5f;
		FloorStyle.Color = FLinearColor(1.f, 0.f, 1.f);
		UDrawDebugLibrary::DrawDebugDirection(Drawer, CurrentFloor.HitResult.ImpactPoint,
			CurrentFloor.HitResult.ImpactNormal, FloorStyle, true, 60.f);
	}

	// Color-matched labels stacked in a camera-facing column hugging the ground ring,
	// so the rotation readouts stay visually attached to the arrows at the feet
	FDrawDebugStringSettings LabelSettings;
	LabelSettings.Height = 7.f;
	LabelSettings.bMonospaced = true;

	const FVector LabelBaseLocation = FeetLocation + ViewRotation.RotateVector(FVector(0.f, 110.f, 50.f));
	int32 LabelRow = 0;
	auto DrawLabelRow = [&](const FString& Text, const FLinearColor& Color)
	{
		FDrawDebugLineStyle TextStyle;
		TextStyle.Thickness = 0.5f;
		TextStyle.Color = Color;
		const FVector RowLocation = LabelBaseLocation - ViewRotation.RotateVector(FVector(0.f, 0.f, 9.f * LabelRow));
		UDrawDebugLibrary::DrawDebugString(Drawer, Text, RowLocation, ViewRotation, TextStyle, false, LabelSettings);
		++LabelRow;
	};

	const float VelocityYaw = CurrentVelocity.IsNearlyZero() ? 0.f : FRotator::NormalizeAxis(CurrentVelocity.Rotation().Yaw);
	const float AccelYaw = CurrentAcceleration.IsNearlyZero() ? 0.f : FRotator::NormalizeAxis(CurrentAcceleration.Rotation().Yaw);
	const float InputYaw = InputVector.IsNearlyZero() ? 0.f : FRotator::NormalizeAxis(InputVector.Rotation().Yaw);

	DrawLabelRow(FString::Printf(TEXT("Control Rot (%6.1f)"), FRotator::NormalizeAxis(ControlYaw.Yaw)), ControlColor);
	DrawLabelRow(FString::Printf(TEXT("Actor Rot   (%6.1f) d=%.1f"), FRotator::NormalizeAxis(ActorYaw.Yaw),
		UKismetMathLibrary::NormalizedDeltaRotator(ControlYaw, ActorYaw).Yaw), ActorColor);
	DrawLabelRow(FString::Printf(TEXT("Desired Rot (%6.1f) d=%.1f"), FRotator::NormalizeAxis(DesiredYaw.Yaw),
		UKismetMathLibrary::NormalizedDeltaRotator(ControlYaw, DesiredYaw).Yaw), DesiredColor);
	DrawLabelRow(FString::Printf(TEXT("Velocity    (%6.1f) %.0f cm/s"), VelocityYaw, CurrentVelocity.Size2D()), VelocityColor);
	DrawLabelRow(FString::Printf(TEXT("Accel       (%6.1f) %.0f"), AccelYaw, CurrentAcceleration.Size()), AccelColor);
	DrawLabelRow(FString::Printf(TEXT("Input       (%6.1f) %.2f"), InputYaw, InputVector.Size()), InputColor);
}

void UGeCharacterMovementComponent::DrawMovementStatePanel(const FDebugDrawer& Drawer, const FRotator& ViewRotation) const
{
	FDrawDebugStringSettings StringSettings;
	StringSettings.Height = 7.f;
	StringSettings.bMonospaced = true;

	FDrawDebugLineStyle LabelStyle;
	LabelStyle.Thickness = 0.5f;
	LabelStyle.Color = FLinearColor::Black;

	// Two-part rows: black label measured with string dimensions, value appended with a state color
	const FVector PanelBaseLocation = UpdatedComponent->GetComponentLocation() + ViewRotation.RotateVector(FVector(0.f, 50.f, 110.f));
	int32 RowIndex = 0;
	auto DrawKeyValueRow = [&](const FString& Label, const FString& Value, const FLinearColor& ValueColor)
	{
		const FVector RowLocation = PanelBaseLocation - ViewRotation.RotateVector(FVector(0.f, 0.f, 9.f * RowIndex));
		UDrawDebugLibrary::DrawDebugString(Drawer, Label, RowLocation, ViewRotation, LabelStyle, false, StringSettings);

		const FVector LabelDimensions = UDrawDebugLibrary::DrawDebugStringDimensions(Label, StringSettings);
		const FVector ValueLocation = RowLocation + ViewRotation.RotateVector(FVector(0.f, LabelDimensions.Y + 1.f, 0.f));
		FDrawDebugLineStyle ValueStyle = LabelStyle;
		ValueStyle.Color = ValueColor;
		UDrawDebugLibrary::DrawDebugString(Drawer, Value, ValueLocation, ViewRotation, ValueStyle, false, StringSettings);
		++RowIndex;
	};

	// Role
	DrawKeyValueRow(TEXT("Role    : "), UEnum::GetValueAsString(GetOwnerRole()), FLinearColor::White);

	// Movement mode (custom mode index appended when relevant)
	FString ModeValue = GetMovementName();
	if (MovementMode == MOVE_Custom)
	{
		ModeValue += FString::Printf(TEXT(" (%d)"), CustomMovementMode);
	}
	FLinearColor ModeColor = FLinearColor::Green;
	switch (MovementMode)
	{
	case MOVE_None:     ModeColor = FLinearColor::Red;            break;
	case MOVE_Falling:  ModeColor = FLinearColor::Yellow;         break;
	case MOVE_Flying:
	case MOVE_Swimming: ModeColor = FLinearColor(0.f, 1.f, 1.f);  break;
	case MOVE_Custom:   ModeColor = FLinearColor(1.f, 0.f, 1.f);  break;
	default: break;
	}
	DrawKeyValueRow(TEXT("Mode    : "), ModeValue, ModeColor);

	// Speed / acceleration
	const FVector CurrentVelocity = GetCurrentVelocity();
	const FVector CurrentAcceleration = GetCurrentAcceleration();
	DrawKeyValueRow(TEXT("Speed   : "),
		FString::Printf(TEXT("%.0f / %.0f (Z %+.0f)"), CurrentVelocity.Size2D(), GetMaxSpeed(), CurrentVelocity.Z),
		CurrentVelocity.Size2D() > 1.f ? FLinearColor::Green : FLinearColor::Gray);
	DrawKeyValueRow(TEXT("Accel   : "),
		FString::Printf(TEXT("%.0f / %.0f"), CurrentAcceleration.Size(), GetMaxAcceleration()),
		!CurrentAcceleration.IsNearlyZero() ? FLinearColor::Green : FLinearColor::Gray);

	// Rotation mode
	FString RotationModeValue = TEXT("Manual");
	if (bOrientRotationToMovement)
	{
		RotationModeValue = TEXT("OrientToMovement");
	}
	else if (bUseControllerDesiredRotation)
	{
		RotationModeValue = TEXT("ControllerDesired");
	}
	if (CharacterOwner->bUseControllerRotationYaw)
	{
		RotationModeValue += TEXT(" +CtrlYaw");
	}
	DrawKeyValueRow(TEXT("RotMode : "), RotationModeValue, FLinearColor::White);

	// Floor: walkable state, distance and slope angle
	FString FloorValue = TEXT("None");
	FLinearColor FloorColor = FLinearColor::Red;
	if (CurrentFloor.bBlockingHit)
	{
		const float SlopeDegrees = FMath::RadiansToDegrees(
			FMath::Acos(FMath::Clamp(CurrentFloor.HitResult.ImpactNormal.Z, -1.f, 1.f)));
		FloorValue = FString::Printf(TEXT("%s Dist=%.1f Slope=%.1fdeg (%s)"),
			CurrentFloor.bWalkableFloor ? TEXT("Walkable") : TEXT("Unwalkable"),
			CurrentFloor.GetDistanceToFloor(),
			SlopeDegrees,
			*GetNameSafe(CurrentFloor.HitResult.GetActor()));
		FloorColor = CurrentFloor.bWalkableFloor ? FLinearColor::Green : FLinearColor::Yellow;
	}
	DrawKeyValueRow(TEXT("Floor   : "), FloorValue, FloorColor);

	// Movement base
	const UObject* MovementBaseObject = CharacterOwner->GetMovementBaseObject();
	FString BaseValue = TEXT("None");
	if (const UActorComponent* BaseComponent = Cast<UActorComponent>(MovementBaseObject))
	{
		BaseValue = GetNameSafe(BaseComponent->GetOwner());
	}
	else if (MovementBaseObject)
	{
		BaseValue = GetNameSafe(MovementBaseObject);
	}
	DrawKeyValueRow(TEXT("Base    : "), BaseValue, MovementBaseObject ? FLinearColor::Green : FLinearColor::Gray);

	// Dynamic capsule stage
	const FString CapsuleValue = FString::Printf(TEXT("%s HalfHeight=%.0f%s"),
		*UEnum::GetDisplayValueAsText(CurrentCapsuleStage).ToString(),
		CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),
		bIsDynamicCapsuleActive ? TEXT(" [Active]") : TEXT(""));
	DrawKeyValueRow(TEXT("Capsule : "), CapsuleValue,
		CurrentCapsuleStage == EJumpCapsuleStage::FullSize ? FLinearColor::Green : FLinearColor::Yellow);

	// Jump timing (only meaningful while a tracked jump is in the air)
	if (IsFalling() && ExpectedJumpApexTime > 0.f)
	{
		DrawKeyValueRow(TEXT("Jump    : "),
			FString::Printf(TEXT("T=%.2f / Apex=%.2f"), AccumulatedJumpTime, ExpectedJumpApexTime),
			FLinearColor(0.f, 1.f, 1.f));
	}
}

void UGeCharacterMovementComponent::DrawMovementBars(const FDebugDrawer& Drawer, const FRotator& ViewRotation) const
{
	// Kept below the speed graph (graph bottom sits at +65 in view space)
	const FVector BarBaseLocation = UpdatedComponent->GetComponentLocation() + ViewRotation.RotateVector(FVector(0.f, -110.f, 25.f));
	const FVector BarDirection = ViewRotation.RotateVector(FVector(0.f, 1.f, 0.f));
	constexpr float MaxBarLength = 80.f;
	constexpr float BarSpacing = 16.f;

	FDrawDebugStringSettings BarTextSettings;
	BarTextSettings.Height = 6.f;
	BarTextSettings.bMonospaced = true;

	int32 BarIndex = 0;
	auto DrawProgressBar = [&](const FString& Label, float Ratio, const FLinearColor& Color)
	{
		const FVector BarStart = BarBaseLocation - ViewRotation.RotateVector(FVector(0.f, 0.f, BarSpacing * BarIndex));
		const float ClampedRatio = FMath::Clamp(Ratio, 0.f, 1.f);

		// Background track
		FDrawDebugLineStyle TrackStyle;
		TrackStyle.Thickness = 3.f;
		TrackStyle.Color = FLinearColor(0.15f, 0.15f, 0.15f);
		UDrawDebugLibrary::DrawDebugLine(Drawer, BarStart, BarStart + BarDirection * MaxBarLength, TrackStyle, false);

		// Filled portion, grayed out when nearly empty
		FDrawDebugLineStyle FillStyle = TrackStyle;
		FillStyle.Color = FMath::IsNearlyZero(ClampedRatio, 0.01f) ? FLinearColor(0.3f, 0.3f, 0.3f) : Color;
		UDrawDebugLibrary::DrawDebugLine(Drawer, BarStart,
			BarStart + BarDirection * FMath::Max(ClampedRatio * MaxBarLength, 0.5f), FillStyle, false);

		// Label above the bar
		FDrawDebugLineStyle TextStyle;
		TextStyle.Thickness = 0.5f;
		TextStyle.Color = FillStyle.Color;
		const FVector TextLocation = BarStart + ViewRotation.RotateVector(FVector(0.f, 0.f, 8.f));
		UDrawDebugLibrary::DrawDebugString(Drawer, FString::Printf(TEXT("%s %.2f"), *Label, Ratio),
			TextLocation, ViewRotation, TextStyle, false, BarTextSettings);
		++BarIndex;
	};

	const float CurrentMaxSpeed = GetMaxSpeed();
	const float CurrentMaxAcceleration = GetMaxAcceleration();
	DrawProgressBar(TEXT("Speed"), CurrentMaxSpeed > 0.f ? GetCurrentVelocity().Size2D() / CurrentMaxSpeed : 0.f,
		FLinearColor::Green);
	DrawProgressBar(TEXT("Accel"), CurrentMaxAcceleration > 0.f ? GetCurrentAcceleration().Size() / CurrentMaxAcceleration : 0.f,
		FLinearColor(1.f, 0.5f, 0.f));

	if (IsFalling() && ExpectedJumpApexTime > 0.f)
	{
		DrawProgressBar(TEXT("JumpApex"), AccumulatedJumpTime / ExpectedJumpApexTime, FLinearColor(0.f, 1.f, 1.f));
	}
}

void UGeCharacterMovementComponent::DrawMovementHistoryTrail(const FDebugDrawer& Drawer) const
{
	if (DebugPositionHistory.Num() < 2)
	{
		return;
	}

	FDrawDebugLineStyle TrailStyle;
	TrailStyle.Thickness = 1.f;
	TrailStyle.Color = FLinearColor(0.f, 1.f, 1.f);

	FDrawDebugArrowSettings TrailArrowSettings;
	TrailArrowSettings.ArrowHeadEndSize = 5.f;
	TrailArrowSettings.ArrowHeadEndType = EDrawDebugArrowHead::Simple;

	for (int32 Index = 1; Index < DebugPositionHistory.Num(); ++Index)
	{
		UDrawDebugLibrary::DrawDebugArrow(Drawer, DebugPositionHistory[Index - 1], DebugPositionHistory[Index],
			TrailStyle, true, TrailArrowSettings);
	}
}

void UGeCharacterMovementComponent::DrawMovementSpeedGraph(const FDebugDrawer& Drawer, const FRotator& ViewRotation) const
{
	if (DebugSpeedHistory.Num() < 2)
	{
		return;
	}

	TArray<float> XValues;
	UDrawDebugLibrary::MakeLinearlySpacedFloatArray(XValues, 0.f, 1.f, DebugSpeedHistory.Num());

	const float MaxYValue = FMath::Max(GetMaxSpeed() * 1.2f, 100.f);
	const FVector GraphLocation = UpdatedComponent->GetComponentLocation() + ViewRotation.RotateVector(FVector(0.f, -110.f, 65.f));

	FDrawDebugLineStyle TextStyle;
	TextStyle.Thickness = 0.5f;
	TextStyle.Color = FLinearColor::Black;

	FDrawDebugLineStyle AxesStyle;
	AxesStyle.Thickness = 0.5f;
	AxesStyle.Color = FLinearColor::Black;

	FDrawDebugLineStyle PlotStyle;
	PlotStyle.Thickness = 1.f;
	PlotStyle.Color = FLinearColor::Green;

	FDrawDebugGraphAxesSettings AxesSettings;
	AxesSettings.Title = FString::Printf(TEXT("Speed2D %.0f / %.0f"), GetCurrentVelocity().Size2D(), GetMaxSpeed());
	AxesSettings.TitleSettings.Height = 6.f;
	AxesSettings.TitleSettings.bMonospaced = true;
	AxesSettings.AxisLabelSettings.Height = 4.5f;
	AxesSettings.bDrawAxesBox = true;

	UDrawDebugLibrary::DrawDebugGraph(Drawer, GraphLocation, ViewRotation, XValues, DebugSpeedHistory,
		0.f, 1.f, 0.f, MaxYValue, 60.f, 40.f, TextStyle, AxesStyle, PlotStyle, false, AxesSettings);
}

#else

void UGeCharacterMovementComponent::DisplayDebugForGame(float, bool, bool)
{
}

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#pragma endregion

void UGeCharacterMovementComponent::MoveAlongFloor(const FVector& InVelocity, float DeltaSeconds, FStepDownResult* OutStepDownResult)
{
	if (!CurrentFloor.IsWalkableFloor())
	{
		return;
	}

	// Move along the current floor
	const FVector Delta = ProjectToGravityFloor(InVelocity) * DeltaSeconds;
	FHitResult Hit(1.f);
	FVector RampVector = ComputeGroundMovementDelta(Delta, CurrentFloor.HitResult, CurrentFloor.bLineTrace);
	SafeMoveUpdatedComponent(RampVector, UpdatedComponent->GetComponentQuat(), true, Hit);
	float LastMoveTimeSlice = DeltaSeconds;
	
	if (Hit.bStartPenetrating)
	{
		// Allow this hit to be used as an impact we can deflect off, otherwise we do nothing the rest of the update and appear to hitch.
		HandleImpact(Hit);
		SlideAlongSurface(Delta, 1.f, Hit.Normal, Hit, true);

		if (Hit.bStartPenetrating)
		{
			OnCharacterStuckInGeometry(&Hit);
		}
	}
	else if (Hit.IsValidBlockingHit())
	{
		// We impacted something (most likely another ramp, but possibly a barrier).
		float PercentTimeApplied = Hit.Time;
		if ((Hit.Time > 0.f) && (GetGravitySpaceZ(Hit.Normal) > UE_KINDA_SMALL_NUMBER) && IsWalkable(Hit))
		{
			// Another walkable ramp.
			const float InitialPercentRemaining = 1.f - PercentTimeApplied;
			RampVector = ComputeGroundMovementDelta(Delta * InitialPercentRemaining, Hit, false);
			LastMoveTimeSlice = InitialPercentRemaining * LastMoveTimeSlice;
			SafeMoveUpdatedComponent(RampVector, UpdatedComponent->GetComponentQuat(), true, Hit);

			const float SecondHitPercent = Hit.Time * InitialPercentRemaining;
			PercentTimeApplied = FMath::Clamp(PercentTimeApplied + SecondHitPercent, 0.f, 1.f);
		}

		if (Hit.IsValidBlockingHit())
		{
			if (CanStepUp(Hit) || (CharacterOwner->GetMovementBase() != nullptr && Hit.HitObjectHandle == CharacterOwner->GetMovementBase()->GetOwner()))
			{
				// hit a barrier, try to step up
				const FVector PreStepUpLocation = UpdatedComponent->GetComponentLocation();
				if (!StepUp(GetGravityDirection(), Delta * (1.f - PercentTimeApplied), Hit, OutStepDownResult))
				{
					UE_LOG_ENHANCED(LogGeCharacterMovement, Verbose, this, TEXT("- StepUp (ImpactNormal %s, Normal %s"), *Hit.ImpactNormal.ToString(), *Hit.Normal.ToString());
					HandleImpact(Hit, LastMoveTimeSlice, RampVector);
					SlideAlongSurface(Delta, 1.f - PercentTimeApplied, Hit.Normal, Hit, true);
				}
				else
				{
					UE_LOG_ENHANCED(LogGeCharacterMovement, Verbose, this, TEXT("+ StepUp (ImpactNormal %s, Normal %s"), *Hit.ImpactNormal.ToString(), *Hit.Normal.ToString());
					if (!bMaintainHorizontalGroundVelocity)
					{
						// Don't recalculate velocity based on this height adjustment, if considering vertical adjustments. Only consider horizontal movement.
						bJustTeleported = true;
						const float StepUpTimeSlice = (1.f - PercentTimeApplied) * DeltaSeconds;
						if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && StepUpTimeSlice >= UE_KINDA_SMALL_NUMBER)
						{
							Velocity = (UpdatedComponent->GetComponentLocation() - PreStepUpLocation) / StepUpTimeSlice;
							Velocity = ProjectToGravityFloor(Velocity);
						}
					}
				}
			}
			else if (Hit.Component.IsValid() && !Hit.Component.Get()->CanCharacterStepUp(CharacterOwner))
			{
				HandleImpact(Hit, LastMoveTimeSlice, RampVector);
				SlideAlongSurface(Delta, 1.f - PercentTimeApplied, Hit.Normal, Hit, true);
			}
		}
	}
}

float UGeCharacterMovementComponent::SlideAlongSurface(const FVector& Delta, float Time, const FVector& InNormal, FHitResult& Hit, bool bHandleImpact)
{
	if (!Hit.bBlockingHit)
	{
		return 0.f;
	}

	FVector Normal(InNormal);
	const FVector::FReal NormalZ = GetGravitySpaceZ(Normal);
	const int32 SlideFixMode = GeCharacterMovementCVars::SlideNormalZFix;

	if (IsMovingOnGround())
	{
		if (NormalZ > 0.f)
		{
			// Normal pointing up: flatten if the surface is not walkable to prevent pushing the character up steep slopes
			if (!IsWalkable(Hit))
			{
				Normal = ProjectToGravityFloor(Normal).GetSafeNormal();
			}
		}
		else if (NormalZ < -UE_KINDA_SMALL_NUMBER)
		{
			// Don't push down into the floor when the impact is on the upper portion of the capsule.
			if (!GeCharacterMovementCVars::SlideMode_SkipsFixup(SlideFixMode) && CurrentFloor.FloorDist < MIN_FLOOR_DIST && CurrentFloor.bBlockingHit)
			{
				const FVector FloorNormal = CurrentFloor.HitResult.Normal;
				const bool bFloorOpposedToMovement = (Delta | FloorNormal) < 0.f && (GetGravitySpaceZ(FloorNormal) < 1.f - UE_DELTA);
				if (bFloorOpposedToMovement)
				{
					// Movement opposes the floor normal: replace the hit normal with the floor normal.
					Normal = FloorNormal;
				}
				
				const bool bProjectToFloor = (SlideFixMode == 0) || (SlideFixMode == 1 && !bFloorOpposedToMovement);
				if (bProjectToFloor)
				{
					Normal = ProjectToGravityFloor(Normal).GetSafeNormal();
				}
			}
		}
	}

	float PercentTimeApplied = 0.f;
	const FVector OldHitNormal = Normal;
	const FString OldWallActor = GetNameSafe(Hit.GetActor());

	FVector SlideDelta = ComputeSlideVector(Delta, Time, Normal, Hit);
	if ((SlideDelta | Delta) > 0.f)
	{
		const FQuat Rotation = UpdatedComponent->GetComponentQuat();
		SafeMoveUpdatedComponent(SlideDelta, Rotation, true, Hit);

		const float FirstHitPercent = Hit.Time;
		PercentTimeApplied = FirstHitPercent;
		if (Hit.IsValidBlockingHit())
		{
			// Notify first impact
			if (bHandleImpact)
			{
				HandleImpact(Hit, FirstHitPercent * Time, SlideDelta);
			}

			// Compute new slide normal when hitting multiple surfaces.
			TwoWallAdjust(SlideDelta, Hit, OldHitNormal);

			const bool bNearlyZero = SlideDelta.IsNearlyZero(1e-3f);
			const bool bForward   = (SlideDelta | Delta) > 0.f;

			// Debug information
			const FVector NewHitNormal = Hit.Normal;
			const FString NewWallActor = GetNameSafe(Hit.GetActor());
			const float CosAngle = FMath::Clamp(OldHitNormal | NewHitNormal, -1.f, 1.f);
			const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(CosAngle));
			UE_LOG_ENHANCED(LogGeCharacterMovement, Verbose, this, TEXT("%hs Mode=%d, Angle=%.2f°, OldHitNormal=%s, NewHitNormal=%s, SlideDelta=%s, bNearlyZero=%d, bForward=%d"),
					__FUNCTION__, SlideFixMode, AngleDeg, *OldHitNormal.ToCompactString(), *NewHitNormal.ToCompactString(), *SlideDelta.ToCompactString(), bNearlyZero, bForward);
			
			if (GeCharacterMovementCVars::SlideMode_UsesTwoWall(SlideFixMode))
			{
				const bool bConcaveCorner = OldHitNormal.Z != 0.f && NewHitNormal.Z != 1.f && (OldHitNormal | NewHitNormal) < -KINDA_SMALL_NUMBER;
				UE_LOG_ENHANCED(LogGeCharacterMovement, Verbose, this, TEXT("%hs [Mode3] ConcaveCornerCheck: OldNormal=%s, NewNormal=%s, Dot=%.3f, bConcave=%d"),
					__FUNCTION__, *OldHitNormal.ToCompactString(), *NewHitNormal.ToCompactString(), (OldHitNormal | NewHitNormal), bConcaveCorner);
				if (bConcaveCorner)
				{
					// Step 1: Blend normals and cross-product into an initial slide direction.
					// The cross-product gives the shared edge direction; adding both normals
					// biases the result toward the angle bisector.
					FVector NewDir = (NewHitNormal ^ OldHitNormal);
					NewDir = NewHitNormal + OldHitNormal + NewDir;
					UE_LOG_ENHANCED(LogGeCharacterMovement, Verbose, this, TEXT("%hs [Mode3] Step1 BlendDir=%s"), __FUNCTION__, *NewDir.ToCompactString());

					// Step 2: Prevent the slide direction from opposing the original movement.
					if ((NewDir | Delta) <= 0.f)
					{
						// Expected direction: Delta projected onto the floor plane.
						FVector ExpectDir = FVector::VectorPlaneProject(Delta, OldHitNormal);
						// Pick whichever ±89° rotation around the wall normal aligns better
						// with the expected direction.
						FVector LeftDir  = Delta.RotateAngleAxis(-89.f, NewHitNormal);
						FVector RightDir = Delta.RotateAngleAxis( 89.f, NewHitNormal);
						const bool bPickLeft = (ExpectDir | LeftDir) > 0.f;
						NewDir = bPickLeft ? LeftDir : RightDir;
						UE_LOG_ENHANCED(LogGeCharacterMovement, Verbose, this, TEXT("%hs [Mode3] Step2 Opposed->Rotate: ExpectDir=%s, PickLeft=%d, NewDir=%s"),
							__FUNCTION__, *ExpectDir.ToCompactString(), bPickLeft, *NewDir.ToCompactString());
					}

					// Step 3: Project onto the wall plane to ensure the character slides
					// flush against the wall surface.
					const FVector PreProjectDir = NewDir;
					NewDir = FVector::VectorPlaneProject(NewDir, NewHitNormal).GetSafeNormal();
					UE_LOG_ENHANCED(LogGeCharacterMovement, Verbose, this, TEXT("%hs [Mode3] Step3 ProjectToWall: %s -> %s"),
						__FUNCTION__, *PreProjectDir.ToCompactString(), *NewDir.ToCompactString());

					// Step 4: Restore speed via horizontal-speed conservation.
					// New direction is nearly vertical (edge case): fall back to projection.
					float SlideSpeed = FMath::Abs(Delta | NewDir);
					
					// Using Delta's raw length would cause a speed drop when the projection
					// angle deviates. Instead: SlideSpeed = horizontal_speed / horizontal_ratio.
					const FVector DeltaHorizontal  = FVector(Delta.X,  Delta.Y,  0.f);
					const FVector NewDirHorizontal = FVector(NewDir.X, NewDir.Y, 0.f);
					const float   NewDirHorizSize  = NewDirHorizontal.Size();
					if (NewDirHorizSize > UE_KINDA_SMALL_NUMBER && GeCharacterMovementCVars::SlideMode_PreservesHorizSpeed(SlideFixMode))
					{
						// New direction has a horizontal component: preserve horizontal speed.
						SlideSpeed = DeltaHorizontal.Size() / NewDirHorizSize;
					}
					
					const FVector OldSlideDelta = SlideDelta;
					SlideDelta = SlideSpeed * (1.f - Hit.Time) * NewDir;
					UE_LOG_ENHANCED(LogGeCharacterMovement, Verbose, this, TEXT("%hs [Mode3] Step4 SlideDelta: %s -> %s (Speed=%.2f, HorizSize=%.4f, HitTime=%.3f)"),
						__FUNCTION__, *OldSlideDelta.ToCompactString(), *SlideDelta.ToCompactString(), SlideSpeed, NewDirHorizSize, Hit.Time);
				}
			}
			
			// const FVector Start = UpdatedComponent->GetComponentLocation();
			// DrawDebugDirectionalArrow(GetWorld(), Start, Start + NewHitNormal * 30.f, 4.0f, FColor::Red, false, 10.0f);
			// DrawDebugDirectionalArrow(GetWorld(), Start, Start + OldHitNormal * 30.f, 4.0f, FColor::Green, false, 10.0f);
			// DrawDebugDirectionalArrow(GetWorld(), Start, Start + Delta.GetSafeNormal() * 30.f, 4.0f, FColor::Blue, false, 10.0f);
			// DrawDebugDirectionalArrow(GetWorld(), Start, Start + SlideDelta.GetSafeNormal() * 30.f, 4.0f, FColor::Yellow, false, 10.0f);
			
			// Only proceed if the new direction is of significant length and not in reverse of original attempted move.
			if (!bNearlyZero && bForward)
			{
				// Perform second move
				SafeMoveUpdatedComponent(SlideDelta, Rotation, true, Hit);
				const float SecondHitPercent = Hit.Time * (1.f - FirstHitPercent);
				PercentTimeApplied += SecondHitPercent;

				// Notify second impact
				if (bHandleImpact && Hit.bBlockingHit)
				{
					HandleImpact(Hit, SecondHitPercent * Time, SlideDelta);
				}
			}
		}

		const float FinalPercent = FMath::Clamp(PercentTimeApplied, 0.f, 1.f);
		return FinalPercent;
	}
	
	return 0.f;
}

void UGeCharacterMovementComponent::TwoWallAdjust(FVector& WorldSpaceDelta, const FHitResult& Hit, const FVector& OldHitNormal) const
{
	// Call parent (UCharacterMovementComponent) to perform the base two-wall adjustment first
	Super::TwoWallAdjust(WorldSpaceDelta, Hit, OldHitNormal);
}
