#include "SToonConverterWidget.h"

#include "SToonPreviewViewport.h"
#include "ToonConverter.h"
#include "ToonParamUtil.h"
#include "ToonOutlineBlueprint.h"
#include "AssetThumbnail.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
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

namespace
{
	// 文字サイズは 大(画面の区切り) / 中(手順の見出し) / 小(説明) の3段階
	TSharedRef<SWidget> MakeTitle(const TCHAR* Text)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(Text))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16));
	}

	TSharedRef<SWidget> MakeHeading(const TCHAR* Text)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(Text))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12));
	}

	TSharedRef<SWidget> MakeHint(const TCHAR* Text)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(Text))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true);
	}
}

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
	GlobalMasterDetailsView = PropertyModule.CreateDetailView(DetailsArgs);
	GlobalOutlineDetailsView = PropertyModule.CreateDetailView(DetailsArgs);

	// 全体設定: スロットごとに違うBaseColorテクスチャは一括の対象にしない
	GlobalMaster = NewObject<UToonMasterSettings>(GetTransientPackage());
	GlobalOutline = NewObject<UToonOutlineSettings>(GetTransientPackage());
	GlobalMasterDetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda(
		[](const FPropertyAndParent& PropertyAndParent)
		{
			return PropertyAndParent.Property.GetFName() != FName(TEXT("BaseColor"));
		}));
	GlobalMasterDetailsView->SetObject(GlobalMaster);
	GlobalOutlineDetailsView->SetObject(GlobalOutline);

	ThumbnailPool = MakeShared<FAssetThumbnailPool>(64);

	// スライダー操作中も含めて、設定値の変更をプレビューへ即反映する
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &SToonConverterWidget::OnObjectPropertyChanged);

	ChildSlot
	[
		SNew(SSplitter)

		// ===== 左: 3Dプレビュー =====
		+ SSplitter::Slot().Value(0.34f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().Padding(8, 8, 8, 0)
			[
				MakeTitle(TEXT("プレビュー"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(8, 2, 8, 4)
			[
				MakeHint(TEXT("左ドラッグ:回転 / 右ドラッグ:ズーム / 中ドラッグ:パン"))
			]

			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SAssignNew(Viewport, SToonPreviewViewport)
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(8, 8, 8, 0)
			[
				MakeHeading(TEXT("ライトの向き"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Pitch(上下)")))
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
					SNew(STextBlock).Text(FText::FromString(TEXT("Yaw(左右)")))
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

			+ SVerticalBox::Slot().AutoHeight().Padding(8, 4, 8, 0)
			[
				MakeHeading(TEXT("カメラの速さ"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(8, 2, 8, 0)
			[
				MakeHint(TEXT("回転・パン・ズーム(ホイール含む)の速さです。1.0が標準。"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(8, 4, 8, 8)
			[
				SNew(SSpinBox<float>)
				.MinValue(0.1f).MaxValue(5.f)
				.MinSliderValue(0.1f).MaxSliderValue(5.f)
				.Delta(0.1f)
				.Value_Lambda([this]() { return CameraSpeed; })
				.OnValueChanged_Lambda([this](float V)
				{
					CameraSpeed = V;
					Viewport->SetCameraSpeed(CameraSpeed);
				})
			]
		]

		// ===== 中: 手順 1〜4 =====
		+ SSplitter::Slot().Value(0.31f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().Padding(8, 8, 8, 4)
			[
				MakeTitle(TEXT("トゥーン変換"))
			]

			// 手順1: スケルタルメッシュ(未選択の間はオレンジ色で目立たせる)
			+ SVerticalBox::Slot().AutoHeight().Padding(8, 0, 8, 8)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor_Lambda([this]()
				{
					return Mesh.IsValid() ? FLinearColor::White : FLinearColor(1.f, 0.55f, 0.15f, 1.f);
				})
				.Padding(10)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot().AutoHeight()
					[
						MakeHeading(TEXT("手順1  スケルタルメッシュを選ぶ(必須)"))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 6)
					[
						MakeHint(TEXT("変換したいキャラのスケルタルメッシュを、下の欄へコンテンツブラウザからドラッグ&ドロップするか、欄をクリックして選びます。"))
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SObjectPropertyEntryBox)
						.AllowedClass(USkeletalMesh::StaticClass())
						.ObjectPath(this, &SToonConverterWidget::GetMeshPath)
						.OnObjectChanged(this, &SToonConverterWidget::OnMeshChanged)
						.AllowClear(true)
						.DisplayUseSelected(true)
						.DisplayBrowse(true)
						.DisplayThumbnail(true)
						.ThumbnailPool(ThumbnailPool)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("まだ選ばれていません。ここを選ぶと、手順2以降が使えるようになります。")))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						.ColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.7f, 0.3f, 1.f)))
						.AutoWrapText(true)
						.Visibility_Lambda([this]() { return Mesh.IsValid() ? EVisibility::Collapsed : EVisibility::Visible; })
					]
				]
			]

			// 手順2: スロット
			+ SVerticalBox::Slot().AutoHeight().Padding(8, 0)
			[
				MakeHeading(TEXT("手順2  スロットを設定する"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(8, 2, 8, 0)
			[
				MakeHint(TEXT("顔のスロットは「顔」、アウトラインを付けたいスロットは「線」にチェックします。行をクリックすると、右で詳細を調整できます。"))
			]
			+ SVerticalBox::Slot().FillHeight(0.45f).Padding(8)
			[
				SAssignNew(ListView, SListView<TSharedPtr<FToonSlotEntry>>)
				.ListItemsSource(&Entries)
				.OnGenerateRow(this, &SToonConverterWidget::OnGenerateRow)
				.OnSelectionChanged(this, &SToonConverterWidget::OnSlotSelected)
				.SelectionMode(ESelectionMode::Single)
			]

			// 手順3: 全体設定(タブで MM_ToonShader / M_ToonOutline を切り替え)
			+ SVerticalBox::Slot().AutoHeight().Padding(8, 0)
			[
				MakeHeading(TEXT("手順3  全体設定(任意)"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(8, 2, 8, 0)
			[
				MakeHint(TEXT("ここを変えると、全スロットへ一括で反映されます。スロットごとの微調整は右の詳細で行います。"))
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(8, 4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SCheckBox)
					.Style(&FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("ToggleButtonCheckbox"))
					.IsChecked_Lambda([this]() { return GlobalTab == 0 ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState) { GlobalTab = 0; })
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("MM_ToonShader"))).Justification(ETextJustify::Center)
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SCheckBox)
					.Style(&FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("ToggleButtonCheckbox"))
					.IsChecked_Lambda([this]() { return GlobalTab == 1 ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState) { GlobalTab = 1; })
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("M_ToonOutline"))).Justification(ETextJustify::Center)
					]
				]
			]

			+ SVerticalBox::Slot().FillHeight(0.55f).Padding(8, 0)
			[
				SNew(SWidgetSwitcher)
				.WidgetIndex_Lambda([this]() { return GlobalTab; })
				+ SWidgetSwitcher::Slot()
				[
					GlobalMasterDetailsView.ToSharedRef()
				]
				+ SWidgetSwitcher::Slot()
				[
					GlobalOutlineDetailsView.ToSharedRef()
				]
			]

			// 手順4: 実行
			+ SVerticalBox::Slot().AutoHeight().Padding(8, 8, 8, 0)
			[
				MakeHeading(TEXT("手順4  変換して適用する"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(8, 2, 8, 0)
			[
				MakeHint(TEXT("メッシュのマテリアルがトゥーン用のMIに置き換わります。「元に戻す」で変換前のマテリアルへ戻せます。"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(8, 4, 8, 0)
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
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(2.f).Padding(0, 0, 4, 0)
				[
					SNew(SButton)
					.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
					.HAlign(HAlign_Center)
					.ContentPadding(FMargin(8.f))
					.IsEnabled_Lambda([this]() { return Mesh.IsValid(); })
					.OnClicked(this, &SToonConverterWidget::OnConvertClicked)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("変換して適用")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4, 0, 0, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FMargin(8.f))
					.ToolTipText(FText::FromString(TEXT("メッシュのマテリアルを、変換前のマテリアルへ戻します")))
					.IsEnabled_Lambda([this]() { return Mesh.IsValid(); })
					.OnClicked(this, &SToonConverterWidget::OnRestoreClicked)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("元に戻す")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
					]
				]
			]
		]

		// ===== 右: 選択スロットの詳細 =====
		+ SSplitter::Slot().Value(0.35f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().Padding(8, 8, 8, 0)
			[
				MakeTitle(TEXT("スロットの詳細"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(8, 2, 8, 4)
			[
				MakeHint(TEXT("中央の一覧で選んだスロットだけの設定です。"))
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(8, 0, 8, 4)
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.Text_Lambda([this]()
				{
					if (!SelectedEntry.IsValid())
					{
						return FText::FromString(TEXT("(スロットが選ばれていません)"));
					}
					return FText::FromString(FString::Printf(TEXT("[%d] %s"),
						SelectedEntry->Index, *SelectedEntry->SlotName.ToString()));
				})
			]

			+ SVerticalBox::Slot().FillHeight(1.f).Padding(8, 0)
			[
				DetailsView.ToSharedRef()
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(8, 8, 8, 0)
			[
				MakeHeading(TEXT("アウトライン"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(8, 2, 8, 4)
			[
				MakeHint(TEXT("このスロットの「線」にチェックしたときだけ有効になります。"))
			]

			+ SVerticalBox::Slot().FillHeight(0.7f).Padding(8, 0, 8, 8)
			[
				OutlineDetailsView.ToSharedRef()
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
	Collector.AddReferencedObject(GlobalMaster);
	Collector.AddReferencedObject(GlobalOutline);
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
	RefreshAll();
}

void SToonConverterWidget::RefreshAll()
{
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
	if (Entries.Num() == 0 || !GlobalMaster || !GlobalOutline)
	{
		return;
	}

	// 全体設定の表示は先頭スロットの値に合わせる(BaseColorは対象外)
	const TSharedPtr<FToonSlotEntry>& First = Entries[0];
	ToonParamUtil::CopyValues(GlobalMaster, First->MasterSettings, FName(TEXT("BaseColor")));
	ToonParamUtil::CopyValues(GlobalOutline, First->OutlineSettings);
	GlobalMasterDetailsView->ForceRefresh();
	GlobalOutlineDetailsView->ForceRefresh();
}

void SToonConverterWidget::ApplyGlobalChange(UObject* GlobalObject, FName PropertyName)
{
	const FName SkipName(TEXT("BaseColor"));
	if (PropertyName == SkipName)
	{
		return;
	}

	for (const TSharedPtr<FToonSlotEntry>& E : Entries)
	{
		if (!E.IsValid())
		{
			continue;
		}

		if (GlobalObject == GlobalMaster.Get())
		{
			if (PropertyName.IsNone())
			{
				ToonParamUtil::CopyValues(E->MasterSettings, GlobalMaster, SkipName);
			}
			else
			{
				ToonParamUtil::CopyProperty(E->MasterSettings, GlobalMaster, PropertyName);
			}
			if (E->PreviewMID)
			{
				ToonParamUtil::ApplyToMID(E->GetActiveSettings(), E->PreviewMID);
			}
		}
		else
		{
			if (PropertyName.IsNone())
			{
				ToonParamUtil::CopyValues(E->OutlineSettings, GlobalOutline);
			}
			else
			{
				ToonParamUtil::CopyProperty(E->OutlineSettings, GlobalOutline, PropertyName);
			}
			if (E->OutlinePreviewMID)
			{
				ToonParamUtil::ApplyToMID(E->OutlineSettings, E->OutlinePreviewMID);
			}
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

	if (Object == GlobalMaster.Get() || Object == GlobalOutline.Get())
	{
		// 構造体(色)の中の1チャンネルを触った場合は、親のメンバー名で扱う
		const FProperty* P = Event.MemberProperty ? Event.MemberProperty : Event.Property;
		ApplyGlobalChange(Object, P ? P->GetFName() : NAME_None);
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

FReply SToonConverterWidget::OnRestoreClicked()
{
	USkeletalMesh* M = Mesh.Get();
	if (!M)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("スケルタルメッシュを選択してください。")));
		return FReply::Handled();
	}

	if (!FToonConverter::HasBackup(M))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(
			TEXT("元のマテリアルの記録がありません。\nこのツールで変換したときに自動で記録されます(Saved/ToonShaderMaster/Backup)。")));
		return FReply::Handled();
	}

	const EAppReturnType::Type Answer = FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(
		TEXT("メッシュのマテリアルを、変換前のマテリアルへ戻します。\n(作成済みのMIやBP Actorは削除されません)\nよろしいですか？")));
	if (Answer != EAppReturnType::Yes)
	{
		return FReply::Handled();
	}

	TArray<FString> Errors;
	const int32 Restored = FToonConverter::RestoreOriginalMaterials(M, Errors);

	// 戻した状態でプレビューとスロット一覧を作り直す
	RefreshAll();

	FString Msg = FString::Printf(TEXT("元のマテリアルへ戻したスロット: %d"), Restored);
	for (const FString& Err : Errors)
	{
		Msg += TEXT("\n") + Err;
	}
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Msg));

	return FReply::Handled();
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
