#pragma once
#include <ranges>
#include <atomic>

#include "connection.h"
#include "user.h"
#include "util/literal.h"

namespace ar
{
	class ConnectionManager : public IConnectionHandler
	{
		using connection_container = std::vector<connection_type*>;
		using user_container = std::unordered_map<connection_type::id_type, User>;
	private:
		ConnectionManager();

	public:
		static ref<ConnectionManager> get();
		~ConnectionManager() override = default;

		void start_validation(Connection<ConnectionType::Server>& conn_) noexcept override;
		void validate(Connection<ConnectionType::Server>& conn_, const Message& msg_) noexcept override;
		void authenticate(Connection<ConnectionType::Server>& conn_, const Message& msg_) noexcept;

		connection_ptr add_connection(asio::ip::tcp::socket&& socket_, ref<IMessageHandler<ConnectionType::Server>> message_handler_) noexcept override;
		void remove_connection(connection_type& conn_) noexcept override;

		void remove_connection(connection_type& conn_, bool reject_) noexcept;

		connection_ptr connection(connection_type::id_type id_) noexcept override;
		std::span<connection_ptr> connections() noexcept override;

		ptr<User> user(connection_type::id_type id_) noexcept;

	private:
		bool is_unique(std::string_view username_) const noexcept;

		template<FeedbackType Type>
		void send_feedback(connection_type& conn_) noexcept;

		template<Serializable T>
		void broadcast(const T& msg_, connection_type::id_type exception_) noexcept;

	private:
		connection_container m_connections;
		user_container m_users;

		static inline std::atomic<connection_type::id_type> s_current_id{};
		constexpr static inline std::string_view KEY = "n1odah10"sv;
	};

	template <FeedbackType Type>
	void ConnectionManager::send_feedback(connection_type& conn_) noexcept
	{
		const FeedbackMessage fed_msg{ Type };
		conn_.send(fed_msg);
	}

	template <Serializable T>
	void ConnectionManager::broadcast(const T& msg_, connection_type::id_type exception_) noexcept
	{
		for (const auto conn : m_connections | std::ranges::views::filter([=](connection_ptr conn_) { return conn_->id() != exception_; }))
		{
			conn->send(msg_);
		}
	}
}
