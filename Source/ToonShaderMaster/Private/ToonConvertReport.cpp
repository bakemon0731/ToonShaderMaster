#include "ToonConvertReport.h"

#include "ToonConverter.h"
#include "Misc/MessageDialog.h"

DEFINE_LOG_CATEGORY(LogToonShaderMaster);

void ToonConvertReport::Report(const FToonConvertResult& Result)
{
	UE_LOG(LogToonShaderMaster, Log, TEXT("変換完了 新規作成: %d / 再利用: %d"), Result.Created, Result.Reused);

	if (!Result.BlueprintPath.IsEmpty())
	{
		UE_LOG(LogToonShaderMaster, Log, TEXT("BP Actorを作成: %s"), *Result.BlueprintPath);
	}
	if (!Result.BlueprintNote.IsEmpty())
	{
		UE_LOG(LogToonShaderMaster, Log, TEXT("%s"), *Result.BlueprintNote);
	}

	if (Result.Errors.Num() == 0)
	{
		return;
	}

	FString Msg = TEXT("変換中にエラーが出ました。");
	for (const FString& Err : Result.Errors)
	{
		UE_LOG(LogToonShaderMaster, Error, TEXT("%s"), *Err);
		Msg += TEXT("\n") + Err;
	}
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Msg));
}
