// Copyright Potionify. Apache-2.0.

#include "ConvexValue.h"

#include "ConvexClientModule.h"
#include "ConvexUtils.h"

#include <convex/json_codec.h>

#include <exception>
#include <string>

using ConvexUtils::FStringToUtf8;
using ConvexUtils::Utf8ToFString;

FConvexValue::FConvexValue()
	: Inner(MakeShared<convex::value>())
{
}

FConvexValue FConvexValue::FromNative(const convex::value& InValue)
{
	FConvexValue Result;
	Result.Inner = MakeShared<convex::value>(InValue);
	return Result;
}

const convex::value& FConvexValue::GetNative() const
{
	// Inner is established by every constructor/factory, but guard defensively.
	static const convex::value NullValue;
	return Inner.IsValid() ? *Inner : NullValue;
}

FConvexValue FConvexValue::Null()
{
	return FConvexValue();
}

FConvexValue FConvexValue::Bool(bool bValue)
{
	return FromNative(convex::value(bValue));
}

FConvexValue FConvexValue::Int64(int64 Value)
{
	return FromNative(convex::value(static_cast<std::int64_t>(Value)));
}

FConvexValue FConvexValue::Float(double Value)
{
	return FromNative(convex::value(Value));
}

FConvexValue FConvexValue::String(const FString& Value)
{
	return FromNative(convex::value(FStringToUtf8(Value)));
}

FConvexValue FConvexValue::Bytes(const TArray<uint8>& Value)
{
	convex::bytes Buffer(Value.GetData(), Value.GetData() + Value.Num());
	return FromNative(convex::value(std::move(Buffer)));
}

FConvexValue FConvexValue::Array(const TArray<FConvexValue>& Value)
{
	convex::value_array Arr;
	Arr.reserve(Value.Num());
	for (const FConvexValue& Element : Value)
	{
		Arr.push_back(Element.GetNative());
	}
	return FromNative(convex::value(std::move(Arr)));
}

FConvexValue FConvexValue::Object(const TMap<FString, FConvexValue>& Value)
{
	return FromNative(convex::value(ConvexMakeArgs(Value)));
}

EConvexValueKind FConvexValue::GetKind() const
{
	switch (GetNative().get_kind())
	{
	case convex::value::kind::null:    return EConvexValueKind::Null;
	case convex::value::kind::boolean: return EConvexValueKind::Boolean;
	case convex::value::kind::int64:   return EConvexValueKind::Int64;
	case convex::value::kind::float64: return EConvexValueKind::Float64;
	case convex::value::kind::string:  return EConvexValueKind::String;
	case convex::value::kind::bytes:   return EConvexValueKind::Bytes;
	case convex::value::kind::array:   return EConvexValueKind::Array;
	case convex::value::kind::object:  return EConvexValueKind::Object;
	default:                           return EConvexValueKind::Null;
	}
}

bool FConvexValue::TryGetBool(bool& OutValue) const
{
	const convex::value& V = GetNative();
	if (!V.is_boolean())
	{
		return false;
	}
	OutValue = V.as_boolean();
	return true;
}

bool FConvexValue::TryGetInt64(int64& OutValue) const
{
	const convex::value& V = GetNative();
	if (!V.is_int64())
	{
		return false;
	}
	OutValue = static_cast<int64>(V.as_int64());
	return true;
}

bool FConvexValue::TryGetFloat(double& OutValue) const
{
	const convex::value& V = GetNative();
	if (!V.is_float64())
	{
		return false;
	}
	OutValue = V.as_float64();
	return true;
}

bool FConvexValue::TryGetString(FString& OutValue) const
{
	const convex::value& V = GetNative();
	if (!V.is_string())
	{
		return false;
	}
	OutValue = Utf8ToFString(V.as_string());
	return true;
}

bool FConvexValue::TryGetBytes(TArray<uint8>& OutValue) const
{
	const convex::value& V = GetNative();
	if (!V.is_bytes())
	{
		return false;
	}
	const convex::bytes& Buffer = V.as_bytes();
	OutValue.Reset();
	OutValue.Append(Buffer.data(), static_cast<int32>(Buffer.size()));
	return true;
}

bool FConvexValue::TryGetArray(TArray<FConvexValue>& OutValue) const
{
	const convex::value& V = GetNative();
	if (!V.is_array())
	{
		return false;
	}
	OutValue.Reset();
	for (const convex::value& Element : V.as_array())
	{
		OutValue.Add(FromNative(Element));
	}
	return true;
}

bool FConvexValue::TryGetObject(TMap<FString, FConvexValue>& OutFields) const
{
	const convex::value& V = GetNative();
	if (!V.is_object())
	{
		return false;
	}
	OutFields.Reset();
	for (const auto& Pair : V.as_object())
	{
		OutFields.Add(Utf8ToFString(Pair.first), FromNative(Pair.second));
	}
	return true;
}

FConvexValue FConvexValue::FromWire(const FString& Json, bool& bOutSuccess)
{
	try
	{
		convex::value Decoded = convex::from_wire_json(FStringToUtf8(Json));
		bOutSuccess = true;
		return FromNative(Decoded);
	}
	catch (const std::exception& Error)
	{
		UE_LOG(LogConvex, Error, TEXT("FConvexValue::FromWire failed: %hs"), Error.what());
		bOutSuccess = false;
		return Null();
	}
}

FString FConvexValue::ToWire(bool& bOutSuccess) const
{
	try
	{
		const std::string Json = convex::to_wire_json(GetNative());
		bOutSuccess = true;
		return Utf8ToFString(Json);
	}
	catch (const std::exception& Error)
	{
		UE_LOG(LogConvex, Error, TEXT("FConvexValue::ToWire failed: %hs"), Error.what());
		bOutSuccess = false;
		return FString();
	}
}

convex::value_object ConvexMakeArgs(const TMap<FString, FConvexValue>& Args)
{
	convex::value_object Object;
	for (const auto& Pair : Args)
	{
		Object.emplace(FStringToUtf8(Pair.Key), Pair.Value.GetNative());
	}
	return Object;
}
