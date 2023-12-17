#pragma once
#include <thread>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include "Connection.h"
#include "util/literal.h"
#include "message/command.h"

namespace ar
{
	class IServer : public IMessageHandler<ConnectionType::Server>
	{
	public:
		using connection_type = ServerConnection;

		explicit IServer(const asio::ip::tcp::endpoint& endpoint_, IConnectionHandler& conn_handler_, int concurrency_hint_ = std::thread::hardware_concurrency());
		~IServer() override = default;

		void start(bool separate_thread_ = true) noexcept;
		void stop() noexcept;

		void broadcast(connection_type::id_type sender_id_, const Message& message_) noexcept;

	protected:
		virtual bool on_new_connection(connection_type& conn_) noexcept { return true; }

	private:
		void handle_accept() noexcept;

	protected:
		asio::io_context m_context;
		ref<IConnectionHandler> m_connection_handler;

	private:
		std::thread m_context_thread;
		asio::ip::tcp::acceptor m_acceptor;
	};
}
