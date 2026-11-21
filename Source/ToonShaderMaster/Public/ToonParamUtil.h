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

	/** 既存マテリアルの値を設定オブジェクトへ読み込む */
	void LoadFromMaterial(UObject* Settings, UMaterialInterface* Mat);
}
