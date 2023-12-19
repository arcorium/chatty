#pragma once
#include "simple_server.h"

namespace ar
{
	class ConnectionManager;

	class application
	{
	public:
		explicit application(u16 port_);

		void start();

	private:
		SimpleServer m_server;
	};
}
