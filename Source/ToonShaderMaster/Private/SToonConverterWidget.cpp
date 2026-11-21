#include "SToonConverterWidget.h"

#include "SToonPreviewViewport.h"
#include "ToonConverter.h"
#include "ToonParamUtil.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "PropertyCustomizationHelpers.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Misc/MessageDialog.h"

void SToonConverterWidget::Construct(const FArguments& InArgs)
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = false;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bShowOptions = false;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyModule.CreateDetailView(DetailsArgs);

	// スライダー操作中も含めて、設定値の変更をプレビューへ即反映する
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &SToonConverterWidget::OnObjectPropertyChanged);

	ChildSlot
	[
		SNew(SSplitter)

		// 左: 3Dプレビュー
		+ SSplitter::Slot().Value(0.55f)
		[
			SAssignNew(Viewport, SToonPreviewViewport)
		]

		// 右: 操作パネル
		+ SSplitter::Slot().Value(0.45f)
		[
			SNew(SVerticalBox)

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

			// ライト向き
			+ SVerticalBox::Slot().AutoHeight().Padding(8, 0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("ライト Pitch")))
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0, 0, 12, 0)
				[
					SNew(SSpinBox<float>)
					.MinValue(-90.f).MaxValue(90.f)
					.Value_Lambda([this]() { return LightPitch; })
					.OnValueChanged_Lambda([this](float V)
					{
						LightPitch = V;
						Viewport->SetLightRotation(LightPitch, LightYaw);
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Yaw")))
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SSpinBox<float>)
					.MinValue(-180.f).MaxValue(180.f)
					.Value_Lambda([this]() { return LightYaw; })
					.OnValueChanged_Lambda([this](float V)
					{
						LightYaw = V;
						Viewport->SetLightRotation(LightPitch, LightYaw);
					})
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(8, 8, 8, 0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("スロット(顔のスロットにチェック。行を選ぶと下で調整できます)")))
				.AutoWrapText(true)
			]

			// スロット一覧
			+ SVerticalBox::Slot().AutoHeight().Padding(8)
			[
				SNew(SBox).MaxDesiredHeight(160.f)
				[
					SAssignNew(ListView, SListView<TSharedPtr<FToonSlotEntry>>)
					.ListItemsSource(&Entries)
					.OnGenerateRow(this, &SToonConverterWidget::OnGenerateRow)
					.OnSelectionChanged(this, &SToonConverterWidget::OnSlotSelected)
					.SelectionMode(ESelectionMode::Single)
				]
			]

			// 選択スロットのパラメータ
			+ SVerticalBox::Slot().FillHeight(1.f).Padding(8, 0)
			[
				DetailsView.ToSharedRef()
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(8)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(FText::FromString(TEXT("変換して適用")))
				.OnClicked(this, &SToonConverterWidget::OnConvertClicked)
			]
		]
	];

	Viewport->SetLightRotation(LightPitch, LightYaw);
}

SToonConverterWidget::~SToonConverterWidget()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
}

void SToonConverterWidget::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (TSharedPtr<FToonSlotEntry>& E : Entries)
	{
		if (E.IsValid())
		{
			Collector.AddReferencedObject(E->MasterSettings);
			Collector.AddReferencedObject(E->FaceSettings);
			Collector.AddReferencedObject(E->PreviewMID);
		}
	}
}

FString SToonConverterWidget::GetMeshPath() const
{
	return Mesh.IsValid() ? Mesh->GetPathName() : FString();
}

void SToonConverterWidget::OnMeshChanged(const FAssetData& AssetData)
{
	Mesh = Cast<USkeletalMesh>(AssetData.GetAsset());
	BuildEntries();

	Viewport->SetMesh(Mesh.Get());
	for (TSharedPtr<FToonSlotEntry>& E : Entries)
	{
		RebuildPreviewMID(*E);
	}

	if (Entries.Num() > 0)
	{
		ListView->SetSelection(Entries[0]);
	}
}

void SToonConverterWidget::BuildEntries()
{
	Entries.Reset();
	SelectedEntry.Reset();
	DetailsView->SetObject(nullptr);

	if (USkeletalMesh* M = Mesh.Get())
	{
		UMaterialInterface* MasterMat = FToonConverter::GetMaster(false);
		UMaterialInterface* FaceMat = FToonConverter::GetMaster(true);

		const TArray<FSkeletalMaterial>& Mats = M->GetMaterials();
		for (int32 i = 0; i < Mats.Num(); ++i)
		{
			UMaterialInterface* Orig = Mats[i].MaterialInterface;

			TSharedPtr<FToonSlotEntry> E = MakeShared<FToonSlotEntry>();
			E->Index = i;
			E->SlotName = Mats[i].MaterialSlotName;
			E->MaterialName = Orig ? Orig->GetName() : TEXT("None");
			E->MasterSettings = NewObject<UToonMasterSettings>(GetTransientPackage());
			E->FaceSettings = NewObject<UToonFaceSettings>(GetTransientPackage());

			// 顔マスターに設定済みの値(FaceSDFのハード参照テクスチャなど)を初期値にする
			if (FaceMat)
			{
				ToonParamUtil::LoadFromMaterial(E->FaceSettings, FaceMat);
			}

			// 元マテリアルのテクスチャを初期値にする(BaseColorはマスターの値より優先)
			if (UTexture2D* Src = Cast<UTexture2D>(FToonConverter::FindSourceTexture(Orig)))
			{
				E->MasterSettings->BaseColor = Src;
				E->FaceSettings->BaseColor = Src;
			}

			// すでにトゥーンMIが適用済みなら、顔かどうかと調整値を読み戻す
			if (const UMaterialInstance* MI = Cast<UMaterialInstance>(Orig))
			{
				if (FaceMat && MI->Parent == FaceMat)
				{
					E->bFace = true;
					ToonParamUtil::LoadFromMaterial(E->FaceSettings, Orig);
				}
				else if (MasterMat && MI->Parent == MasterMat)
				{
					ToonParamUtil::LoadFromMaterial(E->MasterSettings, Orig);
				}
			}

			Entries.Add(E);
		}
	}

	ListView->RequestListRefresh();
}

TSharedRef<ITableRow> SToonConverterWidget::OnGenerateRow(TSharedPtr<FToonSlotEntry> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FToonSlotEntry>>, OwnerTable)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([Item]()
			{
				return Item->bFace ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([this, Item](ECheckBoxState NewState)
			{
				SetFace(Item, NewState == ECheckBoxState::Checked);
			})
		]

		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0)
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("顔")))
		]

		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(8, 0)
		[
			SNew(STextBlock)
			.Text_Lambda([Item]()
			{
				return FText::FromString(FString::Printf(TEXT("[%d] %s  (%s)"),
					Item->Index, *Item->SlotName.ToString(), *Item->MaterialName));
			})
		]
	];
}

void SToonConverterWidget::OnSlotSelected(TSharedPtr<FToonSlotEntry> Item, ESelectInfo::Type SelectInfo)
{
	SelectedEntry = Item;
	DetailsView->SetObject(Item.IsValid() ? Item->GetActiveSettings() : nullptr);
}

void SToonConverterWidget::SetFace(TSharedPtr<FToonSlotEntry> Entry, bool bFace)
{
	if (!Entry.IsValid())
	{
		return;
	}

	Entry->bFace = bFace;
	RebuildPreviewMID(*Entry);

	if (SelectedEntry == Entry)
	{
		DetailsView->SetObject(Entry->GetActiveSettings());
	}
}

void SToonConverterWidget::RebuildPreviewMID(FToonSlotEntry& Entry)
{
	UMaterialInterface* Parent = FToonConverter::GetMaster(Entry.bFace);
	if (!Parent)
	{
		return;
	}

	Entry.PreviewMID = UMaterialInstanceDynamic::Create(Parent, GetTransientPackage());
	ToonParamUtil::ApplyToMID(Entry.GetActiveSettings(), Entry.PreviewMID);
	Viewport->SetSlotMaterial(Entry.Index, Entry.PreviewMID);
}

void SToonConverterWidget::OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
{
	if (!Object)
	{
		return;
	}

	for (const TSharedPtr<FToonSlotEntry>& E : Entries)
	{
		if (E.IsValid() && E->PreviewMID && E->GetActiveSettings() == Object)
		{
			ToonParamUtil::ApplyToMID(Object, E->PreviewMID);
			return;
		}
	}
}

FReply SToonConverterWidget::OnConvertClicked()
{
	USkeletalMesh* M = Mesh.Get();
	if (!M)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("スケルタルメッシュを選択してください。")));
		return FReply::Handled();
	}

	TArray<FToonConvertSlotInput> Inputs;
	for (const TSharedPtr<FToonSlotEntry>& E : Entries)
	{
		FToonConvertSlotInput In;
		In.bFace = E->bFace;
		In.Settings = E->GetActiveSettings();
		Inputs.Add(In);
	}

	FToonConvertResult Result;
	FToonConverter::Convert(M, Inputs, Result);

	// メッシュ側の更新でプレビューの上書きが外れないよう、プレビューを作り直す
	const TArray<FSkeletalMaterial>& Mats = M->GetMaterials();
	for (const TSharedPtr<FToonSlotEntry>& E : Entries)
	{
		if (Mats.IsValidIndex(E->Index) && Mats[E->Index].MaterialInterface)
		{
			E->MaterialName = Mats[E->Index].MaterialInterface->GetName();
		}
		RebuildPreviewMID(*E);
	}
	ListView->RequestListRefresh();

	FString Msg = FString::Printf(TEXT("新規作成: %d / 再利用: %d"), Result.Created, Result.Reused);
	for (const FString& Err : Result.Errors)
	{
		Msg += TEXT("\n") + Err;
	}
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Msg));

	return FReply::Handled();
}
