// Copyright Cinematic QTE System. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "SequencePlayRateController.h"

#if WITH_DEV_AUTOMATION_TESTS

// 注意：无 LevelSequencePlayer 时，ApplyRate 仅做空操作；
// 这里测试 Controller 自身状态（CurrentRate / BlendElapsed / IsBlending）逻辑。

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayRateImmediateSnapTest,
	"CinematicQTE.PlayRate.ImmediateSnapWhenBlendTimeZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPlayRateImmediateSnapTest::RunTest(const FString& Parameters)
{
	FSequencePlayRateController Ctrl;
	Ctrl.SnapTo(1.f);
	Ctrl.RequestBlendTo(0.01f, 0.f);
	TestEqual(TEXT("Immediate snap rate"), Ctrl.GetCurrentRate(), 0.01f);
	TestFalse(TEXT("Not blending"), Ctrl.IsBlending());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayRateBlendProgressTest,
	"CinematicQTE.PlayRate.BlendProgressesOverTime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPlayRateBlendProgressTest::RunTest(const FString& Parameters)
{
	FSequencePlayRateController Ctrl;
	Ctrl.SnapTo(1.f);
	Ctrl.RequestBlendTo(0.0f, 1.f); // 1s 从 1.0 -> 0.0

	Ctrl.Tick(0.5f);
	TestTrue(TEXT("Blending"), Ctrl.IsBlending());
	TestTrue(TEXT("Mid rate between 0 and 1"),
		Ctrl.GetCurrentRate() > 0.4f && Ctrl.GetCurrentRate() < 0.6f);

	Ctrl.Tick(0.6f); // 超出结束
	TestEqual(TEXT("Finished rate"), Ctrl.GetCurrentRate(), 0.f);
	TestFalse(TEXT("Not blending after finish"), Ctrl.IsBlending());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayRateInterruptBlendTest,
	"CinematicQTE.PlayRate.InterruptedBlendStartsFromCurrent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FPlayRateInterruptBlendTest::RunTest(const FString& Parameters)
{
	FSequencePlayRateController Ctrl;
	Ctrl.SnapTo(1.f);
	Ctrl.RequestBlendTo(0.0f, 1.f);
	Ctrl.Tick(0.5f); // 当前约 0.5

	const float Mid = Ctrl.GetCurrentRate();
	Ctrl.RequestBlendTo(1.0f, 1.f); // 反向切换
	TestEqual(TEXT("Start from current rate"), Ctrl.GetCurrentRate(), Mid);

	Ctrl.Tick(1.0f);
	TestEqual(TEXT("Final rate back to 1.0"), Ctrl.GetCurrentRate(), 1.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
