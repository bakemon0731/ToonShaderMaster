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

	/** ConvertOutline が作った/再利用したアウトラインMI(スロット順) */
	TArray<UMaterialInterface*> OutlineMaterials;
	/** 作成したBP Actorのパス。作らなかった/既存なら空 */
	FString BlueprintPath;
	/** BP生成に関する補足メッセージ(既存のため作り直さない等) */
	FString BlueprintNote;
};

/** スロットごとのアウトライン指定 */
struct FToonOutlineSlotInput
{
	bool bEnabled = false;
	/** UToonOutlineSettings。nullなら値は書かない */
	UObject* Settings = nullptr;
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

	/** アウトライン用マスター(M_ToonOutline)をロードして返す */
	static UMaterialInterface* GetOutlineMaster();

	/** アウトラインを使わないスロットに当てる空マテリアル(M_Empty)をロードして返す */
	static UMaterialInterface* GetEmptyMaterial();

	/** スロットのアウトラインMIのパッケージ名(メッシュと同じフォルダ/Materials) */
	static FString GetOutlineMIPackageName(USkeletalMesh* Mesh, FName SlotName);

	/**
	 * チェックしたスロットのアウトラインMIを作る。チェックしていないスロットは M_Empty を当てる
	 * (MIは作らない。以前作ったMIが残っていれば、親を M_Empty に付け替えて「無効」を記録する)。
	 * 結果は OutResult.OutlineMaterials(スロット順)に入る。
	 */
	static bool ConvertOutline(USkeletalMesh* Mesh, const TArray<FToonOutlineSlotInput>& Slots, FToonConvertResult& OutResult);

	/**
	 * Mesh の全スロットをトゥーンMIへ変換して適用する。
	 * Slots[i] がスロットiの指定(顔か・調整値)。足りない分は顔でない・値なしとして扱う。
	 */
	static bool Convert(USkeletalMesh* Mesh, const TArray<FToonConvertSlotInput>& Slots, FToonConvertResult& OutResult);
};
