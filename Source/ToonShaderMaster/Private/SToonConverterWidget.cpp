#include "SToonConverterWidget.h"

#include "ToonConverter.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Misc/MessageDialog.h"

void SToonConverterWidget::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)

		// メッシュ選択欄(「選択中のアセットを使用」ボタン付き)
		+ SVerticalBox::Slot().AutoHeight().Padding(8)
		[
			SNew(SObjectPropertyEntryBox)
			.AllowedClass(USkeletalMesh::StaticClass())
			.ObjectPath(this, &SToonConverterWidget::GetMeshPath)
			.OnObjectChanged(this, &SToonConverterWidget::OnMeshChanged)
			.AllowClear(true)
			.DisplayUseSelected(true)
			.DisplayBrowse(true)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 0)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("顔のスロットにチェックを入れてください(M_ToonFaceShaderのMIになります。未チェックはMM_ToonShaderのMI)")))
			.AutoWrapText(true)
		]

		// スロット一覧
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(8)
		[
			SAssignNew(ListView, SListView<TSharedPtr<FToonSlotRow>>)
			.ListItemsSource(&Rows)
			.OnGenerateRow(this, &SToonConverterWidget::OnGenerateRow)
			.SelectionMode(ESelectionMode::None)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.Text(FText::FromString(TEXT("変換して適用")))
			.OnClicked(this, &SToonConverterWidget::OnConvertClicked)
		]
	];
}

FString SToonConverterWidget::GetMeshPath() const
{
	return Mesh.IsValid() ? Mesh->GetPathName() : FString();
}

void SToonConverterWidget::OnMeshChanged(const FAssetData& AssetData)
{
	Mesh = Cast<USkeletalMesh>(AssetData.GetAsset());
	RebuildRows();
}

void SToonConverterWidget::RebuildRows()
{
	Rows.Reset();

	if (USkeletalMesh* M = Mesh.Get())
	{
		const TArray<FSkeletalMaterial>& Mats = M->GetMaterials();
		for (int32 i = 0; i < Mats.Num(); ++i)
		{
			TSharedPtr<FToonSlotRow> Row = MakeShared<FToonSlotRow>();
			Row->Index = i;
			Row->SlotName = Mats[i].MaterialSlotName;
			Row->MaterialName = Mats[i].MaterialInterface ? Mats[i].MaterialInterface->GetName() : TEXT("None");
			Rows.Add(Row);
		}
	}

	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SToonConverterWidget::OnGenerateRow(TSharedPtr<FToonSlotRow> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FToonSlotRow>>, OwnerTable)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([Item]()
			{
				return Item->bFace ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([Item](ECheckBoxState NewState)
			{
				Item->bFace = (NewState == ECheckBoxState::Checked);
			})
		]

		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0)
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("顔")))
		]

		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(8, 0)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("[%d] %s  (%s)"),
				Item->Index, *Item->SlotName.ToString(), *Item->MaterialName)))
		]
	];
}

FReply SToonConverterWidget::OnConvertClicked()
{
	USkeletalMesh* M = Mesh.Get();
	if (!M)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("スケルタルメッシュを選択してください。")));
		return FReply::Handled();
	}

	TSet<int32> FaceSlots;
	for (const TSharedPtr<FToonSlotRow>& Row : Rows)
	{
		if (Row->bFace)
		{
			FaceSlots.Add(Row->Index);
		}
	}

	FToonConvertResult Result;
	FToonConverter::Convert(M, FaceSlots, Result);

	FString Msg = FString::Printf(TEXT("新規作成: %d / 再利用: %d"), Result.Created, Result.Reused);
	for (const FString& Err : Result.Errors)
	{
		Msg += TEXT("\n") + Err;
	}
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Msg));

	RebuildRows();
	return FReply::Handled();
}
