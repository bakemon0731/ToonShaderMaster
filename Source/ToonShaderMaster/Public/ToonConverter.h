#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;

struct FToonConvertResult
{
	int32 Created = 0;
	int32 Reused = 0;
	TArray<FString> Errors;
};

class FToonConverter
{
public:
	/**
	 * Mesh の全マテリアルスロットをトゥーンMIへ変換して適用する。
	 * FaceSlots に含まれるスロットは M_ToonFaceShader、それ以外は MM_ToonShader が親になる。
	 */
	static bool Convert(USkeletalMesh* Mesh, const TSet<int32>& FaceSlots, FToonConvertResult& OutResult);
};
