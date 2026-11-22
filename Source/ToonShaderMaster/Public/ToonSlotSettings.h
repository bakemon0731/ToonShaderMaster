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
	FLinearColor ShadowColor = FLinearColor(1.f, 0.7f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Shadow")
	FLinearColor ShadowOuterColor = FLinearColor(1.f, 0.5f, 1.f, 1.f);

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

	/**
	 * FaceSDFのUVオフセット。影の形は変えず、位置だけ動かす。
	 * M_ToonFaceShader の同名スカラーパラメータに反映される(FaceSDFのUVs = TexCoord + (U, V))。
	 * UEはVが下向きなので、Vをプラスにすると影が上へ動く。マスター側のデフォルト値も揃えること。
	 */
	UPROPERTY(EditAnywhere, Category = "FaceShadow", meta = (UIMin = "-0.5", UIMax = "0.5"))
	float FaceShadowOffsetU = 0.0f;

	UPROPERTY(EditAnywhere, Category = "FaceShadow", meta = (UIMin = "-0.5", UIMax = "0.5"))
	float FaceShadowOffsetV = 0.05f;
};


// スロットごとの調整値(M_ToonOutline用)。
UCLASS(Transient)
class UToonOutlineSettings : public UObject
{
	GENERATED_BODY()

public:
	// ---- Outline ----
	UPROPERTY(EditAnywhere, Category = "Outline")
	FLinearColor OutlineColor = FLinearColor(0.f, 0.f, 0.f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Outline", meta = (UIMin = "0.0", UIMax = "1.0"))
	float OutlineThickness = 0.1f;

	// ---- Mask ----
	/** Static Switch: UseMask? (マスターのデフォルトはfalse) */
	UPROPERTY(EditAnywhere, Category = "Mask", meta = (ToonParam = "UseTextureMask"))
	bool bUseMask = false;

	/** Static Switch: True=テクスチャマスク / False=頂点カラーマスク(マスターのデフォルトはFalse) */
	UPROPERTY(EditAnywhere, Category = "Mask", meta = (ToonParam = "True_UseTextureMaskOrFalse_UseVertexColorMask"))
	bool bUseTextureMask = false;

	UPROPERTY(EditAnywhere, Category = "Mask")
	TObjectPtr<UTexture2D> OutLineMaskTexture = nullptr;

	/** マスクに使うチャンネル(R=赤, G=緑 ...)。マスターのMaskChannelパラメータ */
	UPROPERTY(EditAnywhere, Category = "Mask")
	FLinearColor MaskChannel = FLinearColor(1.f, 0.f, 0.f, 0.f);
};
