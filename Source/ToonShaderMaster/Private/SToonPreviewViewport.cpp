#include "SToonPreviewViewport.h"

#include "EditorViewportClient.h"
#include "PreviewScene.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

class FToonPreviewViewportClient : public FEditorViewportClient
{
public:
	FToonPreviewViewportClient(FPreviewScene& InScene, const TSharedRef<SEditorViewport>& InViewport)
		: FEditorViewportClient(nullptr, &InScene, InViewport)
	{
		SetViewMode(VMI_Lit);
		SetRealtime(true);
		ViewFOV = 45.f;
		DrawHelper.bDrawGrid = false;
		DrawHelper.bDrawPivot = false;
		bSetListenerPosition = false;
		bUsingOrbitCamera = false; // FPS / フライカメラ操作向け
		SetCustomCameraSpeed(1.f);
	}

	virtual void Tick(float DeltaSeconds) override
	{
		FEditorViewportClient::Tick(DeltaSeconds);

		if (PreviewScene && PreviewScene->GetWorld())
		{
			PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
		}
	}

	/** カメラ速度(1.0が標準) */
	void SetCustomCameraSpeed(float Speed)
	{
		CustomCameraSpeed = FMath::Clamp(Speed, 0.1f, 10.f);
		CameraSpeedScalar = CustomCameraSpeed;
	}

	float GetCustomCameraSpeed() const
	{
		return CustomCameraSpeed;
	}

	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override
	{
		const bool bIsScrollUp = (EventArgs.Key == EKeys::MouseScrollUp);
		const bool bIsScrollDown = (EventArgs.Key == EKeys::MouseScrollDown);

		if (EventArgs.Event == IE_Pressed && (bIsScrollUp || bIsScrollDown))
		{
			FViewport* ActiveViewport = EventArgs.Viewport;
			const bool bHoldingMouseButton = ActiveViewport &&
				(ActiveViewport->KeyState(EKeys::LeftMouseButton) || ActiveViewport->KeyState(EKeys::RightMouseButton));

			// 左または右ボタンを押しながらのホイールでカメラ速度をオンザフライ変更
			if (bHoldingMouseButton)
			{
				const float Step = bIsScrollUp ? 0.25f : -0.25f;
				SetCustomCameraSpeed(CustomCameraSpeed + Step);
				Invalidate();
				return true;
			}
		}

		return FEditorViewportClient::InputKey(EventArgs);
	}

	virtual FLinearColor GetBackgroundColor() const override
	{
		return FLinearColor(0.12f, 0.12f, 0.14f);
	}

private:
	float CustomCameraSpeed = 1.f;
};


void SToonPreviewViewport::Construct(const FArguments& InArgs)
{
	PreviewScene = MakeShared<FPreviewScene>(FPreviewScene::ConstructionValues());
	SEditorViewport::Construct(SEditorViewport::FArguments());
}

SToonPreviewViewport::~SToonPreviewViewport()
{
	if (PreviewScene.IsValid())
	{
		if (OutlineComp)
		{
			PreviewScene->RemoveComponent(OutlineComp);
			OutlineComp = nullptr;
		}
		if (PreviewComp)
		{
			PreviewScene->RemoveComponent(PreviewComp);
			PreviewComp = nullptr;
		}
	}
}

TSharedRef<FEditorViewportClient> SToonPreviewViewport::MakeEditorViewportClient()
{
	ViewportClient = MakeShared<FToonPreviewViewportClient>(*PreviewScene, SharedThis(this));
	return ViewportClient.ToSharedRef();
}

void SToonPreviewViewport::SetMesh(USkeletalMesh* InMesh)
{
	if (OutlineComp)
	{
		PreviewScene->RemoveComponent(OutlineComp);
		OutlineComp = nullptr;
	}
	if (PreviewComp)
	{
		PreviewScene->RemoveComponent(PreviewComp);
		PreviewComp = nullptr;
	}

	if (!InMesh)
	{
		return;
	}

	PreviewComp = NewObject<USkeletalMeshComponent>(GetTransientPackage());
	PreviewComp->SetSkeletalMesh(InMesh);
	PreviewScene->AddComponent(PreviewComp, FTransform::Identity);

	// アウトライン用: 同じメッシュを重ね、親のポーズに追従させる(影なし)
	OutlineComp = NewObject<USkeletalMeshComponent>(GetTransientPackage());
	OutlineComp->SetSkeletalMeshAsset(InMesh);
	OutlineComp->CastShadow = false;
	OutlineComp->bCastDynamicShadow = false;
	OutlineComp->BoundsScale = 1.3f;
	PreviewScene->AddComponent(OutlineComp, FTransform::Identity);
	OutlineComp->SetLeaderPoseComponent(PreviewComp);

	FrameMesh();
}

void SToonPreviewViewport::SetSlotMaterial(int32 SlotIndex, UMaterialInterface* Material)
{
	if (PreviewComp)
	{
		PreviewComp->SetMaterial(SlotIndex, Material);
	}
}

void SToonPreviewViewport::SetOutlineSlotMaterial(int32 SlotIndex, UMaterialInterface* Material)
{
	if (OutlineComp)
	{
		OutlineComp->SetMaterial(SlotIndex, Material);
	}
}

void SToonPreviewViewport::SetCameraSpeed(float Speed)
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->SetCustomCameraSpeed(Speed);
	}
}

void SToonPreviewViewport::SetLightRotation(float Pitch, float Yaw)
{
	if (PreviewScene.IsValid())
	{
		PreviewScene->SetLightDirection(FRotator(Pitch, Yaw, 0.f));
	}
}

void SToonPreviewViewport::FrameMesh()
{
	if (!PreviewComp || !ViewportClient.IsValid())
	{
		return;
	}

	const FBoxSphereBounds Bounds = PreviewComp->Bounds;
	const float Radius = FMath::Max(Bounds.SphereRadius, 1.f);

	// UEのキャラは+X向きなので、+X側から原点方向(yaw=180)を見る
	ViewportClient->SetViewRotation(FRotator(-5.f, 180.f, 0.f));
	ViewportClient->SetViewLocationForOrbiting(Bounds.Origin, Radius * 2.5f);
}
