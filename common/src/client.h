#pragma once
#include <thread>
#include <asio/ip/tcp.hpp>

#include "Connection.h"
#include "handler.h"
#include "util/types.h"

namespace ar
{
	class IClient : public IMessageHandler<ConnectionType::Client>
	{
	public:
		using connection_type = ClientConnection;

		explicit IClient(const asio::ip::address& address_, u16 port_, IConnectionValidator<ConnectionType::Client>& connection_validator_);
		~IClient() override;

		void on_new_in_message(Connection<ConnectionType::Client>& conn_, const Message& message_) noexcept override {};
		void on_new_out_message(Connection<ConnectionType::Client>& conn_, std::span<const u8> message_) noexcept override {};

		void connect(bool separate_thread_ = true) noexcept;
		void disconnect() noexcept;

		connection_type& connection() noexcept { return m_server_connection; }
		const connection_type& connection() const noexcept { return m_server_connection; }

	protected:
		asio::io_context m_context;
		asio::ip::tcp::endpoint m_endpoint;
		ref<IConnectionValidator<ConnectionType::Client>> m_validator;

	private:
		std::thread m_context_thread;
		ClientConnection m_server_connection;
	};
}
