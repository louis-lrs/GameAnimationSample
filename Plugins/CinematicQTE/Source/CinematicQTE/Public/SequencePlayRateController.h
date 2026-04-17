// Copyright Cinematic QTE System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class ULevelSequencePlayer;
class UCurveFloat;

/**
 * Level Sequence 播放速率平滑插值控制器（纯 C++，非 UObject）。
 * 支持目标速率变更时基于当前值重新计算，避免速率突变。
 */
class CINEMATICQTE_API FSequencePlayRateController
{
public:
	FSequencePlayRateController();

	/** 设置目标 Sequence Player */
	void SetPlayer(ULevelSequencePlayer* InPlayer);

	/** 清空 Player 引用 */
	void ClearPlayer();

	/**
	 * 请求速率平滑过渡到 TargetRate。
	 * @param TargetRate 目标速率
	 * @param BlendTime 过渡时长（秒）；<=0 表示立即设置
	 * @param Curve 可选插值曲线；为空则线性
	 */
	void RequestBlendTo(float TargetRate, float BlendTime, UCurveFloat* Curve = nullptr);

	/** 每帧 Tick 推进插值 */
	void Tick(float DeltaTime);

	/** 暂停 / 恢复（外部 Game Pause 时调用） */
	void OnPause();
	void OnResume();

	/** 当前插值后的速率 */
	float GetCurrentRate() const { return CurrentRate; }

	/** 当前是否在过渡中 */
	bool IsBlending() const { return bBlending; }

	/** 立即重置为目标值（如 QTE 强制取消） */
	void SnapTo(float Rate);

private:
	TWeakObjectPtr<ULevelSequencePlayer> PlayerRef;
	TWeakObjectPtr<UCurveFloat> BlendCurve;

	float CurrentRate = 1.f;
	float StartRate = 1.f;
	float TargetRate = 1.f;
	float BlendDuration = 0.f;
	float BlendElapsed = 0.f;
	bool bBlending = false;
	bool bPaused = false;
	/** SetPlayer 设置过有效 Player；用于检测 Player 中途失效（被 GC / Sequence 停止）并只告警一次 */
	bool bHasValidPlayer = false;

	void ApplyRate(float Rate);
};
