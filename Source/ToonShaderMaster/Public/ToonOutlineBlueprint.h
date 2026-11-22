#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;
class UMaterialInterface;
class UBlueprint;

/** アウトライン用のBP Actorを作る */
namespace ToonOutlineBlueprint
{
	/**
	 * BP_<メッシュ名>_Toon を作る(メッシュと同じフォルダ)。
	 *   ToonMesh(ルート: トゥーンのスケルタルメッシュ)
	 *    └ OutlineMesh(同じメッシュ。OutlineMats を各スロットに設定。影なし)
	 * コンストラクションスクリプトで OutlineMesh の Leader Pose に ToonMesh を設定する。
	 * 同名のBPが既にある場合は作り直さず nullptr を返す(OutNote に理由)。
	 */
	UBlueprint* Create(USkeletalMesh* Mesh, const TArray<UMaterialInterface*>& OutlineMats, FString& OutNote);
}
