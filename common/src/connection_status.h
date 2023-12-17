#pragma once

namespace ar
{
	enum class ConnectionType : u8
	{
		Server,
		Client
	};

	enum class ConnectionStatus : u8
	{
		Connecting,		// Client connection start to connect
		Establishing,	// Connection is connected but it's not fully
		Failure,		// Connection is aborted
		Rejected,		// Connection is rejected or refused due to validation error
		Connected,		// Connection is established
		Closed,			// Connection closed
	};
}
