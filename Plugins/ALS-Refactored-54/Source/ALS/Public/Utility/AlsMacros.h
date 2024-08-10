#pragma once

#include "Misc/AssertionMacros.h"

#define ALS_STRINGIFY_IMPLEMENTATION(Value) #Value

#define ALS_STRINGIFY(Value) ALS_STRINGIFY_IMPLEMENTATION(Value)

#define ALS_GET_TYPE_STRING(Type) \
    ((void)sizeof UEAsserts_Private::GetMemberNameCheckedJunk(static_cast<Type*>(nullptr)), TEXTVIEW(#Type))

#if DO_ENSURE && !USING_CODE_ANALYSIS

namespace AlsEnsure
{
    ALS_API bool UE_DEBUG_SECTION VARARGS Execute(std::atomic<bool>& bExecuted, bool bEnsureAlways, const ANSICHAR* Expression,
        const TCHAR* StaticMessage, const TCHAR* Format, ...);
}

#define ALS_ENSURE_IMPLEMENTATION(Capture, bEnsureAlways, Expression, Format, ...) \
    (LIKELY(Expression) || ([Capture]() UE_DEBUG_SECTION \
    { \
        static constexpr auto StaticMessage{TEXT("Ensure failed: " #Expression ", File: " __FILE__ ", Line: " ALS_STRINGIFY(__LINE__) ".")}; \
        static std::atomic<bool> bExecuted{false}; \
        \
        UE_VALIDATE_FORMAT_STRING(Format, ##__VA_ARGS__); \
        static constexpr ::UE::Assert::Private::FStaticEnsureRecord ENSURE_Static(Format, #Expression, __builtin_FILE(), __builtin_LINE(), bEnsureAlways); \
        if ((bEnsureAlways || !bExecuted.load(std::memory_order_relaxed)) && FPlatformMisc::IsEnsureAllowed() && ::UE::Assert::Private::EnsureFailed(bExecuted, &ENSURE_Static, ##__VA_ARGS__)) \
        { \
            PLATFORM_BREAK(); \
        } \
        \
        return false; \
    }()))

#define ALS_ENSURE(Expression) ALS_ENSURE_IMPLEMENTATION( , false, Expression, TEXT(""))
#define ALS_ENSURE_MESSAGE(Expression, Format, ...) ALS_ENSURE_IMPLEMENTATION(&, false, Expression, Format, ##__VA_ARGS__)
#define ALS_ENSURE_ALWAYS(Expression) ALS_ENSURE_IMPLEMENTATION( , true, Expression, TEXT(""))
#define ALS_ENSURE_ALWAYS_MESSAGE(Expression, Format, ...) ALS_ENSURE_IMPLEMENTATION(&, true, Expression, Format, ##__VA_ARGS__)

#else

#define ALS_ENSURE(Expression) (LIKELY(!!(Expression)))
#define ALS_ENSURE_MESSAGE(Expression, Format, ...) (LIKELY(!!(Expression)))
#define ALS_ENSURE_ALWAYS(Expression) (LIKELY(!!(Expression)))
#define ALS_ENSURE_ALWAYS_MESSAGE(Expression, Format, ...) (LIKELY(!!(Expression)))

#endif