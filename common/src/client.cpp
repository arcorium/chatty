#include "client.h"

#include <asio/connect.hpp>
#include <spdlog/spdlog.h>
#include "util/util.h"

namespace ar
{
	IClient::IClient(const asio::ip::address& address_, u16 port_, IConnectionValidator<ConnectionType::Client>& connection_validator_)
		: m_endpoint{address_, port_}, m_validator{connection_validator_}, m_server_connection{ m_context, *this, m_validator}
	{
	}

	IClient::~IClient()
	{
	}

	void IClient::connect(bool separate_thread_) noexcept
	{
		m_server_connection.connect(m_endpoint);

		if (!separate_thread_)
		{
			m_context.run();
			return;
		}

		m_context_thread = std::thread{ [this] { m_context.run(); } };
	}

	void IClient::disconnect() noexcept
	{
		m_server_connection.disconnect();

		if (!m_context.stopped())
			m_context.stop();

		if (m_context_thread.joinable())
			m_context_thread.join();
	}
}
