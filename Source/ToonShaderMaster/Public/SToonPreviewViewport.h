#pragma once

#include "CoreMinimal.h"
#include "SEditorViewport.h"

class FPreviewScene;
class FToonPreviewViewportClient;
class USkeletalMesh;
class USkeletalMeshComponent;
class UMaterialInterface;

/** スケルタルメッシュを表示する3Dプレビュー(左ドラッグ:回転 / 右ドラッグ:ズーム / 中ドラッグ:パン) */
class SToonPreviewViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SToonPreviewViewport) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SToonPreviewViewport();

	/** 表示するメッシュを差し替える(nullで消す) */
	void SetMesh(USkeletalMesh* InMesh);

	/** プレビュー上のスロットのマテリアルだけを差し替える(アセットは変更しない) */
	void SetSlotMaterial(int32 SlotIndex, UMaterialInterface* Material);

	/** アウトライン用メッシュ(親のポーズに追従)のスロットのマテリアルを差し替える */
	void SetOutlineSlotMaterial(int32 SlotIndex, UMaterialInterface* Material);

	/** ライト(ディレクショナル)の向き */
	void SetLightRotation(float Pitch, float Yaw);

	/** カメラ操作(回転・パン・ズーム)の速さ。1.0が標準 */
	void SetCameraSpeed(float Speed);

	/** メッシュ全体が収まる位置へカメラを戻す */
	void FrameMesh();

protected:
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;

private:
	TSharedPtr<FPreviewScene> PreviewScene;
	TSharedPtr<FToonPreviewViewportClient> ViewportClient;
	USkeletalMeshComponent* PreviewComp = nullptr;
	USkeletalMeshComponent* OutlineComp = nullptr;
};
