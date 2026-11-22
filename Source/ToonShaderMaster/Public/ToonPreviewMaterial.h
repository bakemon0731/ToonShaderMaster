#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMaterialInstanceConstant;

/**
 * 3Dプレビュー用のマテリアルを作る。
 * Static Switch(bool)はMIDでは変えられないため、既定値と違うスイッチがあるときだけ、
 * スイッチを反映した一時MICを作ってそれをMIDの親にする。それ以外はマスター直下のMIDのまま。
 */
namespace ToonPreviewMaterial
{
	/**
	 * @param InOutStaticMIC  一時MICの置き場。スイッチが必要なときに作られ、使い回される(GC対象にすること)
	 */
	UMaterialInstanceDynamic* Create(
		UMaterialInterface* Master,
		UObject* Settings,
		UObject* Outer,
		TObjectPtr<UMaterialInstanceConstant>& InOutStaticMIC);
}
