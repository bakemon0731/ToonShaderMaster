#include "ToonPreviewMaterial.h"

#include "ToonParamUtil.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/UObjectGlobals.h"

UMaterialInstanceDynamic* ToonPreviewMaterial::Create(
	UMaterialInterface* Master,
	UObject* Settings,
	UObject* Outer,
	TObjectPtr<UMaterialInstanceConstant>& InOutStaticMIC)
{
	if (!Master || !Settings)
	{
		return nullptr;
	}

	UMaterialInterface* Parent = Master;

	if (ToonParamUtil::HasNonDefaultStaticSwitch(Settings))
	{
		if (!InOutStaticMIC)
		{
			InOutStaticMIC = NewObject<UMaterialInstanceConstant>(Outer);
		}
		// 顔/顔以外の切り替えでマスターが変わった場合に備えて毎回親を合わせる
		if (InOutStaticMIC->Parent != Master)
		{
			InOutStaticMIC->SetParentEditorOnly(Master);
		}
		ToonParamUtil::ApplyStaticSwitches(Settings, InOutStaticMIC);
		UMaterialEditingLibrary::UpdateMaterialInstance(InOutStaticMIC);
		Parent = InOutStaticMIC;
	}

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, Outer);
	ToonParamUtil::ApplyToMID(Settings, MID);
	return MID;
}
