#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "AssetRegistry/AssetData.h"

class USkeletalMesh;

struct FToonSlotRow
{
	int32 Index = 0;
	FName SlotName;
	FString MaterialName;
	bool bFace = false;
};

class SToonConverterWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SToonConverterWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FString GetMeshPath() const;
	void OnMeshChanged(const FAssetData& AssetData);
	void RebuildRows();
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FToonSlotRow> Item, const TSharedRef<STableViewBase>& OwnerTable);
	FReply OnConvertClicked();

	TWeakObjectPtr<USkeletalMesh> Mesh;
	TArray<TSharedPtr<FToonSlotRow>> Rows;
	TSharedPtr<SListView<TSharedPtr<FToonSlotRow>>> ListView;
};
