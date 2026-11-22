#include "ToonOutlineBlueprint.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"

namespace
{
	const FName ToonVarName(TEXT("ToonMesh"));
	const FName OutlineVarName(TEXT("OutlineMesh"));

	UK2Node_VariableGet* MakeGet(UEdGraph* Graph, FName Var, int32 X, int32 Y)
	{
		FGraphNodeCreator<UK2Node_VariableGet> Creator(*Graph);
		UK2Node_VariableGet* Node = Creator.CreateNode();
		Node->VariableReference.SetSelfMember(Var);
		Node->NodePosX = X;
		Node->NodePosY = Y;
		Creator.Finalize();
		return Node;
	}

	/** コンストラクションスクリプトに OutlineMesh->SetLeaderPoseComponent(ToonMesh) を足す。成功でtrue */
	bool AddLeaderPoseNodes(UBlueprint* BP)
	{
		UEdGraph* Graph = FBlueprintEditorUtils::FindUserConstructionScript(BP);
		if (!Graph)
		{
			return false;
		}

		TArray<UK2Node_FunctionEntry*> Entries;
		Graph->GetNodesOfClass(Entries);
		if (Entries.Num() == 0)
		{
			return false;
		}
		UK2Node_FunctionEntry* Entry = Entries[0];

		UFunction* Func = USkinnedMeshComponent::StaticClass()->FindFunctionByName(TEXT("SetLeaderPoseComponent"));
		if (!Func)
		{
			return false;
		}

		FGraphNodeCreator<UK2Node_CallFunction> CallCreator(*Graph);
		UK2Node_CallFunction* Call = CallCreator.CreateNode();
		Call->SetFromFunction(Func);
		Call->NodePosX = Entry->NodePosX + 400;
		Call->NodePosY = Entry->NodePosY;
		CallCreator.Finalize();

		UK2Node_VariableGet* OutlineGet = MakeGet(Graph, OutlineVarName, Entry->NodePosX + 100, Entry->NodePosY + 200);
		UK2Node_VariableGet* ToonGet = MakeGet(Graph, ToonVarName, Entry->NodePosX + 100, Entry->NodePosY + 300);

		const UEdGraphSchema_K2* K2 = GetDefault<UEdGraphSchema_K2>();

		UEdGraphPin* ThenPin = Entry->FindPin(UEdGraphSchema_K2::PN_Then);
		UEdGraphPin* ExecPin = Call->GetExecPin();
		UEdGraphPin* SelfPin = Call->FindPin(UEdGraphSchema_K2::PN_Self);
		if (!ThenPin || !ExecPin || !SelfPin)
		{
			return false;
		}

		// 引数側: Self以外の最初のオブジェクト入力ピン(= Leader にするコンポーネント)
		UEdGraphPin* LeaderPin = nullptr;
		for (UEdGraphPin* Pin : Call->Pins)
		{
			if (Pin->Direction == EGPD_Input
				&& Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object
				&& Pin->PinName != UEdGraphSchema_K2::PN_Self)
			{
				LeaderPin = Pin;
				break;
			}
		}
		if (!LeaderPin)
		{
			return false;
		}

		bool bOk = true;
		bOk &= K2->TryCreateConnection(ThenPin, ExecPin);
		bOk &= K2->TryCreateConnection(OutlineGet->GetValuePin(), SelfPin);
		bOk &= K2->TryCreateConnection(ToonGet->GetValuePin(), LeaderPin);
		return bOk;
	}
}

UBlueprint* ToonOutlineBlueprint::Create(USkeletalMesh* Mesh, const TArray<UMaterialInterface*>& OutlineMats, FString& OutNote)
{
	if (!Mesh)
	{
		OutNote = TEXT("メッシュが指定されていません。");
		return nullptr;
	}

	const FString Dir = FPackageName::GetLongPackagePath(Mesh->GetOutermost()->GetName());
	const FString AssetName = ObjectTools::SanitizeObjectName(FString::Printf(TEXT("BP_%s_Toon"), *Mesh->GetName()));
	const FString PackageName = Dir / AssetName;

	if (FPackageName::DoesPackageExist(PackageName))
	{
		OutNote = FString::Printf(TEXT("%s は既にあるため作り直しませんでした(アウトラインMIは更新済み)。"), *AssetName);
		return nullptr;
	}

	UPackage* Package = CreatePackage(*PackageName);
	UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(), Package, FName(*AssetName), BPTYPE_Normal,
		UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), FName(TEXT("ToonShaderMaster")));
	if (!BP || !BP->SimpleConstructionScript)
	{
		OutNote = TEXT("BP Actorの作成に失敗しました。");
		return nullptr;
	}

	USimpleConstructionScript* SCS = BP->SimpleConstructionScript;

	// 既定のルート(DefaultSceneRoot)を外して、ToonMeshをルートにする
	const TArray<USCS_Node*> OldRoots = SCS->GetRootNodes();
	for (USCS_Node* Old : OldRoots)
	{
		SCS->RemoveNode(Old);
	}

	USCS_Node* ToonNode = SCS->CreateNode(USkeletalMeshComponent::StaticClass(), ToonVarName);
	SCS->AddNode(ToonNode);
	USCS_Node* OutlineNode = SCS->CreateNode(USkeletalMeshComponent::StaticClass(), OutlineVarName);
	ToonNode->AddChildNode(OutlineNode);

	if (USkeletalMeshComponent* ToonComp = Cast<USkeletalMeshComponent>(ToonNode->ComponentTemplate))
	{
		ToonComp->SetSkeletalMeshAsset(Mesh);
	}

	if (USkeletalMeshComponent* OutlineComp = Cast<USkeletalMeshComponent>(OutlineNode->ComponentTemplate))
	{
		OutlineComp->SetSkeletalMeshAsset(Mesh);
		OutlineComp->CastShadow = false;
		OutlineComp->bCastDynamicShadow = false;
		OutlineComp->BoundsScale = 1.3f; // WPOで膨らむ分、カリングで消えないように
		for (int32 i = 0; i < OutlineMats.Num(); ++i)
		{
			if (OutlineMats[i])
			{
				OutlineComp->SetMaterial(i, OutlineMats[i]);
			}
		}
	}

	// コンポーネント変数をグラフから参照できるよう、一度コンパイルしてからノードを足す
	FKismetEditorUtilities::CompileBlueprint(BP);
	const bool bGraphOk = AddLeaderPoseNodes(BP);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);

	if (!bGraphOk)
	{
		OutNote = TEXT("Leader Poseの自動設定に失敗しました。BPのコンストラクションスクリプトで OutlineMesh の Set Leader Pose Component に ToonMesh を渡してください。");
	}

	FAssetRegistryModule::AssetCreated(BP);
	Package->MarkPackageDirty();
	return BP;
}
