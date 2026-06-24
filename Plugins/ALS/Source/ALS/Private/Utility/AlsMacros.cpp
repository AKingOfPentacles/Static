#include "Utility/AlsMacros.h"
#include "CoreGlobals.h"
#include "Templates/Function.h"
#include "Engine/EngineTypes.h" // FOverlapResult ve FMTDResult
#include "DrawDebugHelpers.h"   // ENABLE_DRAW_DEBUG ve DrawDebugLine
#include "Components/PrimitiveComponent.h" // FBodyInstance

#if DO_ENSURE && !USING_CODE_ANALYSIS

bool UE_DEBUG_SECTION AlsEnsure::Execute(std::atomic<bool>& bExecuted, const bool bEnsureAlways, const ANSICHAR* Expression,
	const TCHAR* StaticMessage, const TCHAR* Format, ...)
{
	if ((bExecuted.load(std::memory_order_relaxed) && !bEnsureAlways) || !FPlatformMisc::IsEnsureAllowed())
	{
		return false;
	}

	bExecuted.store(true, std::memory_order_relaxed);

	static constexpr auto FormattedMessageSize{ 4096 };
	TCHAR FormattedMessage[FormattedMessageSize];

	va_list Args;
	va_start(Args, Format);
	TCString<TCHAR>::GetVarArgs(FormattedMessage, FormattedMessageSize, Format, Args);
	va_end(Args);

	if (UNLIKELY(GetEnsureHandler() && GetEnsureHandler()({ Expression, FormattedMessage })))
	{
		return false;
	}

	UE_LOG(LogOutputDevice, Warning, TEXT("%s"), StaticMessage);
	UE_LOG(LogOutputDevice, Warning, TEXT("%s"), FormattedMessage);

	PrintScriptCallstack();

	if (!FPlatformMisc::IsDebuggerPresent())
	{
		FPlatformMisc::PromptForRemoteDebugging(true);
		return false;
	}

#if UE_BUILD_SHIPPING
	return true;
#else
	return !GIgnoreDebugger;
#endif
}

#endif