// Copyright Potionify. Apache-2.0.

#include "Script/ConvexScriptMixins.h"

#include <cmath>

// ---------------------------------------------------------------------------
// FConvexValue: kind
// ---------------------------------------------------------------------------

EConvexValueKind UConvexScriptMixins::Kind(const FConvexValue& Value)
{
	return Value.GetKind();
}

bool UConvexScriptMixins::IsNull(const FConvexValue& Value)
{
	return Value.GetKind() == EConvexValueKind::Null;
}

bool UConvexScriptMixins::IsBool(const FConvexValue& Value)
{
	return Value.GetKind() == EConvexValueKind::Boolean;
}

bool UConvexScriptMixins::IsInt(const FConvexValue& Value)
{
	return Value.GetKind() == EConvexValueKind::Int64;
}

bool UConvexScriptMixins::IsFloat(const FConvexValue& Value)
{
	return Value.GetKind() == EConvexValueKind::Float64;
}

bool UConvexScriptMixins::IsNumber(const FConvexValue& Value)
{
	const EConvexValueKind Kind = Value.GetKind();
	return Kind == EConvexValueKind::Int64 || Kind == EConvexValueKind::Float64;
}

bool UConvexScriptMixins::IsString(const FConvexValue& Value)
{
	return Value.GetKind() == EConvexValueKind::String;
}

bool UConvexScriptMixins::IsBytes(const FConvexValue& Value)
{
	return Value.GetKind() == EConvexValueKind::Bytes;
}

bool UConvexScriptMixins::IsArray(const FConvexValue& Value)
{
	return Value.GetKind() == EConvexValueKind::Array;
}

bool UConvexScriptMixins::IsObject(const FConvexValue& Value)
{
	return Value.GetKind() == EConvexValueKind::Object;
}

// ---------------------------------------------------------------------------
// FConvexValue: scalar reads
// ---------------------------------------------------------------------------

bool UConvexScriptMixins::AsBool(const FConvexValue& Value)
{
	return AsBoolOr(Value, false);
}

bool UConvexScriptMixins::AsBoolOr(const FConvexValue& Value, bool Default)
{
	bool Out = false;
	return Value.TryGetBool(Out) ? Out : Default;
}

int64 UConvexScriptMixins::AsInt(const FConvexValue& Value)
{
	return AsIntOr(Value, 0);
}

int64 UConvexScriptMixins::AsIntOr(const FConvexValue& Value, int64 Default)
{
	int64 Out = 0;
	if (Value.TryGetInt64(Out))
	{
		return Out;
	}
	// A JavaScript number that holds a whole value within the exactly
	// representable range reads as an integer too.
	double AsDouble = 0.0;
	if (Value.TryGetFloat(AsDouble) && std::isfinite(AsDouble) && std::trunc(AsDouble) == AsDouble
		&& AsDouble >= -9007199254740992.0 && AsDouble <= 9007199254740992.0)
	{
		return static_cast<int64>(AsDouble);
	}
	return Default;
}

double UConvexScriptMixins::AsFloat(const FConvexValue& Value)
{
	return AsFloatOr(Value, 0.0);
}

double UConvexScriptMixins::AsFloatOr(const FConvexValue& Value, double Default)
{
	double Out = 0.0;
	if (Value.TryGetFloat(Out))
	{
		return Out;
	}
	int64 AsInt64 = 0;
	if (Value.TryGetInt64(AsInt64))
	{
		return static_cast<double>(AsInt64);
	}
	return Default;
}

FString UConvexScriptMixins::AsString(const FConvexValue& Value)
{
	return AsStringOr(Value, FString());
}

FString UConvexScriptMixins::AsStringOr(const FConvexValue& Value, const FString& Default)
{
	FString Out;
	return Value.TryGetString(Out) ? Out : Default;
}

TArray<uint8> UConvexScriptMixins::AsBytes(const FConvexValue& Value)
{
	TArray<uint8> Out;
	Value.TryGetBytes(Out);
	return Out;
}

TArray<FConvexValue> UConvexScriptMixins::AsArray(const FConvexValue& Value)
{
	TArray<FConvexValue> Out;
	Value.TryGetArray(Out);
	return Out;
}

TMap<FString, FConvexValue> UConvexScriptMixins::AsObject(const FConvexValue& Value)
{
	TMap<FString, FConvexValue> Out;
	Value.TryGetObject(Out);
	return Out;
}

// ---------------------------------------------------------------------------
// FConvexValue: navigation
// ---------------------------------------------------------------------------

int32 UConvexScriptMixins::Length(const FConvexValue& Value)
{
	TArray<FConvexValue> Elements;
	if (Value.TryGetArray(Elements))
	{
		return Elements.Num();
	}
	TMap<FString, FConvexValue> Fields;
	if (Value.TryGetObject(Fields))
	{
		return Fields.Num();
	}
	return 0;
}

FConvexValue UConvexScriptMixins::At(const FConvexValue& Value, int32 Index)
{
	TArray<FConvexValue> Elements;
	if (Value.TryGetArray(Elements) && Elements.IsValidIndex(Index))
	{
		return Elements[Index];
	}
	return FConvexValue::Null();
}

bool UConvexScriptMixins::HasField(const FConvexValue& Value, const FString& Name)
{
	TMap<FString, FConvexValue> Fields;
	return Value.TryGetObject(Fields) && Fields.Contains(Name);
}

FConvexValue UConvexScriptMixins::Get(const FConvexValue& Value, const FString& Name)
{
	TMap<FString, FConvexValue> Fields;
	if (Value.TryGetObject(Fields))
	{
		if (const FConvexValue* Found = Fields.Find(Name))
		{
			return *Found;
		}
	}
	return FConvexValue::Null();
}

FConvexValue UConvexScriptMixins::Field(const FConvexValue& Value, const FString& Path)
{
	TArray<FString> Segments;
	Path.ParseIntoArray(Segments, TEXT("."), /*InCullEmpty=*/true);
	FConvexValue Current = Value;
	for (const FString& Segment : Segments)
	{
		TMap<FString, FConvexValue> Fields;
		if (Current.TryGetObject(Fields))
		{
			const FConvexValue* Found = Fields.Find(Segment);
			if (Found == nullptr)
			{
				return FConvexValue::Null();
			}
			Current = *Found;
			continue;
		}
		TArray<FConvexValue> Elements;
		if (Current.TryGetArray(Elements) && Segment.IsNumeric())
		{
			const int32 Index = FCString::Atoi(*Segment);
			if (!Elements.IsValidIndex(Index))
			{
				return FConvexValue::Null();
			}
			Current = Elements[Index];
			continue;
		}
		return FConvexValue::Null();
	}
	return Current;
}

TArray<FString> UConvexScriptMixins::Keys(const FConvexValue& Value)
{
	TArray<FString> Out;
	TMap<FString, FConvexValue> Fields;
	if (Value.TryGetObject(Fields))
	{
		Fields.GetKeys(Out);
		Out.Sort();
	}
	return Out;
}

FString UConvexScriptMixins::ToJson(const FConvexValue& Value)
{
	bool bOk = false;
	FString Json = Value.ToWire(bOk);
	return bOk ? Json : FString();
}

// ---------------------------------------------------------------------------
// FConvexResult
// ---------------------------------------------------------------------------

bool UConvexScriptMixins::IsError(const FConvexResult& Result)
{
	return !Result.bSuccess;
}

FConvexValue UConvexScriptMixins::ResultField(const FConvexResult& Result, const FString& Path)
{
	return Result.bSuccess ? Field(Result.Value, Path) : FConvexValue::Null();
}

FString UConvexScriptMixins::Describe(const FConvexResult& Result)
{
	if (Result.bSuccess)
	{
		return ToJson(Result.Value);
	}
	if (Result.bIsAppError)
	{
		return FString::Printf(TEXT("ConvexError: %s %s"), *Result.ErrorMessage, *ToJson(Result.ErrorData));
	}
	return FString::Printf(TEXT("Error: %s"), *Result.ErrorMessage);
}

// ---------------------------------------------------------------------------
// FConvexPaginatedSnapshot
// ---------------------------------------------------------------------------

int32 UConvexScriptMixins::Num(const FConvexPaginatedSnapshot& Snapshot)
{
	return Snapshot.Results.Num();
}

FConvexValue UConvexScriptMixins::Item(const FConvexPaginatedSnapshot& Snapshot, int32 Index)
{
	return Snapshot.Results.IsValidIndex(Index) ? Snapshot.Results[Index] : FConvexValue::Null();
}

bool UConvexScriptMixins::CanLoadMore(const FConvexPaginatedSnapshot& Snapshot)
{
	return Snapshot.Status == EConvexPaginationStatus::CanLoadMore;
}

bool UConvexScriptMixins::IsExhausted(const FConvexPaginatedSnapshot& Snapshot)
{
	return Snapshot.Status == EConvexPaginationStatus::Exhausted;
}

bool UConvexScriptMixins::HasError(const FConvexPaginatedSnapshot& Snapshot)
{
	return Snapshot.Status == EConvexPaginationStatus::Error;
}
