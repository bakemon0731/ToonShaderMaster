#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/Texture2D.h"
#include "ToonSlotSettings.generated.h"

/**
 * スロットごとの調整値(MM_ToonShader用)。
 * プロパティ名 = マテリアルのパラメータ名。ToonParamUtil がリフレクションで名前一致で反映する。
 * パラメータを増やすときは、ここにUPROPERTYを足すだけでUI・プレビュー・MI生成に反映される。
 */
UCLASS(Transient)
class UToonMasterSettings : public UObject
{
	GENERATED_BODY()

public:
	// ---- Highlight ----
	UPROPERTY(EditAnywhere, Category = "Highlight", meta = (UIMin = "0.0", UIMax = "1.0"))
	float HightlightStartRange = 0.7f;

	UPROPERTY(EditAnywhere, Category = "Highlight", meta = (UIMin = "0.0", UIMax = "1.0"))
	float HightlightSmoothness = 0.02f;

	UPROPERTY(EditAnywhere, Category = "Highlight")
	FLinearColor HIghlightColor = FLinearColor(5.f, 5.f, 5.f, 1.f);

	// ---- RimLight ----
	UPROPERTY(EditAnywhere, Category = "RimLight", meta = (UIMin = "0.0", UIMax = "1.0"))
	float RimLightSmoothness = 0.45f;

	UPROPERTY(EditAnywhere, Category = "RimLight", meta = (UIMin = "0.0", UIMax = "1.0"))
	float RimLightStartRange = 0.7f;

	UPROPERTY(EditAnywhere, Category = "RimLight", meta = (UIMin = "0.0", UIMax = "5.0"))
	float RimlightRange = 2.0f;

	UPROPERTY(EditAnywhere, Category = "RimLight", meta = (UIMin = "0.0", UIMax = "1.0"))
	float RimBaseIntensity = 0.02f;

	UPROPERTY(EditAnywhere, Category = "RimLight")
	FLinearColor RimLightColor = FLinearColor(1.f, 1.f, 1.f, 1.f);

	// ---- Shadow ----
	UPROPERTY(EditAnywhere, Category = "Shadow", meta = (UIMin = "0.0", UIMax = "1.0"))
	float ShadowStartRange = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Shadow", meta = (UIMin = "0.0", UIMax = "1.0"))
	float ShadowSmoothness = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Shadow", meta = (UIMin = "0.0", UIMax = "1.0"))
	float OuterShadowStartRange = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Shadow", meta = (UIMin = "0.0", UIMax = "1.0"))
	float OuterShadowSmoothness = 0.01f;

	UPROPERTY(EditAnywhere, Category = "Shadow")
	FLinearColor ShadowColor = FLinearColor(0.f, 0.f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Shadow")
	FLinearColor ShadowOuterColor = FLinearColor(0.f, 1.f, 1.f, 1.f);

	// ---- Texture ----
	UPROPERTY(EditAnywhere, Category = "Texture")
	TObjectPtr<UTexture2D> BaseColor = nullptr;

	UPROPERTY(EditAnywhere, Category = "Texture", meta = (UIMin = "0.0", UIMax = "5.0"))
	float BaseColorPower = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Texture", meta = (UIMin = "0.0", UIMax = "5.0"))
	float BaseColorBrightness = 1.0f;
};

/** スロットごとの調整値(M_ToonFaceShader用) */
UCLASS(Transient)
class UToonFaceSettings : public UObject
{
	GENERATED_BODY()

public:
	// ---- Texture ----
	UPROPERTY(EditAnywhere, Category = "Texture")
	TObjectPtr<UTexture2D> BaseColor = nullptr;

	UPROPERTY(EditAnywhere, Category = "Texture")
	TObjectPtr<UTexture2D> FaceSDF = nullptr;

	UPROPERTY(EditAnywhere, Category = "Texture", meta = (UIMin = "0.0", UIMax = "5.0"))
	float BaseColorPower = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Texture", meta = (UIMin = "0.0", UIMax = "5.0"))
	float BaseColorBrightness = 1.0f;

	// ---- FaceShadow ----
	UPROPERTY(EditAnywhere, Category = "FaceShadow")
	FLinearColor FaceShadowColor = FLinearColor(1.f, 0.5f, 0.7f, 1.f);

	UPROPERTY(EditAnywhere, Category = "FaceShadow", meta = (UIMin = "0.0", UIMax = "1.0"))
	float FaceShadowStartRange = 0.2f;

	UPROPERTY(EditAnywhere, Category = "FaceShadow", meta = (UIMin = "0.0", UIMax = "1.0"))
	float FaceShadowSmoothness = 0.0f;
};
