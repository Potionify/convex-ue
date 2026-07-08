// Copyright Potionify. Apache-2.0.

#include "Blueprint/ConvexBlueprintLibrary.h"

#include "ConvexClient.h"
#include "ConvexSubsystem.h"

// ---------------------------------------------------------------------------
// Value construction
// ---------------------------------------------------------------------------

FConvexValue UConvexBlueprintLibrary::MakeConvexNull()
{
	return FConvexValue::Null();
}

FConvexValue UConvexBlueprintLibrary::MakeConvexBool(bool bValue)
{
	return FConvexValue::Bool(bValue);
}

FConvexValue UConvexBlueprintLibrary::MakeConvexInt(int64 Value)
{
	return FConvexValue::Int64(Value);
}

FConvexValue UConvexBlueprintLibrary::MakeConvexFloat(double Value)
{
	return FConvexValue::Float(Value);
}

FConvexValue UConvexBlueprintLibrary::MakeConvexString(const FString& Value)
{
	return FConvexValue::String(Value);
}

FConvexValue UConvexBlueprintLibrary::MakeConvexBytes(const TArray<uint8>& Value)
{
	return FConvexValue::Bytes(Value);
}

FConvexValue UConvexBlueprintLibrary::MakeConvexArray(const TArray<FConvexValue>& Values)
{
	return FConvexValue::Array(Values);
}

FConvexValue UConvexBlueprintLibrary::MakeConvexObject(const TMap<FString, FConvexValue>& Fields)
{
	return FConvexValue::Object(Fields);
}

// ---------------------------------------------------------------------------
// Value reading
// ---------------------------------------------------------------------------

EConvexValueKind UConvexBlueprintLibrary::GetValueKind(const FConvexValue& Value)
{
	return Value.GetKind();
}

bool UConvexBlueprintLibrary::GetBoolValue(const FConvexValue& Value, bool& bSuccess)
{
	bool Out = false;
	bSuccess = Value.TryGetBool(Out);
	return Out;
}

int64 UConvexBlueprintLibrary::GetIntValue(const FConvexValue& Value, bool& bSuccess)
{
	int64 Out = 0;
	bSuccess = Value.TryGetInt64(Out);
	return Out;
}

double UConvexBlueprintLibrary::GetFloatValue(const FConvexValue& Value, bool& bSuccess)
{
	double Out = 0.0;
	bSuccess = Value.TryGetFloat(Out);
	return Out;
}

FString UConvexBlueprintLibrary::GetStringValue(const FConvexValue& Value, bool& bSuccess)
{
	FString Out;
	bSuccess = Value.TryGetString(Out);
	return Out;
}

TArray<uint8> UConvexBlueprintLibrary::GetBytesValue(const FConvexValue& Value, bool& bSuccess)
{
	TArray<uint8> Out;
	bSuccess = Value.TryGetBytes(Out);
	return Out;
}

bool UConvexBlueprintLibrary::GetArrayValue(const FConvexValue& Value, TArray<FConvexValue>& Out)
{
	Out.Reset();
	return Value.TryGetArray(Out);
}

bool UConvexBlueprintLibrary::GetObjectValue(const FConvexValue& Value, TMap<FString, FConvexValue>& Out)
{
	Out.Reset();
	return Value.TryGetObject(Out);
}

FConvexValue UConvexBlueprintLibrary::GetArrayElement(const FConvexValue& Value, int32 Index, bool& bSuccess)
{
	TArray<FConvexValue> Elements;
	if (Value.TryGetArray(Elements) && Elements.IsValidIndex(Index))
	{
		bSuccess = true;
		return Elements[Index];
	}
	bSuccess = false;
	return FConvexValue::Null();
}

int32 UConvexBlueprintLibrary::GetArrayLength(const FConvexValue& Value)
{
	TArray<FConvexValue> Elements;
	if (Value.TryGetArray(Elements))
	{
		return Elements.Num();
	}
	return 0;
}

FConvexValue UConvexBlueprintLibrary::GetObjectField(const FConvexValue& Value, const FString& Name, bool& bSuccess)
{
	TMap<FString, FConvexValue> Fields;
	if (Value.TryGetObject(Fields))
	{
		if (const FConvexValue* Found = Fields.Find(Name))
		{
			bSuccess = true;
			return *Found;
		}
	}
	bSuccess = false;
	return FConvexValue::Null();
}

bool UConvexBlueprintLibrary::GetObjectKeys(const FConvexValue& Value, TArray<FString>& Out)
{
	Out.Reset();
	TMap<FString, FConvexValue> Fields;
	if (Value.TryGetObject(Fields))
	{
		Fields.GenerateKeyArray(Out);
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// Arguments
// ---------------------------------------------------------------------------

FConvexArgs UConvexBlueprintLibrary::MakeEmptyArgs()
{
	return FConvexArgs();
}

FConvexArgs UConvexBlueprintLibrary::AddStringArg(FConvexArgs Args, const FString& Name, const FString& Value)
{
	Args.Fields.Add(Name, FConvexValue::String(Value));
	return Args;
}

FConvexArgs UConvexBlueprintLibrary::AddIntArg(FConvexArgs Args, const FString& Name, int64 Value)
{
	Args.Fields.Add(Name, FConvexValue::Int64(Value));
	return Args;
}

FConvexArgs UConvexBlueprintLibrary::AddFloatArg(FConvexArgs Args, const FString& Name, double Value)
{
	Args.Fields.Add(Name, FConvexValue::Float(Value));
	return Args;
}

FConvexArgs UConvexBlueprintLibrary::AddBoolArg(FConvexArgs Args, const FString& Name, bool Value)
{
	Args.Fields.Add(Name, FConvexValue::Bool(Value));
	return Args;
}

FConvexArgs UConvexBlueprintLibrary::AddNullArg(FConvexArgs Args, const FString& Name)
{
	Args.Fields.Add(Name, FConvexValue::Null());
	return Args;
}

FConvexArgs UConvexBlueprintLibrary::AddBytesArg(FConvexArgs Args, const FString& Name, const TArray<uint8>& Value)
{
	Args.Fields.Add(Name, FConvexValue::Bytes(Value));
	return Args;
}

FConvexArgs UConvexBlueprintLibrary::AddValueArg(FConvexArgs Args, const FString& Name, const FConvexValue& Value)
{
	Args.Fields.Add(Name, Value);
	return Args;
}

const TMap<FString, FConvexValue>& UConvexBlueprintLibrary::ArgsToMap(const FConvexArgs& Args)
{
	return Args.Fields;
}

// ---------------------------------------------------------------------------
// Wire JSON
// ---------------------------------------------------------------------------

FString UConvexBlueprintLibrary::ValueToJsonString(const FConvexValue& Value, bool& bSuccess)
{
	return Value.ToWire(bSuccess);
}

FConvexValue UConvexBlueprintLibrary::JsonStringToValue(const FString& Json, bool& bSuccess)
{
	return FConvexValue::FromWire(Json, bSuccess);
}

// ---------------------------------------------------------------------------
// Result inspection
// ---------------------------------------------------------------------------

bool UConvexBlueprintLibrary::IsResultSuccess(const FConvexResult& Result)
{
	return Result.bSuccess;
}

bool UConvexBlueprintLibrary::IsResultAppError(const FConvexResult& Result)
{
	return Result.bIsAppError;
}

FConvexValue UConvexBlueprintLibrary::GetResultValue(const FConvexResult& Result)
{
	return Result.Value;
}

FString UConvexBlueprintLibrary::GetResultErrorMessage(const FConvexResult& Result)
{
	return Result.ErrorMessage;
}

FConvexValue UConvexBlueprintLibrary::GetResultErrorData(const FConvexResult& Result)
{
	return Result.ErrorData;
}

// ---------------------------------------------------------------------------
// Access
// ---------------------------------------------------------------------------

UConvexClient* UConvexBlueprintLibrary::GetDefaultConvexClient(const UObject* WorldContextObject)
{
	if (UConvexSubsystem* Subsystem = UConvexSubsystem::Get(WorldContextObject))
	{
		return Subsystem->GetDefaultClient();
	}
	return nullptr;
}
