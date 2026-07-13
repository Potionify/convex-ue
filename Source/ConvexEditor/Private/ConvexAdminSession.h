// Copyright Potionify. Apache-2.0.

#pragma once

#include "ConvexDelegates.h"
#include "ConvexDeploymentResolver.h"
#include "ConvexValue.h"
#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

class UConvexClient;
class UConvexSubscription;

/// One deployed function, from _system/cli/modules:apiSpec.
struct FConvexFunctionSpec
{
	/// "module/path:function"
	FString Identifier;
	/// Query | Mutation | Action | HttpAction
	FString FunctionType;
	/// public | internal
	FString Visibility;
	/// Args validator (validator-JSON as a Convex value); null kind when absent.
	FConvexValue ArgsValidator;
	/// Returns validator; null kind when absent.
	FConvexValue ReturnsValidator;
	/// HttpAction only: "METHOD /path".
	FString HttpRoute;
};

/// Result of GET /api/check_admin_key.
struct FConvexAdminKeyInfo
{
	bool bChecked = false;
	bool bValid = false;
	bool bIsReadOnly = false;
	/// Operation grants (DeploymentOp names). Empty for a full key on older
	/// backends that return only {success}.
	TArray<FString> AllowedOps;
	FString Error;
};

/**
 * The editor's admin connection to one Convex deployment: resolves the target
 * from env conventions, owns a UConvexClient authenticated with the deploy
 * key, validates the key over HTTP, and maintains live system-query
 * subscriptions (deployment state, function specs).
 *
 * Game-thread only; all UConvexClient/HTTP callbacks already arrive there.
 */
class FConvexAdminSession : public TSharedFromThis<FConvexAdminSession>
{
public:
	~FConvexAdminSession();

	/// Re-resolve the deployment from settings/env and (re)connect.
	void RefreshAndConnect();

	/// Connect to an explicitly resolved target (tests, future manual entry).
	void ConnectWithConfig(const FConvexDeploymentConfig& InConfig);

	/// Drop the client and all subscriptions.
	void Disconnect();

	// ------------------------------------------------------------------
	// State for the UI
	// ------------------------------------------------------------------

	const FConvexDeploymentConfig& GetConfig() const { return Config; }
	const FConvexAdminKeyInfo& GetKeyInfo() const { return KeyInfo; }
	EConvexConnectionState GetConnectionState() const;
	FConvexConnectionInfo GetConnectionInfo() const;
	bool IsConnected() const { return GetConnectionState() == EConvexConnectionState::Connected; }

	/// "running" | "paused" | "disabled" | "suspended" | "" (unknown yet).
	const FString& GetDeploymentState() const { return DeploymentState; }

	/// Convex server version ("x.y.z"), empty until known.
	const FString& GetServerVersion() const { return ServerVersion; }

	const TArray<FConvexFunctionSpec>& GetFunctions() const { return Functions; }

	/// User tables plus browsable system tables, sorted (from getTableMapping).
	const TArray<FString>& GetTableNames() const { return TableNames; }

	/// The active declared schema as stringified SchemaJson; empty when the
	/// deployment has no schema (or none arrived yet).
	const FString& GetActiveSchemaJson() const { return ActiveSchemaJson; }

	/// The connected client, for editor panels that manage their own
	/// subscriptions (data pages, per-table indexes). Null when disconnected.
	UConvexClient* GetClient() const { return Client.Get(); }

	/// One-shot fetch of GET /api/shapes2 (inferred table shapes). OnDone gets
	/// (bSuccess, response JSON text) on the game thread.
	void FetchShapes(TFunction<void(bool, FString)> OnDone);

	/// Writes on prod-typed deployments are the editor's own guard rail (the
	/// server enforces only key permissions, not deployment type).
	bool IsWriteSafeDeployment() const
	{
		return Config.Type == EConvexDeploymentType::Dev ||
			Config.Type == EConvexDeploymentType::Preview;
	}

	/// Full write gate for editor CRUD: dev/preview deployment AND a valid,
	/// non-read-only key that holds WriteData (when the backend reports ops).
	bool CanWrite() const
	{
		return IsWriteSafeDeployment() && KeyInfo.bChecked && KeyInfo.bValid &&
			!KeyInfo.bIsReadOnly &&
			(KeyInfo.AllowedOps.Num() == 0 || KeyInfo.AllowedOps.Contains(TEXT("WriteData")));
	}

	/// Fires after any observable state change (connection, key info, specs,
	/// deployment state). UI repaints from the getters above.
	DECLARE_MULTICAST_DELEGATE(FOnSessionChanged);
	FOnSessionChanged OnChanged;

	// ------------------------------------------------------------------
	// Function runner
	// ------------------------------------------------------------------

	/// Run a deployed function by identifier with Convex-wire-JSON args
	/// ("{}" for none). Queries run as one-shot reads over the sync
	/// connection; mutations/actions as requests. HttpActions are not
	/// runnable. OnResult fires on the game thread exactly once.
	void RunFunction(const FConvexFunctionSpec& Spec, const FString& ArgsWireJson,
		FConvexResultNative OnResult);

private:
	void CheckAdminKey();
	void SubscribeSystemQueries();
	void NotifyChanged();

	FConvexDeploymentConfig Config;
	FConvexAdminKeyInfo KeyInfo;
	FString DeploymentState;
	FString ServerVersion;
	TArray<FConvexFunctionSpec> Functions;
	TArray<FString> TableNames;
	FString ActiveSchemaJson;

	TStrongObjectPtr<UConvexClient> Client;
	TStrongObjectPtr<UConvexSubscription> StateSubscription;
	TStrongObjectPtr<UConvexSubscription> VersionSubscription;
	TStrongObjectPtr<UConvexSubscription> ApiSpecSubscription;
	TStrongObjectPtr<UConvexSubscription> TableMappingSubscription;
	TStrongObjectPtr<UConvexSubscription> SchemaSubscription;
	FDelegateHandle ConnectionStateHandle;

	/// Bumped on every RefreshAndConnect/Disconnect so stale async callbacks
	/// (HTTP, subscriptions of a torn-down client) are ignored.
	uint64 Generation = 0;
};
