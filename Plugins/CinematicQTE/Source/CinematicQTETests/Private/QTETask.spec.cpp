// Copyright Cinematic QTE System. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "MashQTETask.h"
#include "MashQTEDataAsset.h"
#include "TapQTETask.h"
#include "TapQTEDataAsset.h"
#include "CinematicQTETypes.h"
#include "InputAction.h"
#include "InputActionValue.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CinematicQTETestHelpers
{
	static UInputAction* MakeTestInputAction()
	{
		UInputAction* IA = NewObject<UInputAction>(GetTransientPackage());
		IA->ValueType = EInputActionValueType::Boolean;
		return IA;
	}

	static UMashQTEDataAsset* MakeMashAsset(int32 Count = 5, float Duration = 2.f)
	{
		UMashQTEDataAsset* Asset = NewObject<UMashQTEDataAsset>(GetTransientPackage());
		Asset->Duration = Duration;
		Asset->RequiredPressCount = Count;
		Asset->ProgressPerPress = 0.f; // 自动计算
		Asset->ProgressDecayRate = 0.f;
		Asset->MinPressInterval = 0.f;
		Asset->InputAction = MakeTestInputAction();
		return Asset;
	}

	static UTapQTEDataAsset* MakeTapAsset(float Duration = 1.f, float WStart = 0.4f, float WEnd = 0.6f)
	{
		UTapQTEDataAsset* Asset = NewObject<UTapQTEDataAsset>(GetTransientPackage());
		Asset->Duration = Duration;
		Asset->bUsePerfectWindow = true;
		Asset->PerfectWindowStart = WStart;
		Asset->PerfectWindowEnd = WEnd;
		Asset->InputAction = MakeTestInputAction();
		return Asset;
	}
}

// ============================================================================
// Mash QTE：达标成功
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMashQTESuccessTest,
	"CinematicQTE.Mash.SuccessByReachingTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FMashQTESuccessTest::RunTest(const FString& Parameters)
{
	UMashQTEDataAsset* Asset = CinematicQTETestHelpers::MakeMashAsset(5, 2.f);
	UMashQTETask* Task = NewObject<UMashQTETask>();

	Task->StartQTE(nullptr, nullptr, Asset);
	TestEqual(TEXT("Initial state running"), (int32)Task->GetTaskState(), (int32)EQTETaskState::Running);

	// 5 次按键应达成成功
	FInputActionValue V(true);
	for (int32 i = 0; i < 5; ++i)
	{
		Task->HandleInput(V);
	}

	TestEqual(TEXT("Result is Success"), (int32)Task->GetLastResult(), (int32)EQTEResult::Success);
	TestEqual(TEXT("State finished"), (int32)Task->GetTaskState(), (int32)EQTETaskState::Finished);
	return true;
}

// ============================================================================
// Mash QTE：防连发
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMashQTEMinIntervalTest,
	"CinematicQTE.Mash.RejectPressWithinMinInterval",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FMashQTEMinIntervalTest::RunTest(const FString& Parameters)
{
	UMashQTEDataAsset* Asset = CinematicQTETestHelpers::MakeMashAsset(10, 5.f);
	Asset->MinPressInterval = 0.5f;

	UMashQTETask* Task = NewObject<UMashQTETask>();
	Task->StartQTE(nullptr, nullptr, Asset);

	// 连按 5 次不 Tick，MinInterval 生效 → 只有第一次有效
	FInputActionValue V(true);
	for (int32 i = 0; i < 5; ++i)
	{
		Task->HandleInput(V);
	}

	TestEqual(TEXT("PressCount should be 1 due to min interval"), Task->GetResultMeta().PressCount, 1);
	return true;
}

// ============================================================================
// Mash QTE：超时失败
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMashQTETimeoutTest,
	"CinematicQTE.Mash.TimeoutWhenProgressNotReached",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FMashQTETimeoutTest::RunTest(const FString& Parameters)
{
	UMashQTEDataAsset* Asset = CinematicQTETestHelpers::MakeMashAsset(10, 1.f);
	UMashQTETask* Task = NewObject<UMashQTETask>();

	Task->StartQTE(nullptr, nullptr, Asset);

	// 推进时间到超时
	Task->TickQTE(1.1f);
	TestEqual(TEXT("Result is Timeout"), (int32)Task->GetLastResult(), (int32)EQTEResult::Timeout);
	return true;
}

// ============================================================================
// Tap QTE：完美窗口内成功
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapQTEPerfectWindowSuccessTest,
	"CinematicQTE.Tap.SuccessInPerfectWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTapQTEPerfectWindowSuccessTest::RunTest(const FString& Parameters)
{
	UTapQTEDataAsset* Asset = CinematicQTETestHelpers::MakeTapAsset(1.f, 0.4f, 0.6f);
	UTapQTETask* Task = NewObject<UTapQTETask>();

	Task->StartQTE(nullptr, nullptr, Asset);
	Task->TickQTE(0.5f); // 达到 50% 位置（位于完美窗口内）

	FInputActionValue V(true);
	Task->HandleInput(V);

	TestEqual(TEXT("Tap Perfect Window Success"), (int32)Task->GetLastResult(), (int32)EQTEResult::Success);
	return true;
}

// ============================================================================
// Tap QTE：窗口外失败
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapQTEOutsideWindowFailTest,
	"CinematicQTE.Tap.FailOutsideWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTapQTEOutsideWindowFailTest::RunTest(const FString& Parameters)
{
	UTapQTEDataAsset* Asset = CinematicQTETestHelpers::MakeTapAsset(1.f, 0.4f, 0.6f);
	UTapQTETask* Task = NewObject<UTapQTETask>();

	Task->StartQTE(nullptr, nullptr, Asset);
	Task->TickQTE(0.2f); // 20% 位置（窗口外）

	FInputActionValue V(true);
	Task->HandleInput(V);

	TestEqual(TEXT("Tap Outside Window Fail"), (int32)Task->GetLastResult(), (int32)EQTEResult::Failure);
	return true;
}

// ============================================================================
// Tap QTE：超时
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapQTETimeoutTest,
	"CinematicQTE.Tap.TimeoutWhenNoInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTapQTETimeoutTest::RunTest(const FString& Parameters)
{
	UTapQTEDataAsset* Asset = CinematicQTETestHelpers::MakeTapAsset(1.f, 0.4f, 0.6f);
	UTapQTETask* Task = NewObject<UTapQTETask>();

	Task->StartQTE(nullptr, nullptr, Asset);
	Task->TickQTE(1.1f);

	TestEqual(TEXT("Tap Timeout"), (int32)Task->GetLastResult(), (int32)EQTEResult::Timeout);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
