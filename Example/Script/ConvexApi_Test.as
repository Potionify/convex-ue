// Script-side checks for the Convex plugin on the Hazelight
// UnrealEngine-Angelscript fork. Runs under the AngelscriptTest commandlet
// (convex-ue-build.bat angelscript) and on every hot reload in the editor.
//
// Compiling this file is most of the test: it references the generated
// wrappers in ConvexApi.as and the plugin's script-callable client methods,
// so a binding that went missing fails the build before any assertion runs.

class UConvexScriptProbe : UObject
{
    int Results = 0;
    int Snapshots = 0;

    UFUNCTION()
    void OnResult(FConvexResult Result)
    {
        Results += 1;
    }

    UFUNCTION()
    void OnSnapshot(FConvexPaginatedSnapshot Snapshot)
    {
        Snapshots += 1;
    }

    // Never called: it only has to compile against the generated API.
    void ReferenceGeneratedApi(UConvexClient Client)
    {
        FConvexResultDelegate Handler;
        Handler.BindUFunction(this, n"OnResult");

        ConvexApi::Counters::Get(Client, "probe", Handler);
        ConvexApi::Counters::Increment(Client, "probe", Handler);
        ConvexApi::Counters::Increment(Client, Name = "probe", By = 2.0, OnResult = Handler);
        UConvexSubscription Sub = ConvexApi::Counters::WatchGet(Client, "probe", Handler);
        ConvexApi::Actions::Now(Client, Handler);

        // The generic path the wrappers are built on.
        TMap<FString, FConvexValue> Args;
        Args.Add("name", Convex::MakeConvexString("probe"));
        Client.Query("counters:get", Args, Handler);
        Client.Mutation("counters:increment", Args, Handler);
        Client.Action("actions:now", Args, Handler);
        Client.HttpQuery("counters:get", Args, Handler);
        Client.HttpMutation("counters:increment", Args, Handler);
        Client.HttpAction("actions:now", Args, Handler);
        Client.Subscribe("counters:get", Args, Handler);

        FConvexPaginatedSnapshotDelegate Snap;
        Snap.BindUFunction(this, n"OnSnapshot");
        Client.SubscribePaginated("messages:list", Args, 10, Snap);
    }
}

void Test_ConvexValueLibrary(FUnitTest& T)
{
    bool bOk = false;
    FConvexValue Text = Convex::MakeConvexString("hello");
    T.AssertEquals(Convex::GetStringValue(Text, bOk), "hello");
    T.AssertTrue(bOk);

    FConvexValue Number = Convex::MakeConvexInt(42);
    T.AssertEquals(Convex::GetIntValue(Number, bOk), 42);
    T.AssertTrue(bOk);

    TMap<FString, FConvexValue> Fields;
    Fields.Add("text", Text);
    Fields.Add("number", Number);
    FConvexValue Object = Convex::MakeConvexObject(Fields);
    T.AssertTrue(Convex::GetValueKind(Object) == EConvexValueKind::Object);
}

void Test_ConvexClientFromScript(FUnitTest& T)
{
    // A client the script created, not the subsystem default, so the test
    // needs no deployment URL in settings and never opens a connection.
    UConvexClient Client = Cast<UConvexClient>(NewObject(nullptr, UConvexClient));
    T.AssertNotNull(Client);
    T.AssertFalse(Client.IsInitialized());
    T.AssertTrue(Client.GetConnectionState() == EConvexConnectionState::Disconnected);
}
