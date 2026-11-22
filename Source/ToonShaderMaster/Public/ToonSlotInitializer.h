#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;
class UMaterialInterface;
struct FToonSlotEntry;

/** 変換に使うマスターマテリアル一式 */
struct FToonMasters
{
	UMaterialInterface* Master = nullptr;
	UMaterialInterface* Face = nullptr;
	UMaterialInterface* Outline = nullptr;

	static FToonMasters Load();
};

/**
 * スロット1つ分の編集状態(FToonSlotEntry)の初期値を作る。
 * 優先順位(後のものが前を上書き): クラスの既定値 → マスターの既定値 → 元マテリアルのテクスチャ → 変換済みMIの値
 */
namespace ToonSlotInitializer
{
	void Initialize(FToonSlotEntry& Entry, USkeletalMesh* Mesh, int32 SlotIndex, const FToonMasters& Masters);
}
