// Copyright Cinematic QTE System. All Rights Reserved.

#include "CinematicQTESubsystem.h"
#include "CinematicQTEModule.h"
#include "QTEDataAsset.h"
#include "QTETaskBase.h"
#include "QTEWidgetBase.h"
#include "LevelSequencePlayer.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"

extern TAutoConsoleVariable<int32> CVarQTEDebugShow;
extern TAutoConsoleVariable<FString> CVarQTEDebugForceResult;

// ============================================================================
// USubsystem
// ============================================================================

bool UCinematicQTESubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// 仅在 Game / PIE 世界创建
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->IsGameWorld();
	}
	return false;
}

void UCinematicQTESubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogCinematicQTE, Log, TEXT("CinematicQTESubsystem initialized."));
}

void UCinematicQTESubsystem::Deinitialize()
{
	// 强制取消以清理资源
	if (CurrentTask)
	{
		CancelCurrentQTE(EQTEResult::Cancelled);
	}
	QueuedAssets.Reset();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WidgetRemovalTimer);
	}

	Super::Deinitialize();
	UE_LOG(LogCinematicQTE, Log, TEXT("CinematicQTESubsystem deinitialized."));
}

// ============================================================================
// FTickableGameObject
// ============================================================================

bool UCinematicQTESubsystem::IsTickable() const
{
	return CurrentTask != nullptr || PlayRateController.IsBlending();
}

TStatId UCinematicQTESubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCinematicQTESubsystem, STATGROUP_Tickables);
}

void UCinematicQTESubsystem::Tick(float DeltaTime)
{
	// 游戏暂停时同步暂停
	UWorld* World = GetWorld();
	const bool bPaused = (World && World->IsPaused());
	if (bPaused)
	{
		PlayRateController.OnPause();
		return;
	}
	PlayRateController.OnResume();

	PlayRateController.Tick(DeltaTime);

	if (CurrentTask)
	{
		CurrentTask->TickQTE(DeltaTime);
	}

	// 屏幕调试 HUD
#if !UE_BUILD_SHIPPING
	if (CVarQTEDebugShow.GetValueOnGameThread() != 0 && GEngine)
	{
		if (CurrentTask && CurrentTask->GetDataAsset())
		{
			const FString Info = FString::Printf(
				TEXT("[QTE] %s  Remain=%.2fs  Progress=%.2f  PlayRate=%.3f"),
				*CurrentTask->GetDataAsset()->GetName(),
				CurrentTask->GetRemainingTime(),
				CurrentTask->GetCurrentProgress(),
				PlayRateController.GetCurrentRate());
			GEngine->AddOnScreenDebugMessage((uint64)this, 0.f, FColor::Yellow, Info);
		}
	}
#endif
}

// ============================================================================
// Static accessor
// ============================================================================

UCinematicQTESubsystem* UCinematicQTESubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext) return nullptr;
	if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr)
	{
		return World->GetSubsystem<UCinematicQTESubsystem>();
	}
	return nullptr;
}

// ============================================================================
// Public API
// ============================================================================

bool UCinematicQTESubsystem::StartQTE(UQTEDataAsset* InDataAsset, ULevelSequencePlayer* InPlayer,
	EQTEConflictPolicy InConflictPolicy /*= EQTEConflictPolicy::Ignore*/)
{
	if (!IsValid(InDataAsset))
	{
		UE_LOG(LogCinematicQTE, Warning, TEXT("StartQTE: DataAsset is null."));
		return false;
	}

	FString Reason;
	if (!InDataAsset->IsValidConfig(Reason))
	{
		UE_LOG(LogCinematicQTE, Warning, TEXT("StartQTE rejected: %s"), *Reason);
		return false;
	}

	// 处理冲突
	if (IsQTEActive())
	{
		switch (InConflictPolicy)
		{
		case EQTEConflictPolicy::Ignore:
			UE_LOG(LogCinematicQTE, Log, TEXT("StartQTE ignored: another QTE is active."));
			return false;
		case EQTEConflictPolicy::Queue:
			QueuedAssets.Add(InDataAsset);
			UE_LOG(LogCinematicQTE, Log, TEXT("StartQTE queued (%d waiting)."), QueuedAssets.Num());
			return true;
		case EQTEConflictPolicy::Replace:
			CancelCurrentQTE(EQTEResult::Cancelled);
			break;
		}
	}

	// 跳过 Dedicated Server 的 UI / 输入
	UWorld* World = GetWorld();
	const bool bIsDedicatedServer = (World && World->GetNetMode() == NM_DedicatedServer);

	// 创建 Task
	TSubclassOf<UQTETaskBase> TaskClass = InDataAsset->TaskClass;
	if (!TaskClass)
	{
		UE_LOG(LogCinematicQTE, Warning, TEXT("StartQTE rejected: DataAsset[%s] has no TaskClass."),
			*InDataAsset->GetName());
		return false;
	}

	APlayerController* PC = ResolvePlayerController();
	CurrentTask = NewObject<UQTETaskBase>(this, TaskClass);
	CurrentTask->OnQTEFinished.AddDynamic(this, &UCinematicQTESubsystem::HandleTaskFinished);

	CurrentSequencePlayer = InPlayer;

	// 速率切换
	if (InPlayer)
	{
		PlayRateController.SetPlayer(InPlayer);
		PlayRateController.RequestBlendTo(InDataAsset->SlowMotionRate,
			InDataAsset->SlowDownBlendTime, InDataAsset->BlendCurve);
	}
	else
	{
		UE_LOG(LogCinematicQTE, Warning, TEXT("StartQTE: SequencePlayer is null; skip play rate control."));
	}

	// UI
	if (!bIsDedicatedServer && InDataAsset->WidgetClass)
	{
		CreateAndShowWidget(InDataAsset);
	}

	// 输入
	if (!bIsDedicatedServer && PC)
	{
		BindQTEInput(PC, InDataAsset);
	}

	// 启动任务
	CurrentTask->StartQTE(World, PC, InDataAsset);

	// 检查调试强制结果
	CheckForcedDebugResult();

	return true;
}

void UCinematicQTESubsystem::CancelCurrentQTE(EQTEResult Result /*= EQTEResult::Cancelled*/)
{
	if (CurrentTask)
	{
		CurrentTask->FinishQTE(Result);
		// HandleTaskFinished 会接着完成清理
	}
}

bool UCinematicQTESubsystem::IsQTEActive() const
{
	return CurrentTask != nullptr && CurrentTask->GetTaskState() == EQTETaskState::Running;
}

// ============================================================================
// Task finished handling
// ============================================================================

void UCinematicQTESubsystem::HandleTaskFinished(EQTEResult Result, UQTEDataAsset* InDataAsset, FQTEResultMeta Meta)
{
	UE_LOG(LogCinematicQTE, Log, TEXT("HandleTaskFinished: Result=%d Asset=%s"),
		(int32)Result, InDataAsset ? *InDataAsset->GetName() : TEXT("null"));

	// 恢复动画速率
	const float SpeedUpTime = InDataAsset ? InDataAsset->SpeedUpBlendTime : 0.3f;
	UCurveFloat* Curve = InDataAsset ? InDataAsset->BlendCurve : nullptr;
	PlayRateController.RequestBlendTo(1.0f, SpeedUpTime, Curve);

	// 通知 Widget 播放反馈动画
	if (CurrentWidget)
	{
		CurrentWidget->OnQTEFinished(Result);
	}

	// 延时移除 Widget
	const float FeedbackTime = InDataAsset ? InDataAsset->FeedbackDuration : 0.f;
	DestroyWidgetWithFeedback(FeedbackTime);

	// 解绑输入
	UnbindQTEInput();

	// 广播全局
	OnGlobalQTEFinished.Broadcast(Result, InDataAsset, Meta);

	// 清理 Task
	if (CurrentTask)
	{
		CurrentTask->OnQTEFinished.RemoveDynamic(this, &UCinematicQTESubsystem::HandleTaskFinished);
		CurrentTask = nullptr;
	}
	CurrentSequencePlayer.Reset();

	// 处理队列
	if (QueuedAssets.Num() > 0)
	{
		UQTEDataAsset* Next = QueuedAssets[0];
		QueuedAssets.RemoveAt(0);
		// 下一个 QTE 在一帧后启动，避免递归
		if (UWorld* World = GetWorld())
		{
			FTimerHandle Tmp;
			TWeakObjectPtr<UCinematicQTESubsystem> WeakThis(this);
			TWeakObjectPtr<UQTEDataAsset> WeakAsset(Next);
			World->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateLambda([WeakThis, WeakAsset]()
				{
					if (WeakThis.IsValid() && WeakAsset.IsValid())
					{
						WeakThis->StartQTE(WeakAsset.Get(), nullptr, EQTEConflictPolicy::Ignore);
					}
				}));
		}
	}
}

// ============================================================================
// Widget
// ============================================================================

void UCinematicQTESubsystem::CreateAndShowWidget(UQTEDataAsset* InDataAsset)
{
	APlayerController* PC = ResolvePlayerController();
	if (!PC || !InDataAsset || !InDataAsset->WidgetClass) return;

	CurrentWidget = CreateWidget<UQTEWidgetBase>(PC, InDataAsset->WidgetClass);
	if (CurrentWidget)
	{
		CurrentWidget->InitializeFromTask(CurrentTask, InDataAsset);
		CurrentWidget->AddToViewport(InDataAsset->WidgetZOrder);
	}
}

void UCinematicQTESubsystem::DestroyWidgetWithFeedback(float DelayTime)
{
	UWorld* World = GetWorld();
	if (!World || !CurrentWidget)
	{
		if (CurrentWidget)
		{
			CurrentWidget->RemoveFromParent();
			CurrentWidget = nullptr;
		}
		return;
	}

	if (DelayTime <= 0.f)
	{
		CurrentWidget->RemoveFromParent();
		CurrentWidget = nullptr;
		return;
	}

	PendingFeedbackDuration = DelayTime;
	TWeakObjectPtr<UCinematicQTESubsystem> WeakThis(this);
	World->GetTimerManager().SetTimer(WidgetRemovalTimer,
		FTimerDelegate::CreateLambda([WeakThis]()
		{
			if (WeakThis.IsValid() && WeakThis->CurrentWidget)
			{
				WeakThis->CurrentWidget->RemoveFromParent();
				WeakThis->CurrentWidget = nullptr;
			}
		}),
		DelayTime, false);
}

// ============================================================================
// Input
// ============================================================================

void UCinematicQTESubsystem::BindQTEInput(APlayerController* PC, UQTEDataAsset* InDataAsset)
{
	if (!PC || !InDataAsset || !InDataAsset->InputAction) return;

	ULocalPlayer* LP = PC->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* EIS = LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!EIS) return;

	// 构造动态 Mapping Context（高优先级）
	DynamicMappingContext = NewObject<UInputMappingContext>(this);
	// 注意：此处未添加 Key 映射，因为 InputAction 的按键映射通常已在项目的 IMC 中存在；
	// 我们只需确保该 Action 在 QTE 期间被路由到任务即可。
	EIS->AddMappingContext(DynamicMappingContext, /*Priority=*/1000);

	// 绑定到 EnhancedInputComponent
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PC->InputComponent))
	{
		const FEnhancedInputActionEventBinding& Binding = EIC->BindAction(
			InDataAsset->InputAction, ETriggerEvent::Started, CurrentTask, &UQTETaskBase::HandleInput);
		InputBindingHandles.Add(Binding.GetHandle());
		BoundInputComponent = EIC;
	}
}

void UCinematicQTESubsystem::UnbindQTEInput()
{
	// 移除 Mapping Context
	if (APlayerController* PC = ResolvePlayerController())
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* EIS = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (DynamicMappingContext)
				{
					EIS->RemoveMappingContext(DynamicMappingContext);
				}
			}
		}
	}
	DynamicMappingContext = nullptr;

	// 解绑 EnhancedInputComponent
	if (UEnhancedInputComponent* EIC = BoundInputComponent.Get())
	{
		for (uint32 Handle : InputBindingHandles)
		{
			EIC->RemoveBindingByHandle(Handle);
		}
	}
	InputBindingHandles.Reset();
	BoundInputComponent.Reset();
}

// ============================================================================
// Helpers
// ============================================================================

APlayerController* UCinematicQTESubsystem::ResolvePlayerController() const
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;
	return World->GetFirstPlayerController();
}

void UCinematicQTESubsystem::CheckForcedDebugResult()
{
#if !UE_BUILD_SHIPPING
	const FString Forced = CVarQTEDebugForceResult.GetValueOnGameThread();
	if (Forced.IsEmpty() || !CurrentTask) return;

	EQTEResult R = EQTEResult::None;
	if (Forced.Equals(TEXT("Success"), ESearchCase::IgnoreCase))       R = EQTEResult::Success;
	else if (Forced.Equals(TEXT("Failure"), ESearchCase::IgnoreCase))  R = EQTEResult::Failure;
	else if (Forced.Equals(TEXT("Timeout"), ESearchCase::IgnoreCase))  R = EQTEResult::Timeout;
	else if (Forced.Equals(TEXT("Cancelled"), ESearchCase::IgnoreCase)) R = EQTEResult::Cancelled;

	if (R != EQTEResult::None)
	{
		UE_LOG(LogCinematicQTE, Warning, TEXT("Debug ForceResult applied: %s"), *Forced);
		CurrentTask->SetForcedResult(R);
		// 清除 CVar 防止后续 QTE 仍被强制
		CVarQTEDebugForceResult->Set(TEXT(""), ECVF_SetByConsole);
	}
#endif
}
