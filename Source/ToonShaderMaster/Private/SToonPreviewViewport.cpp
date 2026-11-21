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
		bUsingOrbitCamera = true;
	}

	virtual void Tick(float DeltaSeconds) override
	{
		FEditorViewportClient::Tick(DeltaSeconds);

		if (PreviewScene && PreviewScene->GetWorld())
		{
			PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
		}
	}

	virtual FLinearColor GetBackgroundColor() const override
	{
		return FLinearColor(0.12f, 0.12f, 0.14f);
	}
};

void SToonPreviewViewport::Construct(const FArguments& InArgs)
{
	PreviewScene = MakeShared<FPreviewScene>(FPreviewScene::ConstructionValues());
	SEditorViewport::Construct(SEditorViewport::FArguments());
}

SToonPreviewViewport::~SToonPreviewViewport()
{
	if (PreviewComp && PreviewScene.IsValid())
	{
		PreviewScene->RemoveComponent(PreviewComp);
		PreviewComp = nullptr;
	}
}

TSharedRef<FEditorViewportClient> SToonPreviewViewport::MakeEditorViewportClient()
{
	ViewportClient = MakeShared<FToonPreviewViewportClient>(*PreviewScene, SharedThis(this));
	return ViewportClient.ToSharedRef();
}

void SToonPreviewViewport::SetMesh(USkeletalMesh* InMesh)
{
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

	FrameMesh();
}

void SToonPreviewViewport::SetSlotMaterial(int32 SlotIndex, UMaterialInterface* Material)
{
	if (PreviewComp)
	{
		PreviewComp->SetMaterial(SlotIndex, Material);
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
