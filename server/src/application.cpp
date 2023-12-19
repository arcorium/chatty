﻿#include "application.h"

#include "connection_manager.h"

namespace ar
{
	application::application(u16 port_)
		: m_server{{asio::ip::tcp::v4(), port_}}
	{
	}

	void application::start()
	{
		spdlog::info("Server Started!");
		m_server.start(false);
	}
}
