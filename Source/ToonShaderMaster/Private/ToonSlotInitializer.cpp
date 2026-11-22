#include "ToonSlotInitializer.h"

#include "ToonSlotEntry.h"
#include "ToonConverter.h"
#include "ToonParamUtil.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"

FToonMasters FToonMasters::Load()
{
	FToonMasters M;
	M.Master = FToonConverter::GetMaster(false);
	M.Face = FToonConverter::GetMaster(true);
	M.Outline = FToonConverter::GetOutlineMaster();
	return M;
}

namespace
{
	/** アウトライン: マスターの値を初期値にし、作成済みのアウトラインMIがあればその値と有効状態を読み戻す */
	void InitOutline(FToonSlotEntry& E, USkeletalMesh* Mesh, const FToonMasters& Masters)
	{
		if (Masters.Outline)
		{
			ToonParamUtil::LoadFromMaterial(E.OutlineSettings, Masters.Outline);
		}

		// 作成済みのアウトラインMIがあり、親が M_ToonOutline ならチェック済みとして値を読み戻す
		// (チェックを外したときは親が M_Empty に付け替えられている)
		const FString OutlinePkg = FToonConverter::GetOutlineMIPackageName(Mesh, E.SlotName);
		if (FPackageName::DoesPackageExist(OutlinePkg))
		{
			const FString OutlineObj = OutlinePkg + TEXT(".") + FPackageName::GetLongPackageAssetName(OutlinePkg);
			if (UMaterialInstanceConstant* OMI = LoadObject<UMaterialInstanceConstant>(nullptr, *OutlineObj))
			{
				ToonParamUtil::LoadFromMaterial(E.OutlineSettings, OMI);
				E.bOutline = (Masters.Outline && OMI->Parent == Masters.Outline);
			}
		}
	}

	/** FaceSDF は元マテリアルに無いので、顔用マスターの既定テクスチャを初期値にする */
	void InitFaceSDFDefault(FToonSlotEntry& E, const FToonMasters& Masters)
	{
		if (!Masters.Face)
		{
			return;
		}
		UTexture* DefaultSDF = nullptr;
		if (Masters.Face->GetTextureParameterValue(FHashedMaterialParameterInfo(FName(TEXT("FaceSDF"))), DefaultSDF))
		{
			E.FaceSettings->FaceSDF = Cast<UTexture2D>(DefaultSDF);
		}
	}

	/** すでにトゥーンMIが適用済みなら、顔かどうかと調整値を読み戻す */
	void InitFromAppliedToonMI(FToonSlotEntry& E, UMaterialInterface* Orig, const FToonMasters& Masters)
	{
		const UMaterialInstance* MI = Cast<UMaterialInstance>(Orig);
		if (!MI)
		{
			return;
		}
		if (Masters.Face && MI->Parent == Masters.Face)
		{
			E.bFace = true;
			ToonParamUtil::LoadFromMaterial(E.FaceSettings, Orig);
		}
		else if (Masters.Master && MI->Parent == Masters.Master)
		{
			ToonParamUtil::LoadFromMaterial(E.MasterSettings, Orig);
		}
	}
}

void ToonSlotInitializer::Initialize(FToonSlotEntry& E, USkeletalMesh* Mesh, int32 SlotIndex, const FToonMasters& Masters)
{
	const FSkeletalMaterial& Slot = Mesh->GetMaterials()[SlotIndex];
	UMaterialInterface* Orig = Slot.MaterialInterface;

	E.Index = SlotIndex;
	E.SlotName = Slot.MaterialSlotName;
	E.MaterialName = Orig ? Orig->GetName() : TEXT("None");
	E.MasterSettings = NewObject<UToonMasterSettings>(GetTransientPackage());
	E.FaceSettings = NewObject<UToonFaceSettings>(GetTransientPackage());
	E.OutlineSettings = NewObject<UToonOutlineSettings>(GetTransientPackage());

	InitOutline(E, Mesh, Masters);
	InitFaceSDFDefault(E, Masters);

	// 元マテリアルのテクスチャを初期値にする(BaseColorはマスターの値より優先)
	if (UTexture2D* Src = Cast<UTexture2D>(FToonConverter::FindSourceTexture(Orig)))
	{
		E.MasterSettings->BaseColor = Src;
		E.FaceSettings->BaseColor = Src;
	}

	InitFromAppliedToonMI(E, Orig, Masters);
}
