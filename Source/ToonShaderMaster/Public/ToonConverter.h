#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;
class UMaterialInterface;
class UTexture;

struct FToonConvertResult
{
	int32 Created = 0;
	int32 Reused = 0;
	TArray<FString> Errors;
};

/** スロットごとの変換指定 */
struct FToonConvertSlotInput
{
	bool bFace = false;
	/** UToonMasterSettings または UToonFaceSettings(bFaceに対応する方)。nullなら値は書かない */
	UObject* Settings = nullptr;
};

class FToonConverter
{
public:
	/** bFace=true なら M_ToonFaceShader、false なら MM_ToonShader をロードして返す */
	static UMaterialInterface* GetMaster(bool bFace);

	/** 元マテリアルからBaseColor用のテクスチャを探す(WhiteSquareTexture等) */
	static UTexture* FindSourceTexture(UMaterialInterface* Orig);

	/**
	 * Mesh の全スロットをトゥーンMIへ変換して適用する。
	 * Slots[i] がスロットiの指定(顔か・調整値)。足りない分は顔でない・値なしとして扱う。
	 */
	static bool Convert(USkeletalMesh* Mesh, const TArray<FToonConvertSlotInput>& Slots, FToonConvertResult& OutResult);
};
