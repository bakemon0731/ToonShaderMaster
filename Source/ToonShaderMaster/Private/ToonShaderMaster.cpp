#include "Modules/ModuleManager.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "SToonConverterWidget.h"

#define LOCTEXT_NAMESPACE "FToonShaderMasterModule"

static const FName ToonConverterTabName(TEXT("ToonShaderMasterConverter"));

class FToonShaderMasterModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			ToonConverterTabName,
			FOnSpawnTab::CreateRaw(this, &FToonShaderMasterModule::SpawnTab))
			.SetDisplayName(LOCTEXT("TabTitle", "Toon Shader Converter"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FToonShaderMasterModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ToonConverterTabName);
	}

private:
	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SToonConverterWidget)
			];
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);

		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		FToolMenuSection& Section = Menu->AddSection("ToonShaderMaster", LOCTEXT("SectionLabel", "Toon Shader"));
		Section.AddMenuEntry(
			"OpenToonConverter",
			LOCTEXT("MenuLabel", "Toon Shader Converter"),
			LOCTEXT("MenuTooltip", "スケルタルメッシュのマテリアルをトゥーンシェーダーMIへ変換します"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(ToonConverterTabName);
			})));
	}
};

IMPLEMENT_MODULE(FToonShaderMasterModule, ToonShaderMaster)

#undef LOCTEXT_NAMESPACE
