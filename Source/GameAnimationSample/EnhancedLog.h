#pragma once
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UObject/Interface.h"
#include "EnhancedLog.generated.h"

UCLASS()
class GAMEANIMATIONSAMPLE_API UEnhancedPlayerInfo : public UObject
{
	GENERATED_BODY()

public:
	static FString DefaultGetPlayerInfoString(const UObject* ContextObject);
};

UINTERFACE(Blueprintable)
class GAMEANIMATIONSAMPLE_API UUEnhancedPlayerInfoInterface : public UInterface
{
	GENERATED_BODY()

public:
};

class GAMEANIMATIONSAMPLE_API IUEnhancedPlayerInfoInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent)
	FString GetPlayerName();
	virtual FString GetPlayerName_Implementation();

	UFUNCTION(BlueprintNativeEvent)
	int64 GetPlayerId();
	virtual int64 GetPlayerId_Implementation();
};



#define UE_LOG_ENHANCED(Category, Verbosity, ContextObject, Fmt, ...) \
	{ \
		static ::UE::Logging::Private::FStaticBasicLogDynamicData LOG_Dynamic; \
		static_assert((::ELogVerbosity::Verbosity & ::ELogVerbosity::VerbosityMask) < ::ELogVerbosity::NumVerbosity && ::ELogVerbosity::Verbosity > 0, "Verbosity must be constant and in range."); \
		if constexpr ((::ELogVerbosity::Verbosity & ELogVerbosity::VerbosityMask) == ::ELogVerbosity::Fatal) \
		{ \
			{ \
				const FString FormattedString = UEnhancedPlayerInfo::DefaultGetPlayerInfoString(ContextObject) + FString(Fmt);\
				constexpr int MAX_FORMAT = 4096;\
				const int CopyLen = FMath::Clamp(FormattedString.Len()+1, 0, MAX_FORMAT - 1);\
				static TCHAR FormatArray[MAX_FORMAT] = {} ;\
				FCString::Strncpy(FormatArray, *FormattedString, CopyLen);\
				static constexpr ::UE::Logging::Private::FStaticBasicLogRecord LOG_Static(FormatArray, __builtin_FILE(), __builtin_LINE(), ::ELogVerbosity::Verbosity, LOG_Dynamic); \
				::UE::Logging::Private::BasicFatalLog(Category, &LOG_Static, ##__VA_ARGS__); \
				CA_ASSUME(false); \
			} \
		} \
		else if constexpr ((::ELogVerbosity::Verbosity & ::ELogVerbosity::VerbosityMask) <= ::ELogVerbosity::COMPILED_IN_MINIMUM_VERBOSITY) \
		{ \
			if constexpr ((::ELogVerbosity::Verbosity & ::ELogVerbosity::VerbosityMask) <= Category.GetCompileTimeVerbosity()) \
			{ \
				if (!Category.IsSuppressed(::ELogVerbosity::Verbosity)) \
				{ \
					{ \
						const FString FormattedString = UEnhancedPlayerInfo::DefaultGetPlayerInfoString(ContextObject) + FString(Fmt);\
						constexpr int MAX_FORMAT = 4096;\
						const int CopyLen = FMath::Clamp(FormattedString.Len()+1, 0, MAX_FORMAT - 1);\
						static TCHAR FormatArray[MAX_FORMAT] = {} ;\
						FCString::Strncpy(FormatArray, *FormattedString, CopyLen);\
						static constexpr ::UE::Logging::Private::FStaticBasicLogRecord LOG_Static(FormatArray, __builtin_FILE(), __builtin_LINE(), ::ELogVerbosity::Verbosity, LOG_Dynamic); \
						::UE::Logging::Private::BasicLog(Category, &LOG_Static, ##__VA_ARGS__); \
					} \
				} \
			} \
		} \
	}

#define UE_LOG_GATED(GateVar, CategoryName, Verbosity, ContextObject, Fmt, ...) \
	if (GateVar) { UE_LOG_ENHANCED(CategoryName, Verbosity, ContextObject, Fmt, ##__VA_ARGS__) }