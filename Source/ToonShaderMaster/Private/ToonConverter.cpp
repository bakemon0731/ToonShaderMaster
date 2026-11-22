#include "ToonConverter.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "ToonParamUtil.h"
#include "Materials/MaterialInstance.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	// プラグイン内に置いたマスターマテリアルのパス
	const TCHAR* MasterPath = TEXT("/ToonShaderMaster/Materials/MM_ToonShader.MM_ToonShader");
	const TCHAR* FaceMasterPath = TEXT("/ToonShaderMaster/Materials/M_ToonFaceShader.M_ToonFaceShader");
	const TCHAR* OutlineMasterPath = TEXT("/ToonShaderMaster/Materials/M_ToonOutline.M_ToonOutline");
	const TCHAR* EmptyMaterialPath = TEXT("/ToonShaderMaster/Materials/M_Empty.M_Empty");

	// 両マスター共通のテクスチャパラメータ名
	const FName BaseColorParam(TEXT("BaseColor"));

	// ---- 変換前マテリアルの記録(スロット名 \t オブジェクトパス の1行1スロット) ----
	FString BackupFilePath(USkeletalMesh* Mesh)
	{
		FString Name = Mesh->GetOutermost()->GetName();
		Name.ReplaceInline(TEXT("/"), TEXT("_"));
		return FPaths::ProjectSavedDir() / TEXT("ToonShaderMaster") / TEXT("Backup") / (Name + TEXT(".txt"));
	}

	TMap<FString, FString> LoadBackup(const FString& Path)
	{
		TMap<FString, FString> Map;
		FString Text;
		if (FFileHelper::LoadFileToString(Text, *Path))
		{
			TArray<FString> Lines;
			Text.ParseIntoArrayLines(Lines);
			for (const FString& Line : Lines)
			{
				FString Key, Value;
				if (Line.Split(TEXT("\t"), &Key, &Value))
				{
					Map.Add(Key, Value);
				}
			}
		}
		return Map;
	}

	bool IsToonMI(UMaterialInterface* M, UMaterialInterface* Master, UMaterialInterface* FaceMaster)
	{
		const UMaterialInstance* MI = Cast<UMaterialInstance>(M);
		return MI && (MI->Parent == Master || MI->Parent == FaceMaster);
	}

	/** 変換する前に、トゥーン化されていないスロットの現在のマテリアルを記録する(トゥーン化済みのスロットは記録を残す) */
	void SaveOriginalMaterials(USkeletalMesh* Mesh, UMaterialInterface* Master, UMaterialInterface* FaceMaster)
	{
		const FString Path = BackupFilePath(Mesh);
		TMap<FString, FString> Map = LoadBackup(Path);

		for (const FSkeletalMaterial& SM : Mesh->GetMaterials())
		{
			if (!IsToonMI(SM.MaterialInterface, Master, FaceMaster))
			{
				Map.Add(SM.MaterialSlotName.ToString(),
					SM.MaterialInterface ? SM.MaterialInterface->GetPathName() : FString(TEXT("None")));
			}
		}

		FString Out;
		for (const TPair<FString, FString>& KV : Map)
		{
			Out += KV.Key + TEXT("\t") + KV.Value + TEXT("\n");
		}
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		FFileHelper::SaveStringToFile(Out, *Path);
	}
}

UMaterialInterface* FToonConverter::GetMaster(bool bFace)
{
	return Cast<UMaterialInterface>(FSoftObjectPath(bFace ? FaceMasterPath : MasterPath).TryLoad());
}

bool FToonConverter::HasBackup(USkeletalMesh* Mesh)
{
	return Mesh && IFileManager::Get().FileExists(*BackupFilePath(Mesh));
}

int32 FToonConverter::RestoreOriginalMaterials(USkeletalMesh* Mesh, TArray<FString>& OutErrors)
{
	if (!Mesh)
	{
		return 0;
	}

	const TMap<FString, FString> Map = LoadBackup(BackupFilePath(Mesh));
	if (Map.Num() == 0)
	{
		return 0;
	}

	FScopedTransaction Transaction(FText::FromString(TEXT("Toon MI を元のマテリアルへ戻す")));
	Mesh->Modify();

	int32 Restored = 0;
	TArray<FSkeletalMaterial>& Mats = Mesh->GetMaterials();
	for (FSkeletalMaterial& SM : Mats)
	{
		const FString* Path = Map.Find(SM.MaterialSlotName.ToString());
		if (!Path)
		{
			continue;
		}

		UMaterialInterface* Orig = nullptr;
		if (*Path != TEXT("None"))
		{
			Orig = Cast<UMaterialInterface>(FSoftObjectPath(*Path).TryLoad());
			if (!Orig)
			{
				OutErrors.Add(FString::Printf(TEXT("元のマテリアルが見つかりません: %s (%s)"), **Path, *SM.MaterialSlotName.ToString()));
				continue;
			}
		}

		SM.MaterialInterface = Orig;
		++Restored;
	}

	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();
	return Restored;
}

UMaterialInterface* FToonConverter::GetOutlineMaster()
{
	return Cast<UMaterialInterface>(FSoftObjectPath(OutlineMasterPath).TryLoad());
}

UMaterialInterface* FToonConverter::GetEmptyMaterial()
{
	return Cast<UMaterialInterface>(FSoftObjectPath(EmptyMaterialPath).TryLoad());
}

FString FToonConverter::GetOutlineMIPackageName(USkeletalMesh* Mesh, FName SlotName)
{
	const FString Dir = FPackageName::GetLongPackagePath(Mesh->GetOutermost()->GetName()) / TEXT("Materials");
	const FString AssetName = ObjectTools::SanitizeObjectName(
		FString::Printf(TEXT("MI_%s_%s_Outline"), *Mesh->GetName(), *SlotName.ToString()));
	return Dir / AssetName;
}

UTexture* FToonConverter::FindSourceTexture(UMaterialInterface* Orig)
{
	if (!Orig)
	{
		return nullptr;
	}

	UTexture* Tex = nullptr;
	if (Orig->GetTextureParameterValue(FHashedMaterialParameterInfo(BaseColorParam), Tex) && Tex)
	{
		return Tex;
	}

	TArray<UTexture*> Used;
	Orig->GetUsedTextures(Used, EMaterialQualityLevel::Num, true, ERHIFeatureLevel::Num, true);
	for (UTexture* T : Used)
	{
		if (T)
		{
			return T;
		}
	}
	return nullptr;
}

bool FToonConverter::Convert(USkeletalMesh* Mesh, const TArray<FToonConvertSlotInput>& Slots, FToonConvertResult& OutResult)
{
	if (!Mesh)
	{
		OutResult.Errors.Add(TEXT("スケルタルメッシュが指定されていません。"));
		return false;
	}

	UMaterialInterface* Master = GetMaster(false);
	UMaterialInterface* FaceMaster = GetMaster(true);
	if (!Master)
	{
		OutResult.Errors.Add(FString::Printf(TEXT("マスターが見つかりません: %s"), MasterPath));
	}
	if (!FaceMaster)
	{
		OutResult.Errors.Add(FString::Printf(TEXT("顔用マスターが見つかりません: %s"), FaceMasterPath));
	}
	if (!Master || !FaceMaster)
	{
		return false;
	}

	// 変換前のマテリアルを記録(元に戻すボタン用)
	SaveOriginalMaterials(Mesh, Master, FaceMaster);

	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();

	// MIの保存先: メッシュと同じフォルダ/Materials
	const FString Dir = FPackageName::GetLongPackagePath(Mesh->GetOutermost()->GetName()) / TEXT("Materials");

	FScopedTransaction Transaction(FText::FromString(TEXT("Toon MI 変換")));
	Mesh->Modify();

	const int32 NumSlots = Mesh->GetMaterials().Num();
	for (int32 i = 0; i < NumSlots; ++i)
	{
		UMaterialInterface* Orig = Mesh->GetMaterials()[i].MaterialInterface;
		const FName SlotName = Mesh->GetMaterials()[i].MaterialSlotName;

		const FToonConvertSlotInput Input = Slots.IsValidIndex(i) ? Slots[i] : FToonConvertSlotInput();
		UMaterialInterface* Parent = Input.bFace ? FaceMaster : Master;

		const FString AssetName = ObjectTools::SanitizeObjectName(
			FString::Printf(TEXT("MI_%s_%s"), *Mesh->GetName(), *SlotName.ToString()));
		const FString PackageName = Dir / AssetName;

		UMaterialInstanceConstant* MI = nullptr;
		bool bReused = false;

		// 既存MIがあれば再利用して親だけ付け替える
		if (FPackageName::DoesPackageExist(PackageName))
		{
			MI = LoadObject<UMaterialInstanceConstant>(nullptr, *(PackageName + TEXT(".") + AssetName));
			if (MI)
			{
				MI->Modify();
				MI->SetParentEditorOnly(Parent);
				bReused = true;
				++OutResult.Reused;
			}
		}

		if (!MI)
		{
			UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
			Factory->InitialParent = Parent;
			MI = Cast<UMaterialInstanceConstant>(
				AssetTools.CreateAsset(AssetName, Dir, UMaterialInstanceConstant::StaticClass(), Factory));
			if (MI)
			{
				++OutResult.Created;
			}
		}

		if (!MI)
		{
			OutResult.Errors.Add(FString::Printf(TEXT("MIの作成に失敗: スロット[%d] %s"), i, *SlotName.ToString()));
			continue;
		}

		if (Input.Settings)
		{
			// UIで調整した値を反映。新規MIはデフォルトと違う値だけ書いて親を継承、再利用MIは全て上書き
			ToonParamUtil::ApplyToMI(Input.Settings, MI, bReused);
		}
		else if (UTexture* Tex = FindSourceTexture(Orig))
		{
			MI->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(BaseColorParam), Tex);
		}

		MI->PostEditChange();
		MI->MarkPackageDirty();

		// 自動適用（USkeletalMesh アセット側のスロットを書き換える）
		TArray<FSkeletalMaterial>& Materials = Mesh->GetMaterials();
		if (Materials.IsValidIndex(i))
		{
			Materials[i].MaterialInterface = MI;
		}
	}

	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();

	return OutResult.Errors.Num() == 0;
}

bool FToonConverter::ConvertOutline(USkeletalMesh* Mesh, const TArray<FToonOutlineSlotInput>& Slots, FToonConvertResult& OutResult)
{
	OutResult.OutlineMaterials.Reset();

	if (!Mesh)
	{
		OutResult.Errors.Add(TEXT("スケルタルメッシュが指定されていません。"));
		return false;
	}

	UMaterialInterface* OutlineMaster = GetOutlineMaster();
	if (!OutlineMaster)
	{
		OutResult.Errors.Add(FString::Printf(TEXT("アウトライン用マスターが見つかりません: %s"), OutlineMasterPath));
		return false;
	}

	UMaterialInterface* EmptyMaterial = GetEmptyMaterial();
	if (!EmptyMaterial)
	{
		OutResult.Errors.Add(FString::Printf(TEXT("空マテリアルが見つかりません: %s"), EmptyMaterialPath));
		return false;
	}

	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	const FString Dir = FPackageName::GetLongPackagePath(Mesh->GetOutermost()->GetName()) / TEXT("Materials");

	const int32 NumSlots = Mesh->GetMaterials().Num();
	for (int32 i = 0; i < NumSlots; ++i)
	{
		const FName SlotName = Mesh->GetMaterials()[i].MaterialSlotName;
		const FToonOutlineSlotInput Input = Slots.IsValidIndex(i) ? Slots[i] : FToonOutlineSlotInput();

		const FString PackageName = GetOutlineMIPackageName(Mesh, SlotName);
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);

		// チェックなし: M_Empty を当てる。以前作ったMIがあれば親を付け替えて「無効」を記録しておく
		if (!Input.bEnabled)
		{
			if (FPackageName::DoesPackageExist(PackageName))
			{
				if (UMaterialInstanceConstant* Old = LoadObject<UMaterialInstanceConstant>(nullptr, *(PackageName + TEXT(".") + AssetName)))
				{
					Old->Modify();
					Old->SetParentEditorOnly(EmptyMaterial);
					Old->PostEditChange();
					Old->MarkPackageDirty();
				}
			}
			OutResult.OutlineMaterials.Add(EmptyMaterial);
			continue;
		}

		UMaterialInstanceConstant* MI = nullptr;
		bool bReused = false;

		if (FPackageName::DoesPackageExist(PackageName))
		{
			MI = LoadObject<UMaterialInstanceConstant>(nullptr, *(PackageName + TEXT(".") + AssetName));
			if (MI)
			{
				MI->Modify();
				MI->SetParentEditorOnly(OutlineMaster);
				bReused = true;
				++OutResult.Reused;
			}
		}

		if (!MI)
		{
			UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
			Factory->InitialParent = OutlineMaster;
			MI = Cast<UMaterialInstanceConstant>(
				AssetTools.CreateAsset(AssetName, Dir, UMaterialInstanceConstant::StaticClass(), Factory));
			if (MI)
			{
				++OutResult.Created;
			}
		}

		if (!MI)
		{
			OutResult.Errors.Add(FString::Printf(TEXT("アウトラインMIの作成に失敗: スロット[%d] %s"), i, *SlotName.ToString()));
			OutResult.OutlineMaterials.Add(nullptr);
			continue;
		}

		if (Input.Settings)
		{
			ToonParamUtil::ApplyToMI(Input.Settings, MI, bReused);
		}

		MI->PostEditChange();
		MI->MarkPackageDirty();

		OutResult.OutlineMaterials.Add(MI);
	}

	return OutResult.Errors.Num() == 0;
}
