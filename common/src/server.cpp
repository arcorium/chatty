#include "server.h"

#include <ranges>
#include <spdlog/spdlog.h>

#include "Connection.h"
#include "util/literal.h"
#include "util/util.h"


namespace ar
{
	IServer::IServer(const asio::ip::tcp::endpoint& endpoint_, IConnectionHandler& conn_handler_,
		int concurrency_hint_)
		: m_context{ concurrency_hint_ }, m_connection_handler{conn_handler_}, m_acceptor{ m_context, endpoint_ }
	{
		handle_accept();
	}

	void IServer::start(bool separate_thread_) noexcept
	{
		if (!separate_thread_)
		{
			m_context.run();
			return;
		}

		m_context_thread = std::thread{ [this] { m_context.run(); } };
	}

	void IServer::stop() noexcept
	{
		if (!m_context.stopped())
			m_context.stop();

		if (m_context_thread.joinable())
			m_context_thread.join();
	}

	void IServer::broadcast(connection_type::id_type sender_id_, const Message& message_) noexcept
	{
		auto connections = m_connection_handler->connections();
		for (auto conn: connections /*| std::ranges::views::filter([id = sender_id_](const connection_type& conn){ return conn.id() != id; })*/)
			conn->send(message_);
	}

	void IServer::handle_accept() noexcept
	{
		m_acceptor.async_accept([&](const asio::error_code& ec_, asio::ip::tcp::socket&& socket_)
			{
				if (ec_)
				{
					spdlog::warn("Accept Connection Error: {}", ec_.message());
					return;
				}

				auto conn = m_connection_handler->add_connection(std::forward<asio::ip::tcp::socket>(socket_), *this);
				
				if (!on_new_connection(*conn))
				{
					m_connection_handler->remove_connection(*conn);
					handle_accept();
					return;
				}
				m_connection_handler->start_validation(*conn);

				handle_accept();
			});
	}
}
