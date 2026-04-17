// Copyright Cinematic QTE System. All Rights Reserved.

#include "QTESectionInterface.h"
#include "Sequencer/MovieSceneQTESection.h"
#include "QTEDataAsset.h"
#include "SequencerSectionPainter.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "QTESectionInterface"

FQTESection::FQTESection(UMovieSceneSection& InSection, TWeakPtr<ISequencer> InSequencer)
	: FSequencerSection(InSection)
	, WeakSequencer(InSequencer)
{
}

UMovieSceneQTESection* FQTESection::GetQTESection() const
{
	return Cast<UMovieSceneQTESection>(WeakSection.Get());
}

/** Section 是否处于 Key 模式（AnimNotify 风格），由 bUseSectionRangeAsDuration 承载 */
static bool IsKeyMode(const UMovieSceneQTESection* Section)
{
	return Section && !Section->bUseSectionRangeAsDuration;
}

FText FQTESection::GetSectionTitle() const
{
	UMovieSceneQTESection* Section = GetQTESection();
	if (!Section)
	{
		return FText::GetEmpty();
	}

	const FString AssetName = Section->QTEDataAsset
		? Section->QTEDataAsset->GetName()
		: LOCTEXT("NoAsset", "<No QTE Asset>").ToString();

	if (IsKeyMode(Section))
	{
		return FText::FromString(FString::Printf(TEXT("QTE: %s"), *AssetName));
	}
	return FText::FromString(FString::Printf(TEXT("QTE(Range): %s"), *AssetName));
}

FText FQTESection::GetSectionToolTip() const
{
	const UMovieSceneQTESection* Section = GetQTESection();
	if (!Section)
	{
		return FText::GetEmpty();
	}

	if (IsKeyMode(Section))
	{
		return LOCTEXT("TipKey",
			"QTE Key (AnimNotify-like): 瞬时触发点，运行时长由 DataAsset.Duration 决定，Section 长度仅作占位。");
	}
	return LOCTEXT("TipRange",
		"QTE Range (AnimNotifyState-like): 区间模式，Section 长度即为 QTE 运行时长。");
}

int32 FQTESection::PaintDiamondIcon(FSequencerSectionPainter& Painter, int32 LayerId) const
{
	const FGeometry& Geo = Painter.SectionGeometry;

	// 菱形图标尺寸：取 Section 高度的 80%，保证视觉醒目
	const float IconSize = FMath::Min(static_cast<float>(Geo.Size.Y) * 0.8f, 20.f);
	const FVector2D IconDim(IconSize, IconSize);

	// 水平方向对齐 Section 左边缘（即 QTE 触发帧），图标向左偏移半个尺寸使其中心落在起点
	// 垂直方向垂直居中
	const FVector2D Offset(-IconSize * 0.5f, (static_cast<float>(Geo.Size.Y) - IconSize) * 0.5f);

	const FSlateBrush* DiamondBrush = FAppStyle::GetBrush("Sequencer.KeyDiamond");
	if (!DiamondBrush)
	{
		return LayerId;
	}

	const FLinearColor Tint = FLinearColor(1.f, 0.85f, 0.2f, 1.f); // 金色，突出"事件点"语义
	FSlateDrawElement::MakeBox(
		Painter.DrawElements,
		LayerId + 1,
		Geo.ToPaintGeometry(IconDim, FSlateLayoutTransform(Offset)),
		DiamondBrush,
		Painter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect,
		Tint
	);

	return LayerId + 2;
}

int32 FQTESection::OnPaintSection(FSequencerSectionPainter& Painter) const
{
	int32 LayerId = Painter.PaintSectionBackground();

	// 仅 Key 模式绘制菱形 icon；Range 模式保持默认矩形条
	if (IsKeyMode(GetQTESection()))
	{
		LayerId = PaintDiamondIcon(Painter, LayerId);
	}
	return LayerId;
}

#undef LOCTEXT_NAMESPACE
