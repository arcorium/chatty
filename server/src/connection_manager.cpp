#include "connection_manager.h"

#include <ranges>

#include "util/util.h"

#include <spdlog/spdlog.h>

namespace ar
{
	ConnectionManager::ConnectionManager()
	{
	}

	ref<ConnectionManager> ConnectionManager::get()
	{
		static ConnectionManager manager{};
		return manager;
	}

	void ConnectionManager::start_validation(Connection<ConnectionType::Server>& conn_) noexcept
	{
		// Send challenge
		const auto number = generate_random_numbers<usize>();
		m_users[conn_.id()].key = number;
		const ValidationMessage val_msg{number};
		const Message msg{ val_msg };
		conn_.send(msg);

		// Wait for answer
		// conn_.read_timed(std::chrono::milliseconds::zero());
		conn_.read_once();
	}

	void ConnectionManager::validate(Connection<ConnectionType::Server>& conn_, const Message& msg_) noexcept
	{
		auto number = m_users[conn_.id()].key;
		number = encrypt_xor(number, KEY);

		const auto message = msg_.body_as<ValidationMessage>();
		const bool result = message.challenge == number;
		if (!result)
		{
			send_feedback<false>(conn_);
			return;
		}

		send_feedback<true>(conn_);

		// spdlog::info("[{}] Accepted Connection", conn_.id());
		conn_.read_once([this](connection_type& conn_, const Message& msg_)
			{
				authenticate(conn_, msg_);
			});
	}

	void ConnectionManager::authenticate(Connection<ConnectionType::Server>& conn_, const Message& msg_) noexcept
	{
		// Do authentication?
		auto msg = msg_.body_as<AuthenticateMessage>();
		if (!is_unique(msg.username))
		{
			send_feedback<false>(conn_);
			remove_connection(conn_);
			return;
		}
		send_feedback<true>(conn_);

		const auto id = conn_.id();
		spdlog::info("User logged in {}:{}", id, msg.username);

		const NewUserMessage new_user_message{ id, msg.username };
		m_users[id].name = std::move(msg.username);
		m_users[id].public_key = std::move(msg.public_key);

		// Send to all connections that there is new user connected
		for (const auto conn : m_connections | std::ranges::views::filter([=](connection_ptr conn_) { return conn_->id() != id; }))
		{
			conn->send(new_user_message);
		}

		conn_.start();
	}

	ConnectionManager::connection_ptr ConnectionManager::add_connection(asio::ip::tcp::socket&& socket_, ref<IMessageHandler<ConnectionType::Server>> message_handler_) noexcept
	{
		auto temp = new connection_type{ ++s_current_id, std::forward<asio::ip::tcp::socket>(socket_), message_handler_, *this };
		m_connections.emplace_back(temp);
		return temp;
	}

	void ConnectionManager::remove_connection(connection_type& conn_) noexcept
	{
		const auto id = conn_.id();

		// Send to all connections that this id is disconnected
		const UserDisconnectMessage dc_message{ id };
		for(const auto conn : m_connections | std::ranges::views::filter([=](connection_ptr conn_){ return conn_->id() != id; }))
		{
			conn->send(dc_message);
		}

		if (std::erase_if(m_connections, [=](connection_ptr conn2_) { return conn2_->id() == id; }))
		{
			spdlog::info("Client {} disconnected", id);
			m_users.erase(id);
		}
	}

	ConnectionManager::connection_ptr ConnectionManager::connection(connection_type::id_type id_) noexcept
	{
		const auto conn = std::ranges::find_if(m_connections, [&](const connection_ptr conn2_) { return conn2_->id() == id_; });

		if (conn == m_connections.end())
			return nullptr;
		return *conn;
	}

	std::span<ConnectionManager::connection_ptr> ConnectionManager::connections() noexcept
	{
		return m_connections;
	}

	ptr<User> ConnectionManager::user(connection_type::id_type id_) noexcept
	{
		const auto it = m_users.find(id_);
		if (it == m_users.end())
			return {};
		return &it->second;
	}

	bool ConnectionManager::is_unique(std::string_view username_) const noexcept
	{
		const auto it = std::ranges::find_if(m_users, [&](const std::pair<connection_type::id_type, User>& val_)
		{
			return username_ == val_.second.name;
		});

		return it == m_users.end();
	}
}
