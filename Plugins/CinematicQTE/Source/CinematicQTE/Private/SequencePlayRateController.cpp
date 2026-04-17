// Copyright Cinematic QTE System. All Rights Reserved.

#include "SequencePlayRateController.h"
#include "CinematicQTEModule.h"
#include "LevelSequencePlayer.h"
#include "Curves/CurveFloat.h"

FSequencePlayRateController::FSequencePlayRateController()
{
}

void FSequencePlayRateController::SetPlayer(ULevelSequencePlayer* InPlayer)
{
	PlayerRef = InPlayer;
	if (InPlayer)
	{
		CurrentRate = InPlayer->GetPlayRate();
		TargetRate = CurrentRate;
		StartRate = CurrentRate;
		bBlending = false;
	}
}

void FSequencePlayRateController::ClearPlayer()
{
	PlayerRef.Reset();
	bBlending = false;
}

void FSequencePlayRateController::RequestBlendTo(float InTargetRate, float BlendTime, UCurveFloat* Curve /*= nullptr*/)
{
	BlendCurve = Curve;

	if (BlendTime <= 0.f)
	{
		CurrentRate = InTargetRate;
		TargetRate = InTargetRate;
		StartRate = InTargetRate;
		bBlending = false;
		ApplyRate(CurrentRate);
		return;
	}

	// 新请求：从当前插值值重新开始
	StartRate = CurrentRate;
	TargetRate = InTargetRate;
	BlendDuration = BlendTime;
	BlendElapsed = 0.f;
	bBlending = true;

	UE_LOG(LogCinematicQTE, Verbose, TEXT("PlayRateController: blend %.3f -> %.3f in %.2fs"),
		StartRate, TargetRate, BlendDuration);
}

void FSequencePlayRateController::Tick(float DeltaTime)
{
	if (bPaused || !bBlending)
	{
		return;
	}

	BlendElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(BlendElapsed / FMath::Max(BlendDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);

	float EvalAlpha = Alpha;
	if (UCurveFloat* Curve = BlendCurve.Get())
	{
		EvalAlpha = Curve->GetFloatValue(Alpha);
	}

	CurrentRate = FMath::Lerp(StartRate, TargetRate, EvalAlpha);
	ApplyRate(CurrentRate);

	if (Alpha >= 1.f)
	{
		CurrentRate = TargetRate;
		ApplyRate(CurrentRate);
		bBlending = false;
	}
}

void FSequencePlayRateController::OnPause()
{
	bPaused = true;
}

void FSequencePlayRateController::OnResume()
{
	bPaused = false;
}

void FSequencePlayRateController::SnapTo(float Rate)
{
	CurrentRate = Rate;
	StartRate = Rate;
	TargetRate = Rate;
	bBlending = false;
	ApplyRate(Rate);
}

void FSequencePlayRateController::ApplyRate(float Rate)
{
	if (ULevelSequencePlayer* Player = PlayerRef.Get())
	{
		Player->SetPlayRate(Rate);
	}
	else
	{
		// 静默跳过，由调用方统一监控
		UE_LOG(LogCinematicQTE, VeryVerbose, TEXT("PlayRateController: player invalid, skip SetPlayRate(%.3f)"), Rate);
	}
}
