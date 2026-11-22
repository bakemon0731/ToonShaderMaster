#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "AssetRegistry/AssetData.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ToonSlotSettings.h"

class USkeletalMesh;
class IDetailsView;
class FAssetThumbnailPool;
class SToonPreviewViewport;
struct FPropertyChangedEvent;

/** マテリアルスロット1つ分の編集状態 */
struct FToonSlotEntry
{
	int32 Index = 0;
	FName SlotName;
	FString MaterialName;
	bool bFace = false;
	/** このスロットにアウトラインを付けるか */
	bool bOutline = false;

	// 顔/顔以外のどちらに切り替えても値を失わないよう、両方持つ
	TObjectPtr<UToonMasterSettings> MasterSettings = nullptr;
	TObjectPtr<UToonFaceSettings> FaceSettings = nullptr;

	TObjectPtr<UToonOutlineSettings> OutlineSettings = nullptr;

	/** 3Dプレビュー用のマテリアル */
	TObjectPtr<UMaterialInstanceDynamic> PreviewMID = nullptr;
	TObjectPtr<UMaterialInstanceDynamic> OutlinePreviewMID = nullptr;

	UObject* GetActiveSettings() const
	{
		return bFace ? static_cast<UObject*>(FaceSettings.Get()) : static_cast<UObject*>(MasterSettings.Get());
	}
};

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
	void ApplyGlobalChange(FName PropertyName);
	void OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event);

	FReply OnConvertClicked();

	TWeakObjectPtr<USkeletalMesh> Mesh;
	TArray<TSharedPtr<FToonSlotEntry>> Entries;
	TSharedPtr<FToonSlotEntry> SelectedEntry;

	TSharedPtr<SListView<TSharedPtr<FToonSlotEntry>>> ListView;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<IDetailsView> OutlineDetailsView;
	TSharedPtr<IDetailsView> GlobalDetailsView;
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
	TObjectPtr<UToonGlobalSettings> GlobalSettings = nullptr;
	bool bCreateBlueprint = true;
	TSharedPtr<SToonPreviewViewport> Viewport;

	float LightPitch = -35.f;
	float LightYaw = 150.f;
};
