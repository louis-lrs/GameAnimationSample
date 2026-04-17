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
	/** Start Token：在 Section 起始帧触发 QTE */
	struct FQTEStartToken : IMovieSceneExecutionToken
	{
		TObjectPtr<UQTEDataAsset> DataAsset;
		EQTEConflictPolicy ConflictPolicy;
		FGuid OwnerToken;
		float OverrideDuration;  // <=0 表示走 DA.Duration

		FQTEStartToken(UQTEDataAsset* InAsset, EQTEConflictPolicy InPolicy, FGuid InOwner, float InOverride)
			: DataAsset(InAsset), ConflictPolicy(InPolicy), OwnerToken(InOwner), OverrideDuration(InOverride) {}

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

			ULevelSequencePlayer* SeqPlayer = Cast<ULevelSequencePlayer>(Player.AsUObject());
			ensureAlwaysMsgf(SeqPlayer != nullptr,
				TEXT("CinematicQTE: failed to resolve ULevelSequencePlayer from IMovieScenePlayer (Asset=%s). ")
				TEXT("Slow-motion will NOT be applied."),
				*DataAsset->GetName());

			Sub->StartQTE(DataAsset, SeqPlayer, ConflictPolicy, OverrideDuration, OwnerToken);

			UE_LOG(LogCinematicQTE, Log, TEXT("Sequencer QTE fired: Asset=%s OverrideDuration=%.2f Owner=%s"),
				*DataAsset->GetName(), OverrideDuration, *OwnerToken.ToString());
		}
	};

	/** End Token：B 模式下 Section 尾端到达时，若 QTE 仍属本 Section 则 Timeout */
	struct FQTEEndToken : IMovieSceneExecutionToken
	{
		FGuid OwnerToken;
		explicit FQTEEndToken(FGuid InOwner) : OwnerToken(InOwner) {}

		virtual void Execute(const FMovieSceneContext& Context,
			const FMovieSceneEvaluationOperand& Operand,
			FPersistentEvaluationData& PersistentData,
			IMovieScenePlayer& Player) override
		{
			UObject* PlaybackContext = Player.GetPlaybackContext();
			UWorld* World = PlaybackContext ? PlaybackContext->GetWorld() : nullptr;
			if (!World) return;

			UCinematicQTESubsystem* Sub = World->GetSubsystem<UCinematicQTESubsystem>();
			if (!Sub) return;

			Sub->CancelIfOwnedBy(OwnerToken, EQTEResult::Timeout);
		}
	};
}

FMovieSceneQTESectionTemplate::FMovieSceneQTESectionTemplate(const UMovieSceneQTESection& InSection)
	: QTEDataAsset(InSection.QTEDataAsset)
	, ConflictPolicy(InSection.ConflictPolicy)
	, bUseRange(InSection.bUseSectionRangeAsDuration)
	, SectionSignature(InSection.GetSignature())
{
	if (InSection.HasStartFrame())
	{
		StartFrame = InSection.GetInclusiveStartFrame();
	}
	if (InSection.HasEndFrame())
	{
		EndFrame = InSection.GetExclusiveEndFrame();
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

	// 仅前进方向触发
	if (Context.GetDirection() != EPlayDirection::Forwards)
	{
		return;
	}

	const FFrameTime Current = Context.GetTime();
	const FFrameTime Previous = Context.GetPreviousTime();

	// Start：首次越过起始帧
	if (Previous < FFrameTime(StartFrame) && Current >= FFrameTime(StartFrame))
	{
		// B 模式下以 Section 长度为 OverrideDuration；A 模式 OverrideDuration<=0 由 Task 走 DA.Duration
		float OverrideDuration = -1.f;
		if (bUseRange && EndFrame > StartFrame)
		{
			const FFrameRate TickResolution = Context.GetFrameRate();
			const double Seconds = TickResolution.AsSeconds(FFrameTime(EndFrame - StartFrame));
			OverrideDuration = static_cast<float>(Seconds);
		}
		ExecutionTokens.Add(FQTEStartToken(QTEDataAsset, ConflictPolicy, SectionSignature, OverrideDuration));
	}

	// End：仅 B 模式；首次越过结束帧时通知 Subsystem 以 Timeout 结束
	if (bUseRange && EndFrame > StartFrame)
	{
		if (Previous < FFrameTime(EndFrame) && Current >= FFrameTime(EndFrame))
		{
			ExecutionTokens.Add(FQTEEndToken(SectionSignature));
		}
	}
}
