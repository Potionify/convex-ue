// Copyright Potionify. Apache-2.0.

#include "ConvexEditorJson.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	using FPrettyWriter = TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>;
	using FPrettyWriterFactory = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>;

	/// Default JSON value for one validator node. Depth-capped: validators are
	/// finite, but unions of objects can nest.
	TSharedPtr<FJsonValue> DefaultForValidator(const FConvexValue& Validator, int32 Depth)
	{
		if (Depth > 8)
		{
			return MakeShared<FJsonValueNull>();
		}

		TMap<FString, FConvexValue> Fields;
		if (!Validator.TryGetObject(Fields))
		{
			return MakeShared<FJsonValueNull>();
		}
		const FConvexValue* TypeValue = Fields.Find(TEXT("type"));
		FString Type;
		if (TypeValue == nullptr || !TypeValue->TryGetString(Type))
		{
			return MakeShared<FJsonValueNull>();
		}

		if (Type == TEXT("string")) return MakeShared<FJsonValueString>(TEXT(""));
		if (Type == TEXT("number") || Type == TEXT("float64"))
		{
			return MakeShared<FJsonValueNumber>(0.0);
		}
		if (Type == TEXT("int64") || Type == TEXT("bigint"))
		{
			// Wire form for int64 is {"$integer": <base64>}; 0 keeps the seed
			// readable and functions declaring v.int64 will reject it with a
			// clear validator message if the user leaves it as a float.
			return MakeShared<FJsonValueNumber>(0.0);
		}
		if (Type == TEXT("boolean")) return MakeShared<FJsonValueBoolean>(false);
		if (Type == TEXT("null")) return MakeShared<FJsonValueNull>();
		if (Type == TEXT("id")) return MakeShared<FJsonValueString>(TEXT(""));
		if (Type == TEXT("bytes"))
		{
			const TSharedPtr<FJsonObject> Bytes = MakeShared<FJsonObject>();
			Bytes->SetStringField(TEXT("$bytes"), TEXT(""));
			return MakeShared<FJsonValueObject>(Bytes);
		}
		if (Type == TEXT("array"))
		{
			return MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{});
		}
		if (Type == TEXT("record"))
		{
			return MakeShared<FJsonValueObject>(MakeShared<FJsonObject>());
		}
		if (Type == TEXT("literal"))
		{
			// {type:"literal", value:<constant>}
			if (const FConvexValue* Literal = Fields.Find(TEXT("value")))
			{
				FString S;
				bool B = false;
				int64 I = 0;
				double D = 0.0;
				if (Literal->TryGetString(S)) return MakeShared<FJsonValueString>(S);
				if (Literal->TryGetBool(B)) return MakeShared<FJsonValueBoolean>(B);
				if (Literal->TryGetInt64(I))
				{
					return MakeShared<FJsonValueNumber>(static_cast<double>(I));
				}
				if (Literal->TryGetFloat(D)) return MakeShared<FJsonValueNumber>(D);
			}
			return MakeShared<FJsonValueNull>();
		}
		if (Type == TEXT("union"))
		{
			// {type:"union", value:[<validator>...]} -> first member's default.
			if (const FConvexValue* Members = Fields.Find(TEXT("value")))
			{
				TArray<FConvexValue> Array;
				if (Members->TryGetArray(Array) && Array.Num() > 0)
				{
					return DefaultForValidator(Array[0], Depth + 1);
				}
			}
			return MakeShared<FJsonValueNull>();
		}
		if (Type == TEXT("object"))
		{
			// {type:"object", value:{field:{fieldType:<validator>, optional}}}
			const TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
			if (const FConvexValue* Shape = Fields.Find(TEXT("value")))
			{
				TMap<FString, FConvexValue> ShapeFields;
				if (Shape->TryGetObject(ShapeFields))
				{
					ShapeFields.KeySort(TLess<FString>());
					for (const TPair<FString, FConvexValue>& Field : ShapeFields)
					{
						TMap<FString, FConvexValue> FieldSpec;
						if (!Field.Value.TryGetObject(FieldSpec))
						{
							continue;
						}
						bool bOptional = false;
						if (const FConvexValue* Optional = FieldSpec.Find(TEXT("optional")))
						{
							Optional->TryGetBool(bOptional);
						}
						if (bOptional)
						{
							continue;
						}
						const FConvexValue* FieldType = FieldSpec.Find(TEXT("fieldType"));
						Object->SetField(Field.Key, FieldType != nullptr
														? DefaultForValidator(*FieldType, Depth + 1)
														: MakeShared<FJsonValueNull>());
					}
				}
			}
			return MakeShared<FJsonValueObject>(Object);
		}
		// "any" and anything unrecognized.
		return MakeShared<FJsonValueNull>();
	}

	FString SerializeValuePretty(const TSharedPtr<FJsonValue>& Value)
	{
		FString Out;
		const TSharedRef<FPrettyWriter> Writer = FPrettyWriterFactory::Create(&Out);
		switch (Value->Type)
		{
			case EJson::Object:
				FJsonSerializer::Serialize(Value->AsObject().ToSharedRef(), Writer);
				break;
			case EJson::Array:
				FJsonSerializer::Serialize(Value->AsArray(), Writer);
				break;
			default:
				// Scalars are already single-line; serialize via a wrapper
				// array and strip it to keep one code path out of the API.
				return FString();
		}
		Writer->Close();
		return Out;
	}
}

namespace ConvexEditorJson
{

FString PrettyPrint(const FString& WireJson)
{
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(WireJson);
	TSharedPtr<FJsonValue> Value;
	if (!FJsonSerializer::Deserialize(Reader, Value) || !Value.IsValid())
	{
		return WireJson;
	}
	if (Value->Type != EJson::Object && Value->Type != EJson::Array)
	{
		return WireJson.TrimStartAndEnd();  // scalar: already display-ready
	}
	const FString Pretty = SerializeValuePretty(Value);
	return Pretty.IsEmpty() ? WireJson : Pretty;
}

FString SeedArgsFromValidator(const FConvexValue& ArgsValidator)
{
	const TSharedPtr<FJsonValue> Default = DefaultForValidator(ArgsValidator, 0);
	if (Default->Type != EJson::Object)
	{
		return TEXT("{}");
	}
	const FString Pretty = SerializeValuePretty(Default);
	return Pretty.IsEmpty() ? TEXT("{}") : Pretty;
}

}  // namespace ConvexEditorJson
