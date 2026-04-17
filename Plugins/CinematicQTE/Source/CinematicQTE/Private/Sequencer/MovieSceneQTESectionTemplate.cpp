// Copyright Cinematic QTE System. All Rights Reserved.

#include "Sequencer/MovieSceneQTESectionTemplate.h"
#include "Sequencer/MovieSceneQTESection.h"
#include "Evaluation/MovieSceneExecutionTokens.h"
#include "IMovieScenePlayer.h"
#include "CinematicQTESubsystem.h"
#include "CinematicQTEModule.h"
#include "QTEDataAsset.h"
#include "LevelSequencePlayer.h"

namespace
{
	/** 执行 Token：推迟到 Tokens Apply 阶段，由主线程 WorldContext 调用 Subsystem */
	struct FQTEExecutionToken : IMovieSceneExecutionToken
	{
		TObjectPtr<UQTEDataAsset> DataAsset;
		uint8 ConflictPolicy;

		FQTEExecutionToken(UQTEDataAsset* InAsset, uint8 InPolicy)
			: DataAsset(InAsset), ConflictPolicy(InPolicy) {}

		virtual void Execute(const FMovieSceneContext& Context,
			const FMovieSceneEvaluationOperand& Operand,
			FPersistentEvaluationData& PersistentData,
			IMovieScenePlayer& Player) override
		{
			if (!DataAsset) return;

			UObject* PlaybackContext = Player.GetPlaybackContext();
			UWorld* World = PlaybackContext ? PlaybackContext->GetWorld() : nullptr;
			if (!World) return;

			UCinematicQTESubsystem* Sub = World->GetSubsystem<UCinematicQTESubsystem>();
			if (!Sub) return;

			// 尝试将 Player 当成 ULevelSequencePlayer（常见情况）
			ULevelSequencePlayer* SeqPlayer = Cast<ULevelSequencePlayer>(PlaybackContext);
			// 若 PlaybackContext 不是 Player 本身，则从 GetEvaluationState 等途径获取失败时传 nullptr
			Sub->StartQTE(DataAsset, SeqPlayer, static_cast<EQTEConflictPolicy>(ConflictPolicy));

			UE_LOG(LogCinematicQTE, Log, TEXT("Sequencer QTE fired: Asset=%s"), *DataAsset->GetName());
		}
	};
}

FMovieSceneQTESectionTemplate::FMovieSceneQTESectionTemplate(const UMovieSceneQTESection& InSection)
	: QTEDataAsset(InSection.QTEDataAsset)
	, ConflictPolicy(InSection.ConflictPolicy)
{
	if (InSection.HasStartFrame())
	{
		StartFrame = InSection.GetInclusiveStartFrame();
	}
}

void FMovieSceneQTESectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand,
	const FMovieSceneContext& Context,
	const FPersistentEvaluationData& PersistentData,
	FMovieSceneExecutionTokens& ExecutionTokens) const
{
	if (!QTEDataAsset)
	{
		return;
	}

	// 只在时间正向越过 StartFrame 时触发（避免往复评估重复触发）
	const FFrameTime Current = Context.GetTime();
	const FFrameTime Previous = Context.GetPreviousTime();

	// 仅前进方向
	if (Context.GetDirection() != EPlayDirection::Forwards)
	{
		return;
	}
	// 当前首次越过起始帧
	if (Previous < FFrameTime(StartFrame) && Current >= FFrameTime(StartFrame))
	{
		ExecutionTokens.Add(FQTEExecutionToken(QTEDataAsset, ConflictPolicy));
	}
}
