#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInstanceConstant.h"
#include "ToonSlotSettings.h"

/** マテリアルスロット1つ分の編集状態(データだけ持つ。作り方は ToonSlotInitializer / ToonPreviewMaterial の担当) */
struct FToonSlotEntry
{
	int32 Index = 0;
	FName SlotName;
	FString MaterialName;
	bool bFace = false;
	/** このスロットにアウトラインを付けるか */
	bool bOutline = false;

	// 顔/顔以外のどちらに切り替えても値を失わないよう、両方持つ
	TObjectPtr<UToonMasterSettings> MasterSettings = nullptr;
	TObjectPtr<UToonFaceSettings> FaceSettings = nullptr;

	TObjectPtr<UToonOutlineSettings> OutlineSettings = nullptr;

	/** 3Dプレビュー用のマテリアル */
	TObjectPtr<UMaterialInstanceDynamic> PreviewMID = nullptr;
	/** Static Switch をプレビューへ反映するための一時MIC(PreviewMIDの親になる。不要なら使わない) */
	TObjectPtr<UMaterialInstanceConstant> PreviewStaticMIC = nullptr;
	TObjectPtr<UMaterialInstanceDynamic> OutlinePreviewMID = nullptr;

	UObject* GetActiveSettings() const
	{
		return bFace ? static_cast<UObject*>(FaceSettings.Get()) : static_cast<UObject*>(MasterSettings.Get());
	}
};
