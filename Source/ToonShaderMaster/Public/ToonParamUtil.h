#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMaterialInstanceConstant;

/** 設定オブジェクト(UToonMasterSettings / UToonFaceSettings)とマテリアルの値をやりとりする。プロパティ名=パラメータ名 */
namespace ToonParamUtil
{
	/** 全プロパティをMID(プレビュー用)へ反映する */
	void ApplyToMID(UObject* Settings, UMaterialInstanceDynamic* MID);

	/** MIへ書き込む。bWriteAll=false の間はクラスのデフォルト値と異なるものだけ書く(親の値を継承させる) */
	void ApplyToMI(UObject* Settings, UMaterialInstanceConstant* MI, bool bWriteAll);

	/** 同じクラスの設定オブジェクト同士で値をコピーする。SkipName のプロパティは除く */
	void CopyValues(UObject* Dst, const UObject* Src, FName SkipName = NAME_None);

	/** 同じクラスの設定オブジェクト同士で、指定した1プロパティだけコピーする */
	void CopyProperty(UObject* Dst, const UObject* Src, FName PropertyName);

	/** 既存マテリアルの値を設定オブジェクトへ読み込む */
	void LoadFromMaterial(UObject* Settings, UMaterialInterface* Mat);
}
