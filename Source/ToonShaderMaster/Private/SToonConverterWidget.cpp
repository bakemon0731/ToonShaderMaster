#include "SToonConverterWidget.h"

#include "SToonPreviewViewport.h"
#include "ToonConverter.h"
#include "ToonParamUtil.h"
#include "ToonOutlineBlueprint.h"
#include "AssetThumbnail.h"
#include "Engine/Blueprint.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
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
	OutlineDetailsView = PropertyModule.CreateDetailView(DetailsArgs);
	GlobalDetailsView = PropertyModule.CreateDetailView(DetailsArgs);

	GlobalSettings = NewObject<UToonGlobalSettings>(GetTransientPackage());
	GlobalDetailsView->SetObject(GlobalSettings);
	ThumbnailPool = MakeShared<FAssetThumbnailPool>(64);

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

			// 全体設定(全スロットへ一括反映)
			+ SVerticalBox::Slot().AutoHeight().Padding(8, 8, 8, 0)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("全体設定(変更すると全スロットへ一括反映)")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(8, 0)
			[
				SNew(SBox).MaxDesiredHeight(130.f)
				[
					GlobalDetailsView.ToSharedRef()
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(8, 8, 8, 0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("スロット(顔=顔用シェーダー / 線=アウトラインを付ける。行を選ぶと下で調整できます)")))
				.AutoWrapText(true)
			]

			// スロット一覧
			+ SVerticalBox::Slot().AutoHeight().Padding(8)
			[
				SNew(SBox).MaxDesiredHeight(240.f)
				[
					SAssignNew(ListView, SListView<TSharedPtr<FToonSlotEntry>>)
					.ListItemsSource(&Entries)
					.OnGenerateRow(this, &SToonConverterWidget::OnGenerateRow)
					.OnSelectionChanged(this, &SToonConverterWidget::OnSlotSelected)
					.SelectionMode(ESelectionMode::Single)
				]
			]

			// 選択スロットのパラメータ(トゥーン)
			+ SVerticalBox::Slot().FillHeight(1.f).Padding(8, 0)
			[
				DetailsView.ToSharedRef()
			]

			// 選択スロットのパラメータ(アウトライン)
			+ SVerticalBox::Slot().AutoHeight().Padding(8, 8, 8, 0)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("アウトライン設定(選択中のスロット)")))
			]
			+ SVerticalBox::Slot().FillHeight(0.7f).Padding(8, 0)
			[
				OutlineDetailsView.ToSharedRef()
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(8, 8, 8, 0)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bCreateBlueprint ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState S) { bCreateBlueprint = (S == ECheckBoxState::Checked); })
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("アウトライン用のBP Actorも作る(親:トゥーン / 子:アウトライン)")))
				]
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
	Collector.AddReferencedObject(GlobalSettings);
	for (TSharedPtr<FToonSlotEntry>& E : Entries)
	{
		if (E.IsValid())
		{
			Collector.AddReferencedObject(E->MasterSettings);
			Collector.AddReferencedObject(E->FaceSettings);
			Collector.AddReferencedObject(E->OutlineSettings);
			Collector.AddReferencedObject(E->PreviewMID);
			Collector.AddReferencedObject(E->OutlinePreviewMID);
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
		RebuildOutlinePreviewMID(*E);
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
	OutlineDetailsView->SetObject(nullptr);

	if (USkeletalMesh* M = Mesh.Get())
	{
		UMaterialInterface* MasterMat = FToonConverter::GetMaster(false);
		UMaterialInterface* FaceMat = FToonConverter::GetMaster(true);
		UMaterialInterface* OutlineMat = FToonConverter::GetOutlineMaster();

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
			E->OutlineSettings = NewObject<UToonOutlineSettings>(GetTransientPackage());

			// アウトライン: マスターの値を初期値にし、作成済みのアウトラインMIがあればその値と有効状態を読み戻す
			if (OutlineMat)
			{
				ToonParamUtil::LoadFromMaterial(E->OutlineSettings, OutlineMat);
			}
			{
				// 作成済みのアウトラインMIがあり、親が M_ToonOutline ならチェック済みとして値を読み戻す
				// (チェックを外したときは親が M_Empty に付け替えられている)
				const FString OutlinePkg = FToonConverter::GetOutlineMIPackageName(M, E->SlotName);
				if (FPackageName::DoesPackageExist(OutlinePkg))
				{
					const FString OutlineObj = OutlinePkg + TEXT(".") + FPackageName::GetLongPackageAssetName(OutlinePkg);
					if (UMaterialInstanceConstant* OMI = LoadObject<UMaterialInstanceConstant>(nullptr, *OutlineObj))
					{
						ToonParamUtil::LoadFromMaterial(E->OutlineSettings, OMI);
						E->bOutline = (OutlineMat && OMI->Parent == OutlineMat);
					}
				}
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

	SyncGlobalFromEntries();

	ListView->RequestListRefresh();
}

TSharedRef<ITableRow> SToonConverterWidget::OnGenerateRow(TSharedPtr<FToonSlotEntry> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	// スロットに今入っているマテリアルのサムネイル(コンテンツブラウザと同じ見た目)
	TSharedRef<SWidget> ThumbWidget = SNew(SBox).WidthOverride(64.f).HeightOverride(64.f);
	if (Mesh.IsValid() && Mesh->GetMaterials().IsValidIndex(Item->Index))
	{
		if (UMaterialInterface* SlotMat = Mesh->GetMaterials()[Item->Index].MaterialInterface)
		{
			TSharedRef<FAssetThumbnail> Thumb = MakeShared<FAssetThumbnail>(FAssetData(SlotMat), 64, 64, ThumbnailPool);
			ThumbWidget = SNew(SBox).WidthOverride(64.f).HeightOverride(64.f)
			[
				Thumb->MakeThumbnailWidget()
			];
		}
	}

	return SNew(STableRow<TSharedPtr<FToonSlotEntry>>, OwnerTable)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4)
		[
			ThumbWidget
		]

		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(4)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text_Lambda([Item]()
				{
					return FText::FromString(FString::Printf(TEXT("[%d] %s  (%s)"),
						Item->Index, *Item->SlotName.ToString(), *Item->MaterialName));
				})
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
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
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 16, 0)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("顔")))
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([Item]()
					{
						return Item->bOutline ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([this, Item](ECheckBoxState NewState)
					{
						SetOutline(Item, NewState == ECheckBoxState::Checked);
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("線")))
				]
			]
		]
	];
}

void SToonConverterWidget::OnSlotSelected(TSharedPtr<FToonSlotEntry> Item, ESelectInfo::Type SelectInfo)
{
	SelectedEntry = Item;
	DetailsView->SetObject(Item.IsValid() ? Item->GetActiveSettings() : nullptr);
	OutlineDetailsView->SetObject(Item.IsValid() ? Item->OutlineSettings.Get() : nullptr);
}

void SToonConverterWidget::SetOutline(TSharedPtr<FToonSlotEntry> Entry, bool bOutline)
{
	if (!Entry.IsValid())
	{
		return;
	}

	Entry->bOutline = bOutline;
	RebuildOutlinePreviewMID(*Entry);
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

void SToonConverterWidget::RebuildOutlinePreviewMID(FToonSlotEntry& Entry)
{
	UMaterialInterface* Parent = FToonConverter::GetOutlineMaster();
	if (!Parent)
	{
		return;
	}

	Entry.OutlinePreviewMID = UMaterialInstanceDynamic::Create(Parent, GetTransientPackage());
	// Static SwitchはMIDでは変えられないため、プレビューはマスターの既定のまま(MI生成時に反映される)
	ToonParamUtil::ApplyToMID(Entry.OutlineSettings, Entry.OutlinePreviewMID);

	// チェックなしのスロットは M_Empty(何も描画しない)
	UMaterialInterface* Mat = Entry.OutlinePreviewMID;
	if (!Entry.bOutline)
	{
		if (UMaterialInterface* Empty = FToonConverter::GetEmptyMaterial())
		{
			Mat = Empty;
		}
	}
	Viewport->SetOutlineSlotMaterial(Entry.Index, Mat);
}

void SToonConverterWidget::SyncGlobalFromEntries()
{
	if (Entries.Num() == 0 || !GlobalSettings)
	{
		return;
	}

	// 全体設定の表示は先頭スロットの値に合わせる
	const TSharedPtr<FToonSlotEntry>& First = Entries[0];
	GlobalSettings->ShadowColor = First->MasterSettings->ShadowColor;
	GlobalSettings->ShadowOuterColor = First->MasterSettings->ShadowOuterColor;
	GlobalSettings->OutlineThickness = First->OutlineSettings->OutlineThickness;
	GlobalDetailsView->ForceRefresh();
}

void SToonConverterWidget::ApplyGlobalChange(FName PropertyName)
{
	const bool bAll = PropertyName.IsNone();

	for (const TSharedPtr<FToonSlotEntry>& E : Entries)
	{
		if (!E.IsValid())
		{
			continue;
		}

		if (bAll || PropertyName == GET_MEMBER_NAME_CHECKED(UToonGlobalSettings, ShadowColor))
		{
			E->MasterSettings->ShadowColor = GlobalSettings->ShadowColor;
		}
		if (bAll || PropertyName == GET_MEMBER_NAME_CHECKED(UToonGlobalSettings, ShadowOuterColor))
		{
			E->MasterSettings->ShadowOuterColor = GlobalSettings->ShadowOuterColor;
		}
		if (bAll || PropertyName == GET_MEMBER_NAME_CHECKED(UToonGlobalSettings, OutlineThickness))
		{
			E->OutlineSettings->OutlineThickness = GlobalSettings->OutlineThickness;
		}

		if (E->PreviewMID)
		{
			ToonParamUtil::ApplyToMID(E->GetActiveSettings(), E->PreviewMID);
		}
		if (E->OutlinePreviewMID)
		{
			ToonParamUtil::ApplyToMID(E->OutlineSettings, E->OutlinePreviewMID);
		}
	}

	// 選択中スロットの表示を更新
	DetailsView->ForceRefresh();
	OutlineDetailsView->ForceRefresh();
}

void SToonConverterWidget::OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
{
	if (!Object)
	{
		return;
	}

	if (Object == GlobalSettings.Get())
	{
		ApplyGlobalChange(Event.Property ? Event.Property->GetFName() : NAME_None);
		return;
	}

	for (const TSharedPtr<FToonSlotEntry>& E : Entries)
	{
		if (!E.IsValid())
		{
			continue;
		}
		if (E->PreviewMID && E->GetActiveSettings() == Object)
		{
			ToonParamUtil::ApplyToMID(Object, E->PreviewMID);
			return;
		}
		if (E->OutlinePreviewMID && E->OutlineSettings == Object)
		{
			ToonParamUtil::ApplyToMID(Object, E->OutlinePreviewMID);
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

	// アウトライン: 1つでもチェックがあれば、チェックしたスロットのMIを作り(他はM_Empty)、BP Actorも作る
	bool bAnyOutline = false;
	TArray<FToonOutlineSlotInput> OutlineInputs;
	for (const TSharedPtr<FToonSlotEntry>& E : Entries)
	{
		FToonOutlineSlotInput In;
		In.bEnabled = E->bOutline;
		In.Settings = E->OutlineSettings;
		bAnyOutline |= E->bOutline;
		OutlineInputs.Add(In);
	}
	if (bAnyOutline)
	{
		if (FToonConverter::ConvertOutline(M, OutlineInputs, Result) && bCreateBlueprint)
		{
			if (UBlueprint* BP = ToonOutlineBlueprint::Create(M, Result.OutlineMaterials, Result.BlueprintNote))
			{
				Result.BlueprintPath = BP->GetPathName();
			}
		}
	}

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
	ListView->RebuildList();

	FString Msg = FString::Printf(TEXT("新規作成: %d / 再利用: %d"), Result.Created, Result.Reused);
	if (!Result.BlueprintPath.IsEmpty())
	{
		Msg += TEXT("\nBP Actorを作成: ") + Result.BlueprintPath;
	}
	if (!Result.BlueprintNote.IsEmpty())
	{
		Msg += TEXT("\n") + Result.BlueprintNote;
	}
	for (const FString& Err : Result.Errors)
	{
		Msg += TEXT("\n") + Err;
	}
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Msg));

	return FReply::Handled();
}
