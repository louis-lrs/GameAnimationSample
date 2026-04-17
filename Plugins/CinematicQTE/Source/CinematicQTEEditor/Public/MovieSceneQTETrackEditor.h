// Copyright Cinematic QTE System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneTrackEditor.h"

class ISequencer;
class UMovieSceneTrack;
class UMovieSceneQTETrack;
class UQTEDataAsset;

/**
 * QTE Track 的 Sequencer 编辑器支持。
 * 在 Sequencer 菜单中提供 "Add QTE Track" 项。
 */
class FMovieSceneQTETrackEditor : public FMovieSceneTrackEditor
{
public:
	FMovieSceneQTETrackEditor(TSharedRef<ISequencer> InSequencer);
	virtual ~FMovieSceneQTETrackEditor() = default;

	/** Track Editor 工厂函数 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> InSequencer);

	// ====== ISequencerTrackEditor ======
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const override;
	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override;
	virtual void BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track) override;
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject,
		UMovieSceneTrack& Track, FGuid ObjectBinding) override;

private:
	void HandleAddQTETrack();

	/** 添加 QTE Section（单一入口）。默认长度 0.25s，运行时长由 Section.bUseSectionRangeAsDuration 控制 */
	void HandleAddQTESection(UMovieSceneTrack* Track);
};
