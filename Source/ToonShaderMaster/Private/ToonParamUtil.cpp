#include "ToonParamUtil.h"

#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/UnrealType.h"
#include "MaterialEditingLibrary.h"

namespace
{
	bool IsLinearColor(const FProperty* P)
	{
		const FStructProperty* S = CastField<FStructProperty>(P);
		return S && S->Struct == TBaseStructure<FLinearColor>::Get();
	}

	/** マテリアルのパラメータ名。meta=(ToonParam="...") があればそれ、なければプロパティ名 */
	FName ParamNameOf(const FProperty* P)
	{
		if (P->HasMetaData(TEXT("ToonParam")))
		{
			return FName(*P->GetMetaData(TEXT("ToonParam")));
		}
		return P->GetFName();
	}
}

void ToonParamUtil::ApplyToMID(UObject* Settings, UMaterialInstanceDynamic* MID)
{
	if (!Settings || !MID)
	{
		return;
	}

	// 一旦リセットしてから全て設定し直す(テクスチャを外したときにマスターのデフォルトへ戻す)
	MID->ClearParameterValues();

	for (TFieldIterator<FProperty> It(Settings->GetClass()); It; ++It)
	{
		FProperty* P = *It;
		const FName Name = ParamNameOf(P);

		// Static Switch(bool)はMIDでは変えられないので、MI生成時にだけ反映する
		if (const FFloatProperty* F = CastField<FFloatProperty>(P))
		{
			MID->SetScalarParameterValue(Name, F->GetPropertyValue_InContainer(Settings));
		}
		else if (IsLinearColor(P))
		{
			MID->SetVectorParameterValue(Name, *P->ContainerPtrToValuePtr<FLinearColor>(Settings));
		}
		else if (const FObjectProperty* O = CastField<FObjectProperty>(P))
		{
			if (UTexture* Tex = Cast<UTexture>(O->GetObjectPropertyValue_InContainer(Settings)))
			{
				MID->SetTextureParameterValue(Name, Tex);
			}
		}
	}
}

bool ToonParamUtil::HasNonDefaultStaticSwitch(const UObject* Settings)
{
	if (!Settings)
	{
		return false;
	}

	const UObject* CDO = Settings->GetClass()->GetDefaultObject();
	for (TFieldIterator<FBoolProperty> It(Settings->GetClass()); It; ++It)
	{
		if (!It->Identical_InContainer(Settings, CDO))
		{
			return true;
		}
	}
	return false;
}

void ToonParamUtil::ApplyStaticSwitches(UObject* Settings, UMaterialInstanceConstant* MI)
{
	if (!Settings || !MI)
	{
		return;
	}

	for (TFieldIterator<FBoolProperty> It(Settings->GetClass()); It; ++It)
	{
		UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(
			MI, ParamNameOf(*It), It->GetPropertyValue_InContainer(Settings));
	}
}

void ToonParamUtil::ApplyToMI(UObject* Settings, UMaterialInstanceConstant* MI, bool bWriteAll)
{
	if (!Settings || !MI)
	{
		return;
	}

	const UObject* CDO = Settings->GetClass()->GetDefaultObject();

	for (TFieldIterator<FProperty> It(Settings->GetClass()); It; ++It)
	{
		FProperty* P = *It;
		if (!bWriteAll && P->Identical_InContainer(Settings, CDO))
		{
			continue;
		}

		const FMaterialParameterInfo Info(ParamNameOf(P));

		if (const FBoolProperty* B = CastField<FBoolProperty>(P))
		{
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(
				MI, ParamNameOf(P), B->GetPropertyValue_InContainer(Settings));
		}
		else if (const FFloatProperty* F = CastField<FFloatProperty>(P))
		{
			MI->SetScalarParameterValueEditorOnly(Info, F->GetPropertyValue_InContainer(Settings));
		}
		else if (IsLinearColor(P))
		{
			MI->SetVectorParameterValueEditorOnly(Info, *P->ContainerPtrToValuePtr<FLinearColor>(Settings));
		}
		else if (const FObjectProperty* O = CastField<FObjectProperty>(P))
		{
			if (UTexture* Tex = Cast<UTexture>(O->GetObjectPropertyValue_InContainer(Settings)))
			{
				// 親マスターと同じテクスチャなら書かない(マスター側の差し替えに追従させる)
				UTexture* ParentTex = nullptr;
				if (!bWriteAll && MI->Parent
					&& MI->Parent->GetTextureParameterValue(FHashedMaterialParameterInfo(ParamNameOf(P)), ParentTex)
					&& ParentTex == Tex)
				{
					continue;
				}
				MI->SetTextureParameterValueEditorOnly(Info, Tex);
			}
		}
	}
}

void ToonParamUtil::LoadFromMaterial(UObject* Settings, UMaterialInterface* Mat)
{
	if (!Settings || !Mat)
	{
		return;
	}

	for (TFieldIterator<FProperty> It(Settings->GetClass()); It; ++It)
	{
		FProperty* P = *It;
		const FHashedMaterialParameterInfo Info(ParamNameOf(P));

		if (const FBoolProperty* B = CastField<FBoolProperty>(P))
		{
			// Static Switchは設定済みのMIからだけ読む(マスター単体ならクラスのデフォルトのまま)
			if (const UMaterialInstanceConstant* MIns = Cast<UMaterialInstanceConstant>(Mat))
			{
				B->SetPropertyValue_InContainer(Settings,
					UMaterialEditingLibrary::GetMaterialInstanceStaticSwitchParameterValue(
						const_cast<UMaterialInstanceConstant*>(MIns), ParamNameOf(P)));
			}
		}
		else if (const FFloatProperty* F = CastField<FFloatProperty>(P))
		{
			float V = 0.f;
			if (Mat->GetScalarParameterValue(Info, V))
			{
				F->SetPropertyValue_InContainer(Settings, V);
			}
		}
		else if (IsLinearColor(P))
		{
			FLinearColor C;
			if (Mat->GetVectorParameterValue(Info, C))
			{
				*P->ContainerPtrToValuePtr<FLinearColor>(Settings) = C;
			}
		}
		else if (const FObjectProperty* O = CastField<FObjectProperty>(P))
		{
			UTexture* T = nullptr;
			if (Mat->GetTextureParameterValue(Info, T))
			{
				if (UTexture2D* T2 = Cast<UTexture2D>(T))
				{
					O->SetObjectPropertyValue_InContainer(Settings, T2);
				}
			}
		}
	}
}

void ToonParamUtil::CopyValues(UObject* Dst, const UObject* Src, FName SkipName)
{
	if (!Dst || !Src || Dst->GetClass() != Src->GetClass())
	{
		return;
	}

	for (TFieldIterator<FProperty> It(Dst->GetClass()); It; ++It)
	{
		FProperty* P = *It;
		if (P->GetFName() == SkipName)
		{
			continue;
		}
		P->CopyCompleteValue_InContainer(Dst, Src);
	}
}

void ToonParamUtil::CopyProperty(UObject* Dst, const UObject* Src, FName PropertyName)
{
	if (!Dst || !Src || Dst->GetClass() != Src->GetClass())
	{
		return;
	}

	if (FProperty* P = Dst->GetClass()->FindPropertyByName(PropertyName))
	{
		P->CopyCompleteValue_InContainer(Dst, Src);
	}
}
