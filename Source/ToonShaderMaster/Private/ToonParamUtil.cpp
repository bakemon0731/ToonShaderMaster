#include "ToonParamUtil.h"

#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/UnrealType.h"

namespace
{
	bool IsLinearColor(const FProperty* P)
	{
		const FStructProperty* S = CastField<FStructProperty>(P);
		return S && S->Struct == TBaseStructure<FLinearColor>::Get();
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
		const FName Name = P->GetFName();

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

		const FMaterialParameterInfo Info(P->GetFName());

		if (const FFloatProperty* F = CastField<FFloatProperty>(P))
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
					&& MI->Parent->GetTextureParameterValue(FHashedMaterialParameterInfo(P->GetFName()), ParentTex)
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
		const FHashedMaterialParameterInfo Info(P->GetFName());

		if (const FFloatProperty* F = CastField<FFloatProperty>(P))
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
