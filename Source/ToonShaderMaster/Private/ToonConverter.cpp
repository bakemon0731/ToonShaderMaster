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

namespace
{
	// プラグイン内に置いたマスターマテリアルのパス
	const TCHAR* MasterPath = TEXT("/ToonShaderMaster/Materials/MM_ToonShader.MM_ToonShader");
	const TCHAR* FaceMasterPath = TEXT("/ToonShaderMaster/Materials/M_ToonFaceShader.M_ToonFaceShader");

	// 両マスター共通のテクスチャパラメータ名
	const FName BaseColorParam(TEXT("BaseColor"));

	/** 元マテリアルからBaseColor用のテクスチャを探す(WhiteSquareTexture等) */
	UTexture* FindSourceTexture(UMaterialInterface* Orig)
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
}

bool FToonConverter::Convert(USkeletalMesh* Mesh, const TSet<int32>& FaceSlots, FToonConvertResult& OutResult)
{
	if (!Mesh)
	{
		OutResult.Errors.Add(TEXT("スケルタルメッシュが指定されていません。"));
		return false;
	}

	UMaterialInterface* Master = Cast<UMaterialInterface>(FSoftObjectPath(MasterPath).TryLoad());
	UMaterialInterface* FaceMaster = Cast<UMaterialInterface>(FSoftObjectPath(FaceMasterPath).TryLoad());
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
		const bool bFace = FaceSlots.Contains(i);
		UMaterialInterface* Parent = bFace ? FaceMaster : Master;

		const FString AssetName = ObjectTools::SanitizeObjectName(
			FString::Printf(TEXT("MI_%s_%s"), *Mesh->GetName(), *SlotName.ToString()));
		const FString PackageName = Dir / AssetName;

		UMaterialInstanceConstant* MI = nullptr;

		// 既存MIがあれば再利用して親だけ付け替える
		if (FPackageName::DoesPackageExist(PackageName))
		{
			MI = LoadObject<UMaterialInstanceConstant>(nullptr, *(PackageName + TEXT(".") + AssetName));
			if (MI)
			{
				MI->Modify();
				MI->SetParentEditorOnly(Parent);
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

		// BaseColorテクスチャだけ上書き。その他のパラメータはマスターのデフォルトを継承する
		if (UTexture* Tex = FindSourceTexture(Orig))
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
