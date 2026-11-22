#pragma once

#include "CoreMinimal.h"

struct FToonConvertResult;

DECLARE_LOG_CATEGORY_EXTERN(LogToonShaderMaster, Log, All);

/** 変換結果の知らせ方を決める(変換そのものは ToonConverter の担当) */
namespace ToonConvertReport
{
	/**
	 * 結果をOutput Logへ書く。ダイアログは、エラーがあるときだけ出す(成功時は何も出さない)。
	 */
	void Report(const FToonConvertResult& Result);
}
