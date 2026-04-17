// Copyright Cinematic QTE System. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISequencerSection.h"

class ISequencer;
class UMovieSceneQTESection;
class FSequencerSectionPainter;

/**
 * QTE Section 的 Sequencer 可视化接口。
 *
 * 对齐 AnimNotify / AnimNotifyState 的视觉语义；两种模式由 Section 自身的
 * bUseSectionRangeAsDuration 字段承载：
 *   - Key 模式（bUseSectionRangeAsDuration=false，默认）：Section 渲染为带菱形 icon 的
 *     矩形条，表示"瞬时触发点 + DataAsset.Duration 驱动的固定时长"；
 *   - Range 模式（bUseSectionRangeAsDuration=true）：Section 渲染为默认矩形条，
 *     Section 长度即为 QTE 运行时长。
 *
 * 两种模式下 Section 均可自由拖动位置 / 拉伸长度 / 在 Details 面板中编辑属性，
 * 不使用 UE 原生的 IsLocked（它会禁用整块交互）。
 */
class FQTESection : public FSequencerSection
{
public:
	FQTESection(UMovieSceneSection& InSection, TWeakPtr<ISequencer> InSequencer);

	// ====== ISequencerSection ======
	virtual int32 OnPaintSection(FSequencerSectionPainter& Painter) const override;
	virtual FText GetSectionTitle() const override;
	virtual FText GetSectionToolTip() const override;

private:
	/** 在 Key 模式下于 Section 起点绘制菱形 icon；返回新的 LayerId */
	int32 PaintDiamondIcon(FSequencerSectionPainter& Painter, int32 LayerId) const;

	UMovieSceneQTESection* GetQTESection() const;

	TWeakPtr<ISequencer> WeakSequencer;
};
