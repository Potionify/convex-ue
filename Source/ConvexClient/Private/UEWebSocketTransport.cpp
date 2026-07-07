// Copyright Potionify. Apache-2.0.

#include "UEWebSocketTransport.h"

#include "ConvexClientModule.h"
#include "ConvexUtils.h"

#include "IWebSocket.h"
#include "WebSocketsModule.h"

#include <exception>
#include <string>
#include <utility>

using ConvexUtils::FStringToUtf8;
using ConvexUtils::RunOnGameThread;
using ConvexUtils::Utf8ToFString;

namespace
{
	/**
	 * Shared, thread-safe state for one websocket connection. Referenced by the
	 * connection object (on the client worker thread) and by the IWebSocket
	 * delegate lambdas (game thread).
	 *
	 * Two independent locks avoid a lock-order inversion with the convex
	 * client's internal mutex:
	 *   - ObserverGuard is held while forwarding to the observer. Because the
	 *     observer re-enters the client's lock, this lock must NOT be taken on
	 *     the send path (send_text runs while the client already holds its
	 *     lock). The connection destructor takes ObserverGuard to set bDead,
	 *     which blocks until any in-flight forward finishes -> the "no callback
	 *     after destruction" guarantee.
	 *   - SocketGuard protects the IWebSocket handle and the pending-send queue.
	 */
	struct FConvexWebSocketState : public TSharedFromThis<FConvexWebSocketState, ESPMode::ThreadSafe>
	{
		// --- Observer forwarding (game thread) -------------------------------
		FCriticalSection ObserverGuard;
		convex::websocket_observer* Observer = nullptr;
		bool bDead = false;     // connection destroyed; suppress all forwarding
		bool bClosed = false;   // on_close already delivered (terminal)

		// --- Socket + send queue (game thread for IWebSocket ops) ------------
		FCriticalSection SocketGuard;
		TSharedPtr<IWebSocket> Socket;
		bool bOpen = false;
		TArray<std::string> PendingText;

		void ForwardOpen()
		{
			FScopeLock Lock(&ObserverGuard);
			if (bDead || bClosed || Observer == nullptr)
			{
				return;
			}
			Observer->on_open();
		}

		void ForwardMessage(std::string Text)
		{
			FScopeLock Lock(&ObserverGuard);
			if (bDead || bClosed || Observer == nullptr)
			{
				return;
			}
			Observer->on_message(std::move(Text));
		}

		void ForwardClose(std::string Reason)
		{
			FScopeLock Lock(&ObserverGuard);
			if (bDead || bClosed || Observer == nullptr)
			{
				return;
			}
			bClosed = true;
			Observer->on_close(std::move(Reason));
		}

		// Game thread: flush queued frames if the socket is open.
		void DrainSends()
		{
			FScopeLock Lock(&SocketGuard);
			if (!bOpen || !Socket.IsValid())
			{
				return;
			}
			for (const std::string& Frame : PendingText)
			{
				Socket->Send(Utf8ToFString(Frame));
			}
			PendingText.Reset();
		}
	};

	/// convex::websocket_connection backed by the shared state above.
	class FConvexWebSocketConnection : public convex::websocket_connection
	{
	public:
		explicit FConvexWebSocketConnection(TSharedRef<FConvexWebSocketState, ESPMode::ThreadSafe> InState)
			: State(MoveTemp(InState))
		{
		}

		virtual ~FConvexWebSocketConnection() override
		{
			// Mark dead first: taking ObserverGuard blocks until any in-flight
			// forward completes, and thereafter every forwarder early-returns.
			// This synchronously satisfies the transport contract's safety
			// property ("no further observer callbacks after destruction").
			{
				FScopeLock Lock(&State->ObserverGuard);
				State->bDead = true;
			}

			// The physical Close() below is deliberately queued rather than
			// awaited: IWebSocket is game-thread-affine, and this destructor
			// runs on the convex worker during reconnects but on the game
			// thread during client shutdown — a blocking wait here would
			// self-deadlock in the latter case. The convex client tolerates a
			// briefly-lingering old socket (its connection-generation guard
			// discards anything the socket might still do server-side).
			// Close the socket and drop delegates on the game thread.
			TSharedRef<FConvexWebSocketState, ESPMode::ThreadSafe> CapturedState = State;
			RunOnGameThread([CapturedState]()
			{
				FScopeLock Lock(&CapturedState->SocketGuard);
				CapturedState->bOpen = false;
				if (CapturedState->Socket.IsValid())
				{
					TSharedPtr<IWebSocket> Socket = CapturedState->Socket;
					Socket->OnConnected().Clear();
					Socket->OnConnectionError().Clear();
					Socket->OnClosed().Clear();
					Socket->OnMessage().Clear();
					Socket->OnRawMessage().Clear();
					Socket->OnMessageSent().Clear();
					if (Socket->IsConnected())
					{
						Socket->Close();
					}
					CapturedState->Socket.Reset();
				}
			});
		}

		virtual void send_text(std::string text) override
		{
			TSharedRef<FConvexWebSocketState, ESPMode::ThreadSafe> CapturedState = State;
			{
				FScopeLock Lock(&CapturedState->SocketGuard);
				CapturedState->PendingText.Add(std::move(text));
			}
			RunOnGameThread([CapturedState]() { CapturedState->DrainSends(); });
		}

	private:
		TSharedRef<FConvexWebSocketState, ESPMode::ThreadSafe> State;
	};
}

std::unique_ptr<convex::websocket_connection> FUEWebSocketTransport::connect(
	const std::string& url,
	const std::map<std::string, std::string>& headers,
	convex::websocket_observer& observer)
{
	TSharedRef<FConvexWebSocketState, ESPMode::ThreadSafe> State =
		MakeShared<FConvexWebSocketState, ESPMode::ThreadSafe>();
	State->Observer = &observer;

	const FString Url = Utf8ToFString(url);
	TMap<FString, FString> HeaderMap;
	for (const auto& Header : headers)
	{
		HeaderMap.Add(Utf8ToFString(Header.first), Utf8ToFString(Header.second));
	}

	RunOnGameThread([State, Url, HeaderMap]()
	{
		FWebSocketsModule& Module = FModuleManager::LoadModuleChecked<FWebSocketsModule>(TEXT("WebSockets"));
		TSharedPtr<IWebSocket> Socket = Module.CreateWebSocket(Url, TEXT(""), HeaderMap);
		if (!Socket.IsValid())
		{
			State->ForwardClose(std::string("ConnectFailed: CreateWebSocket returned null"));
			return;
		}

		TWeakPtr<FConvexWebSocketState, ESPMode::ThreadSafe> WeakState = State;

		Socket->OnConnected().AddLambda([WeakState]()
		{
			if (TSharedPtr<FConvexWebSocketState, ESPMode::ThreadSafe> Pinned = WeakState.Pin())
			{
				{
					FScopeLock Lock(&Pinned->SocketGuard);
					Pinned->bOpen = true;
				}
				// Deliver on_open BEFORE draining: the client's on_open handler
				// enqueues the Connect frame via send_text, which then drains.
				Pinned->ForwardOpen();
				Pinned->DrainSends();
			}
		});

		Socket->OnMessage().AddLambda([WeakState](const FString& Message)
		{
			if (TSharedPtr<FConvexWebSocketState, ESPMode::ThreadSafe> Pinned = WeakState.Pin())
			{
				Pinned->ForwardMessage(FStringToUtf8(Message));
			}
		});

		Socket->OnClosed().AddLambda([WeakState](int32 StatusCode, const FString& Reason, bool /*bWasClean*/)
		{
			if (TSharedPtr<FConvexWebSocketState, ESPMode::ThreadSafe> Pinned = WeakState.Pin())
			{
				{
					FScopeLock Lock(&Pinned->SocketGuard);
					Pinned->bOpen = false;
				}
				const FString Text = Reason.IsEmpty() ? FString::Printf(TEXT("Closed(%d)"), StatusCode) : Reason;
				Pinned->ForwardClose(FStringToUtf8(Text));
			}
		});

		Socket->OnConnectionError().AddLambda([WeakState](const FString& Error)
		{
			if (TSharedPtr<FConvexWebSocketState, ESPMode::ThreadSafe> Pinned = WeakState.Pin())
			{
				{
					FScopeLock Lock(&Pinned->SocketGuard);
					Pinned->bOpen = false;
				}
				const FString Text = Error.IsEmpty() ? TEXT("ConnectionError") : Error;
				Pinned->ForwardClose(FStringToUtf8(Text));
			}
		});

		{
			FScopeLock Lock(&State->SocketGuard);
			if (State->bDead)
			{
				// Destroyed before setup completed; never touch the socket.
				return;
			}
			State->Socket = Socket;
		}
		Socket->Connect();
	});

	return std::make_unique<FConvexWebSocketConnection>(State);
}
