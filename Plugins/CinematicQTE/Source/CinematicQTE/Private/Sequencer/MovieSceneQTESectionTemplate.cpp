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
		EQTEConflictPolicy ConflictPolicy;

		FQTEExecutionToken(UQTEDataAsset* InAsset, EQTEConflictPolicy InPolicy)
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

			// 通过 IMovieScenePlayer::AsUObject() 拿到真正的 Player 实例。
			// 注意：GetPlaybackContext() 返回的是 ALevelSequenceActor 或 UWorld，永远不是 Player 本身，
			// 这里必须用 AsUObject()，否则 SlowMotion 的 SetPlayRate 永远不会生效。
			ULevelSequencePlayer* SeqPlayer = Cast<ULevelSequencePlayer>(Player.AsUObject());
			ensureAlwaysMsgf(SeqPlayer != nullptr,
				TEXT("CinematicQTE: failed to resolve ULevelSequencePlayer from IMovieScenePlayer (Asset=%s). ")
				TEXT("Slow-motion will NOT be applied."),
				*DataAsset->GetName());

			Sub->StartQTE(DataAsset, SeqPlayer, ConflictPolicy);

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
