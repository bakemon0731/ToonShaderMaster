#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "AssetRegistry/AssetData.h"
#include "ToonSlotEntry.h"

class USkeletalMesh;
class IDetailsView;
class FAssetThumbnailPool;
class SToonPreviewViewport;
struct FPropertyChangedEvent;

class SToonConverterWidget : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SToonConverterWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SToonConverterWidget();

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SToonConverterWidget"); }

private:
	FString GetMeshPath() const;
	void OnMeshChanged(const FAssetData& AssetData);
	void BuildEntries();

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FToonSlotEntry> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnSlotSelected(TSharedPtr<FToonSlotEntry> Item, ESelectInfo::Type SelectInfo);

	void SetFace(TSharedPtr<FToonSlotEntry> Entry, bool bFace);
	void SetOutline(TSharedPtr<FToonSlotEntry> Entry, bool bOutline);
	void RebuildPreviewMID(FToonSlotEntry& Entry);
	void RebuildOutlinePreviewMID(FToonSlotEntry& Entry);
	void SyncGlobalFromEntries();
	void ApplyGlobalChange(UObject* GlobalObject, FName PropertyName);
	void RefreshAll();
	void OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event);

	FReply OnConvertClicked();
	FReply OnRestoreClicked();

	TWeakObjectPtr<USkeletalMesh> Mesh;
	TArray<TSharedPtr<FToonSlotEntry>> Entries;
	TSharedPtr<FToonSlotEntry> SelectedEntry;

	TSharedPtr<SListView<TSharedPtr<FToonSlotEntry>>> ListView;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<IDetailsView> OutlineDetailsView;
	TSharedPtr<IDetailsView> GlobalMasterDetailsView;
	TSharedPtr<IDetailsView> GlobalOutlineDetailsView;
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;

	/** 全体設定。MM_ToonShader / M_ToonOutline の全パラメータ(BaseColorテクスチャを除く)。変更は全スロットへ一括反映 */
	TObjectPtr<UToonMasterSettings> GlobalMaster = nullptr;
	TObjectPtr<UToonOutlineSettings> GlobalOutline = nullptr;
	/** 全体設定の表示タブ(0=MM_ToonShader, 1=M_ToonOutline) */
	int32 GlobalTab = 0;
	bool bCreateBlueprint = true;
	TSharedPtr<SToonPreviewViewport> Viewport;

	float CameraSpeed = 1.f;
	float LightPitch = -35.f;
	float LightYaw = 150.f;
};
