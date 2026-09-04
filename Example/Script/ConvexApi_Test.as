// Script-side checks for the Convex plugin on the Hazelight
// UnrealEngine-Angelscript fork. Runs under the AngelscriptTest commandlet
// (convex-ue-build.bat angelscript) and on every hot reload in the editor.
//
// Compiling this file is most of the test: it references the generated
// wrappers in ConvexApi.as (the local backend's API) and ConvexFixture.as
// (the codegen golden fixture, which declares return shapes the local
// backend does not), plus the plugin's script-callable client methods and
// mixins. A binding that went missing fails the build before any assertion
// runs. The live tests at the bottom talk to the Docker backend at
// 127.0.0.1:3210 and skip themselves when it is not running.

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

    // Typed callbacks from the fixture API.
    UFUNCTION()
    void OnCounter(float Value, FConvexResult Result)
    {
        Results += 1;
    }

    UFUNCTION()
    void OnMessages(TArray<FConvexFixtureMessagesListElement> Value, FConvexResult Result)
    {
        Results += 1;
    }

    UFUNCTION()
    void OnMessagePage(TArray<FConvexFixtureMessagesListElement> Page, FConvexPaginatedSnapshot Snapshot)
    {
        Snapshots += 1;
    }

    UFUNCTION()
    void OnKitchenSink(FConvexFixtureSinkKitchenSinkResult Value, FConvexResult Result)
    {
        Results += 1;
    }

    // Never called: it only has to compile against the generated APIs.
    void ReferenceGeneratedApi(UConvexClient Client)
    {
        FConvexResultDelegate Handler;
        Handler.BindUFunction(this, n"OnResult");

        ConvexApi::Counters::Get(Client, "probe", Handler);
        ConvexApi::Counters::Increment(Client, "probe", Handler);
        ConvexApi::Counters::Increment(Client, Name = "probe", By = 2.0, OnResult = Handler);
        UConvexSubscription Sub = ConvexApi::Counters::WatchGet(Client, "probe", Handler);
        ConvexApi::Actions::Now(Client, Handler);

        // A declared object argument is a struct.
        FConvexApiMessagesListPaginatedPaginationOpts Opts;
        Opts.NumItems = 5;
        Opts.Cursor = Convex::MakeConvexNull();
        ConvexApi::Messages::ListPaginated(Client, "general", Opts, Handler);

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

        // Typed delegates, adapters, and the Types namespace from the fixture.
        FConvexFixtureCountersGetDelegate Counter;
        Counter.BindUFunction(this, n"OnCounter");
        ConvexFixture::Counters::Get(Client, "probe", Counter);
        ConvexFixture::Counters::WatchGet(Client, "probe", Counter);

        FConvexFixtureMessagesListDelegate Messages;
        Messages.BindUFunction(this, n"OnMessages");
        ConvexFixture::Messages::List(Client, "general", Messages);

        FConvexFixtureMessagesListPaginatedPageDelegate Page;
        Page.BindUFunction(this, n"OnMessagePage");
        ConvexFixture::Messages::WatchListPaginatedPaginated(Client, "general", 10, Page);

        FConvexFixtureSinkKitchenSinkDelegate Sink;
        Sink.BindUFunction(this, n"OnKitchenSink");
        FConvexFixtureSinkKitchenSinkNested Nested;
        Nested.Inner = "x";
        Nested.bHasDeep = true;
        Nested.Deep.X = 1.5;
        TArray<FConvexFixtureSinkKitchenSinkItemsElement> Items;
        TArray<uint8> Bytes;
        TArray<int64> Counts;
        TArray<bool> Flags;
        TArray<float> Nums;
        TArray<FString> Tags;
        FConvexValue Null = Convex::MakeConvexNull();
        ConvexFixture::Sink::KitchenSink(Client, Null, true, 1, Bytes, Counts, "doc", 1.0, Flags, 2,
            Items, true, 42.0, "hello", 3.0, Null, Nums, Null, "s", Tags, Null, Nested, Sink);

        // Auth and lifecycle bindings.
        Client.SetUserAuthWithRefreshEvent("jwt");
        Client.OnAuthRefreshRequested.AddUFunction(this, n"OnAuthRefresh");
        Client.Shutdown();
    }

    UFUNCTION()
    void OnAuthRefresh()
    {
    }
}

// ---------------------------------------------------------------------------
// Value library and mixins (no backend)
// ---------------------------------------------------------------------------

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

void Test_ConvexValueMixins(FUnitTest& T)
{
    TMap<FString, FConvexValue> Stats;
    Stats.Add("hp", Convex::MakeConvexFloat(12.5));
    Stats.Add("level", Convex::MakeConvexInt(3));
    TArray<FConvexValue> Tags;
    Tags.Add(Convex::MakeConvexString("a"));
    Tags.Add(Convex::MakeConvexString("b"));
    TMap<FString, FConvexValue> Player;
    Player.Add("name", Convex::MakeConvexString("ada"));
    Player.Add("alive", Convex::MakeConvexBool(true));
    Player.Add("stats", Convex::MakeConvexObject(Stats));
    Player.Add("tags", Convex::MakeConvexArray(Tags));
    FConvexValue Value = Convex::MakeConvexObject(Player);

    // Predicates and kind.
    T.AssertTrue(Value.IsObject());
    T.AssertTrue(Value.Kind() == EConvexValueKind::Object);
    T.AssertTrue(Value.Get("name").IsString());
    T.AssertTrue(Value.Get("missing").IsNull());
    T.AssertTrue(Value.Get("stats").Get("level").IsInt());
    T.AssertTrue(Value.Get("stats").Get("hp").IsNumber());

    // Scalar reads with defaults.
    T.AssertEquals(Value.Get("name").AsString(), "ada");
    T.AssertEquals(Value.Get("missing").AsStringOr("none"), "none");
    T.AssertTrue(Value.Get("alive").AsBool());
    T.AssertEquals(Value.Field("stats.level").AsInt(), 3);
    T.AssertAlmostEquals(Value.Field("stats.hp").AsFloat(), 12.5);
    // Float64 reads as int when whole, Int64 reads as float.
    T.AssertEquals(Convex::MakeConvexFloat(7.0).AsInt(), 7);
    T.AssertEquals(Convex::MakeConvexFloat(7.5).AsIntOr(-1), -1);
    T.AssertAlmostEquals(Convex::MakeConvexInt(9).AsFloat(), 9.0);

    // Navigation.
    T.AssertEquals(Value.Get("tags").Length(), 2);
    T.AssertEquals(Value.Get("tags").At(1).AsString(), "b");
    T.AssertEquals(Value.Field("tags.0").AsString(), "a");
    T.AssertTrue(Value.Field("tags.5").IsNull());
    T.AssertTrue(Value.HasField("stats"));
    T.AssertFalse(Value.HasField("stats.hp"));
    T.AssertEquals(Value.Keys().Num(), 4);
    T.AssertEquals(Value.Keys()[0], "alive");
    T.AssertEquals(Value.Length(), 4);
    T.AssertEquals(Value.Get("name").Length(), 0);
    T.AssertEquals(Value.Get("tags").AsArray().Num(), 2);
    T.AssertEquals(Value.Get("stats").AsObject().Num(), 2);
    T.AssertEquals(Convex::MakeConvexString("x").ToJson(), "\"x\"");
}

void Test_ConvexTypesDecodeEncode(FUnitTest& T)
{
    // Round-trip a fixture struct through the generated Types namespace.
    FConvexFixtureMessagesListElement Message;
    Message.Id = "abc";
    Message.CreationTime = 1.0;
    Message.Author = "ada";
    Message.Body = "hi";
    Message.Channel = "general";
    FConvexValue Wire = ConvexFixture::Types::Encode(Message);
    T.AssertEquals(Wire.Get("author").AsString(), "ada");
    T.AssertFalse(Wire.HasField("edited"));

    FConvexFixtureMessagesListElement Back = ConvexFixture::Types::DecodeMessagesListElement(Wire);
    T.AssertEquals(Back.Id, "abc");
    T.AssertEquals(Back.Body, "hi");
    T.AssertFalse(Back.bHasEdited);

    Message.bHasEdited = true;
    Message.Edited = true;
    Back = ConvexFixture::Types::DecodeMessagesListElement(ConvexFixture::Types::Encode(Message));
    T.AssertTrue(Back.bHasEdited);
    T.AssertTrue(Back.Edited);

    // Nested optional struct.
    FConvexFixtureSinkKitchenSinkNested Nested;
    Nested.Inner = "in";
    FConvexValue NestedWire = ConvexFixture::Types::Encode(Nested);
    T.AssertFalse(NestedWire.HasField("deep"));
    Nested.bHasDeep = true;
    Nested.Deep.X = 2.5;
    FConvexFixtureSinkKitchenSinkNested NestedBack =
        ConvexFixture::Types::DecodeSinkKitchenSinkNested(ConvexFixture::Types::Encode(Nested));
    T.AssertTrue(NestedBack.bHasDeep);
    T.AssertAlmostEquals(NestedBack.Deep.X, 2.5);
}

void Test_ConvexClientFromScript(FUnitTest& T)
{
    // A client the script created, not the subsystem default, so the test
    // needs no deployment URL in settings and never opens a connection.
    UConvexClient Client = Cast<UConvexClient>(NewObject(nullptr, UConvexClient));
    T.AssertNotNull(Client);
    T.AssertFalse(Client.IsInitialized());
    T.AssertTrue(Client.GetConnectionState() == EConvexConnectionState::Disconnected);

    // Initialize is script-callable; a malformed URL fails and stays retryable.
    Client.Initialize("not a url");
    T.AssertTrue(Client.HasInitializationFailed());
    T.AssertFalse(Client.IsInitialized());
    Client.Shutdown();
}

// ---------------------------------------------------------------------------
// Live tests against the local Docker backend
// ---------------------------------------------------------------------------

class UConvexLiveProbe : UObject
{
    bool bDone = false;
    FConvexResult Last;
    float Counter = 0.0;
    bool bCounterDone = false;
    TArray<FConvexFixtureMessagesListElement> Messages;
    bool bMessagesDone = false;

    UFUNCTION()
    void OnResult(FConvexResult Result)
    {
        Last = Result;
        bDone = true;
    }

    UFUNCTION()
    void OnCounter(float Value, FConvexResult Result)
    {
        Counter = Value;
        Last = Result;
        bCounterDone = true;
    }

    UFUNCTION()
    void OnMessages(TArray<FConvexFixtureMessagesListElement> Value, FConvexResult Result)
    {
        Messages = Value;
        Last = Result;
        bMessagesDone = true;
    }
}

/// Connect a fresh client to the local backend; null when it is not running.
UConvexClient ConnectLiveClient(FUnitTest& T)
{
    UConvexClient Client = Cast<UConvexClient>(NewObject(nullptr, UConvexClient));
    Client.Initialize("http://127.0.0.1:3210");
    for (int i = 0; i < 30 && Client.GetConnectionState() != EConvexConnectionState::Connected; ++i)
    {
        ConvexExampleTest::Pump(0.1);
    }
    if (Client.GetConnectionState() != EConvexConnectionState::Connected)
    {
        Log("Skipping live script test: no Convex backend at 127.0.0.1:3210");
        Client.Shutdown();
        return nullptr;
    }
    return Client;
}

void Test_ConvexLiveCounterFromScript(FUnitTest& T)
{
    UConvexClient Client = ConnectLiveClient(T);
    if (Client == nullptr)
    {
        return;
    }
    UConvexLiveProbe Probe = Cast<UConvexLiveProbe>(NewObject(nullptr, UConvexLiveProbe));
    FString Name = "script-" + Math::RandRange(0, 1000000000);

    // A mutation through the generated wrapper, read back with the mixins.
    FConvexResultDelegate Handler;
    Handler.BindUFunction(Probe, n"OnResult");
    ConvexApi::Counters::Increment(Client, Name = Name, By = 3.0, OnResult = Handler);
    for (int i = 0; i < 50 && !Probe.bDone; ++i)
    {
        ConvexExampleTest::Pump(0.1);
    }
    T.AssertTrue(Probe.bDone, "increment callback fired");
    T.AssertTrue(Probe.Last.bSuccess, Probe.Last.Describe());

    Probe.bDone = false;
    ConvexApi::Counters::Get(Client, Name, Handler);
    for (int i = 0; i < 50 && !Probe.bDone; ++i)
    {
        ConvexExampleTest::Pump(0.1);
    }
    T.AssertTrue(Probe.bDone, "get callback fired");
    T.AssertTrue(Probe.Last.bSuccess, Probe.Last.Describe());
    T.AssertAlmostEquals(Probe.Last.Value.AsFloat(), 3.0);

    // The same query through a typed delegate and its generated adapter,
    // which the client keeps alive until the callback fires.
    FConvexFixtureCountersGetDelegate Typed;
    Typed.BindUFunction(Probe, n"OnCounter");
    ConvexFixture::Counters::Get(Client, Name, Typed);
    for (int i = 0; i < 50 && !Probe.bCounterDone; ++i)
    {
        ConvexExampleTest::Pump(0.1);
    }
    T.AssertTrue(Probe.bCounterDone, "typed get callback fired");
    T.AssertAlmostEquals(Probe.Counter, 3.0);

    Client.Shutdown();
}

void Test_ConvexLiveMessagesTypedFromScript(FUnitTest& T)
{
    UConvexClient Client = ConnectLiveClient(T);
    if (Client == nullptr)
    {
        return;
    }
    UConvexLiveProbe Probe = Cast<UConvexLiveProbe>(NewObject(nullptr, UConvexLiveProbe));
    FString Channel = "script-" + Math::RandRange(0, 1000000000);

    FConvexResultDelegate Handler;
    Handler.BindUFunction(Probe, n"OnResult");
    ConvexApi::Messages::Send(Client, "ada", "hello from script", Channel, Handler);
    for (int i = 0; i < 50 && !Probe.bDone; ++i)
    {
        ConvexExampleTest::Pump(0.1);
    }
    T.AssertTrue(Probe.bDone, "send callback fired");
    T.AssertTrue(Probe.Last.bSuccess, Probe.Last.Describe());

    // The fixture declares messages:list as array<doc>; the local backend's
    // documents carry those fields, so the typed wrapper decodes them.
    FConvexFixtureMessagesListDelegate Typed;
    Typed.BindUFunction(Probe, n"OnMessages");
    ConvexFixture::Messages::List(Client, Channel, Typed);
    for (int i = 0; i < 50 && !Probe.bMessagesDone; ++i)
    {
        ConvexExampleTest::Pump(0.1);
    }
    T.AssertTrue(Probe.bMessagesDone, "typed list callback fired");
    T.AssertTrue(Probe.Last.bSuccess, Probe.Last.Describe());
    T.AssertEquals(Probe.Messages.Num(), 1);
    T.AssertEquals(Probe.Messages[0].Author, "ada");
    T.AssertEquals(Probe.Messages[0].Body, "hello from script");
    T.AssertEquals(Probe.Messages[0].Channel, Channel);
    T.AssertFalse(Probe.Messages[0].Id.IsEmpty());
    T.AssertFalse(Probe.Messages[0].bHasEdited);

    Client.Shutdown();
}
