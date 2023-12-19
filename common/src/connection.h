#pragma once
#include <queue>
#include <span>
#include <asio.hpp>

#include "util/types.h"
#include "message/message.h"
#include "handler.h"
#include "util/pointer.h"
#include "util/asio.h"

#include <memory>
#include <spdlog/spdlog.h>

namespace ar
{

	template<ConnectionType Owner>
	class Connection;

	template<ConnectionType Owner>
	constexpr void empty_timeout_callback(Connection<Owner>&) {}

	template<ConnectionType Owner>
	constexpr void empty_complete_callback(Connection<Owner>&, const Message&) {}

	template<>
	class Connection<ConnectionType::Server>
	{
	public:
		using id_type = u32;
		using socket_type = asio::ip::tcp::socket;

		Connection(id_type id_, asio::ip::tcp::socket&& socket_, IMessageHandler<ConnectionType::Server>& msg_handler_, IConnectionHandler& conn_handler_)
			: m_on_writing{ false },
			  m_read_once_timer{std::make_unique<asio::steady_timer>(socket_.get_executor())}, m_id{id_},
			  m_message_handler{msg_handler_}, m_connection_handler{conn_handler_}, m_header_input_buffer{},
			  m_socket{std::forward<socket_type>(socket_)}
		{
		}

		Connection(const Connection& other) = delete;
		Connection& operator=(const Connection& other) = delete;

		Connection(Connection&& other) noexcept
			: m_on_writing(other.m_on_writing),
			  m_read_once_timer{std::move(other.m_read_once_timer)},
			  m_id(other.m_id),
			  m_message_handler(other.m_message_handler),
			  m_connection_handler(other.m_connection_handler),
			  m_header_input_buffer(std::move(other.m_header_input_buffer)),
			  m_out_messages(std::move(other.m_out_messages)),
			  m_input_message(std::move(other.m_input_message)),
			m_socket(std::move(other.m_socket))
		{
		}

		Connection& operator=(Connection&& other) noexcept
		{
			if (this == &other)
				return *this;
			m_on_writing = other.m_on_writing;
			m_id = other.m_id;
			m_message_handler = other.m_message_handler;
			m_connection_handler = other.m_connection_handler;
			m_header_input_buffer = std::move(other.m_header_input_buffer);
			m_out_messages = std::move(other.m_out_messages);
			m_input_message = std::move(other.m_input_message);
			m_socket = std::move(other.m_socket);
			m_read_once_timer = std::move(other.m_read_once_timer);

			other.m_id = 0;
			return *this;
		}

		~Connection() noexcept
		{
			close();
		}

		void start() noexcept
		{
			read_header<true>();
		}

		void close() noexcept
		{
			if (!is_connected())
				return;
			m_connection_handler->remove_connection(*this);
			m_socket.close();
		}

		/**
		 * \brief do async_read only once
		 */
		template<std::invocable<Connection&> F = decltype(empty_timeout_callback<ConnectionType::Server>), std::invocable<Connection&, const Message&> F1 = decltype(empty_complete_callback<ConnectionType::Server>)>
		void read_timed(const std::chrono::milliseconds& timeout_, F1&& complete_callback_ = empty_complete_callback<ConnectionType::Server>, F&& timeout_callback_ = empty_timeout_callback<ConnectionType::Server>) noexcept
		{
			constexpr bool has_complete_handler = !std::is_same_v<F1, decltype(empty_complete_callback<ConnectionType::Server>)>;
			constexpr bool has_timeout_handler = !std::is_same_v<F, decltype(empty_timeout_callback<ConnectionType::Server>)>;
			read_header<false, true, !has_complete_handler>();

			if (timeout_ != std::chrono::milliseconds::zero())
			{
				m_read_once_timer->expires_after(timeout_);
				m_read_once_timer->async_wait([cf = std::forward<F1>(complete_callback_), tf = std::forward<F>(timeout_callback_), this](const asio::error_code& ec_)
				{
						// Timer callback only got called when the timer is expired
						if (!ec_)
						{
							if constexpr (has_timeout_handler)
								std::invoke(tf, *this);
						}
						// 
						else
						{
							if constexpr (has_complete_handler)
								std::invoke(cf, *this, m_input_message);
						}
				});
			}
		}

		/**
		 * \brief do async_read only once, commonly used for handshake protocol
		 */
		template<std::invocable<Connection&, const Message&> F = decltype(empty_complete_callback<ConnectionType::Server>)>
		void read_once(F&& complete_callback_ = empty_complete_callback<ConnectionType::Server>) noexcept
		{
			constexpr bool has_complete_handler = !std::is_same_v<F, decltype(empty_complete_callback<ConnectionType::Server>)>;
			read_header<false, false, !has_complete_handler>();

			// if (m_read_once_timer->expiry() > asio::steady_timer::clock_type::now())
			m_read_once_timer->expires_after(10000s);
			m_read_once_timer->async_wait([cf = std::forward<F>(complete_callback_), this](const asio::error_code& ec_)
				{
					// callback will be called when the timer is cancelled, which will be on message_handler
					if (ec_)
					{
						if constexpr (has_complete_handler)
							std::invoke(cf, *this, m_input_message);
					}
				});
		}

		void send(const Message& msg_)
		{
			send(msg_.serialize());
		}

		template<Serializable T>
		void send(const T& msg_) noexcept {
			send(Message{msg_});
		}

		id_type id() const noexcept { return m_id; }
		socket_type& socket() noexcept { return m_socket; }
		const socket_type& socket() const noexcept { return m_socket; }
		bool is_connected() const noexcept { return m_socket.is_open(); }

	private:
		void send(std::span<const u8> msg_)
		{
			m_out_messages.emplace(msg_.begin(), msg_.end());

			if (m_on_writing)
				return;
			m_on_writing = true;
			asio::async_write(m_socket, asio::buffer(m_out_messages.front(), m_out_messages.front().size()), [&](const asio::error_code& ec_, size_t a)
			{
				handle_write(ec_);
			});
		}

		// TODO: Using enum class with Bitmask, instead of 3 booleans
		template<bool Continuous, bool Timed = false, bool Handle = true>
		void read_header() noexcept
		{
			static_assert((Continuous && !Timed) || (!Continuous && Timed) || (!Continuous && !Timed), "Couldn't do continuous read with timed turned on, Timed can only be used for Non-continuous read");
			static_assert((Continuous && Handle) || (!Continuous && (Handle || !Handle)), "Continuous read should be handled by IMessageHandler");

			asio::async_read(m_socket, asio::buffer(m_header_input_buffer, Message::header_size), [&](const asio::error_code& ec_, [[maybe_unused]] size_t)
			{
				if constexpr (Timed)
				{
					if (m_read_once_timer->expiry() < asio::steady_timer::clock_type::now())
						return;
				}
				handle_read_header<Continuous, Handle>(ec_);
			});
		}

		template<bool Continuous, bool Handle>
		void read_body() noexcept
		{
			m_input_message.resize_body();
			asio::async_read(m_socket, asio::buffer(m_input_message.body, m_input_message.header.body_size), [&](const asio::error_code& ec_, [[maybe_unused]] size_t bytes_) {handle_read_body<Continuous, Handle>(ec_); });
		}

		template<bool Continuous, bool Handle>
		void handle_read_header(const asio::error_code& ec_) noexcept
		{
			if (ec_)
			{
				// socket closed
				if (is_disconnect_error(ec_))
				{
					close();
					return;
				}

				if constexpr (Continuous)
					read_header<true>();
				return;
			}
			if (!m_input_message.parse_header(m_header_input_buffer))
			{
				if constexpr (Continuous)
					read_header<true>();
				return;
			}
			read_body<Continuous, Handle>();
		}

		template<bool Continuous, bool Handle>
		void handle_read_body(const asio::error_code& ec_) noexcept
		{
			if (ec_)
			{
				// socket closed
				if (is_disconnect_error(ec_))
				{
					close();
					return;
				}
				// close();
			}
			else
			{
				m_read_once_timer->cancel_one();
				if constexpr (Handle)
					handle_message();
			}

			if constexpr (Continuous)
				read_header<true>();
		}

		void handle_write(const asio::error_code& ec_)
		{
			if (ec_)
			{
				// Close regardless of the error
				close();
				return;
			}

			m_message_handler->on_new_out_message(*this, m_out_messages.front());

			m_out_messages.pop();
			if (m_out_messages.empty())
			{
				m_on_writing = false;
				return;
			}

			asio::async_write(m_socket, asio::buffer(m_out_messages.front(), m_out_messages.front().size()), [&](const asio::error_code& ec_, size_t a)
			{
				handle_write(ec_);
			});
		}

		void handle_message()
		{
			if (m_input_message.type() == MessageType::Validation)
			{
				m_connection_handler->validate(*this, m_input_message);
			}
			else
				m_message_handler->on_new_in_message(*this, m_input_message);
		}

	private:
		bool m_on_writing;
		std::unique_ptr<asio::steady_timer> m_read_once_timer;

		id_type m_id;

		ref<IMessageHandler<ConnectionType::Server>> m_message_handler;
		ref<IConnectionHandler> m_connection_handler;

		std::array<u8, Message::header_size> m_header_input_buffer;
		std::queue<std::vector<u8>> m_out_messages;
		Message m_input_message;
		socket_type m_socket;
	};

	template<>
	class Connection<ConnectionType::Client>
	{
	public:
		using socket_type = asio::ip::tcp::socket;
		using message_handler_type = IMessageHandler<ConnectionType::Client>;

		Connection(asio::io_context& context_, message_handler_type& msg_handler_, IConnectionValidator<ConnectionType::Client>& validator_)
			: m_on_writing{ false }/*, m_is_closed{ false }*/, m_read_once_timer{ std::make_unique<asio::steady_timer>(context_) },
			  m_message_handler{ msg_handler_ },
			  m_validation_handler{validator_}, m_header_input_buffer{}, m_socket{context_}
		{
		}

		Connection(const Connection& other) = delete;
		Connection& operator=(const Connection& other) = delete;

		Connection(Connection&& other) noexcept
			: m_on_writing(other.m_on_writing)/*, m_is_closed{ other.m_is_closed}*/,
			  m_read_once_timer{std::move(other.m_read_once_timer)},
			  m_message_handler(other.m_message_handler),
			  m_validation_handler(other.m_validation_handler),
			  m_header_input_buffer(std::move(other.m_header_input_buffer)),
			  m_out_messages(std::move(other.m_out_messages)),
			  m_input_message(std::move(other.m_input_message)),
			m_socket(std::move(other.m_socket))
		{
		}

		Connection& operator=(Connection&& other) noexcept
		{
			if (this == &other)
				return *this;
			m_on_writing = other.m_on_writing;
			m_message_handler = other.m_message_handler;
			m_validation_handler = other.m_validation_handler;
			m_header_input_buffer = std::move(other.m_header_input_buffer);
			m_out_messages = std::move(other.m_out_messages);
			m_input_message = std::move(other.m_input_message);
			m_socket = std::move(other.m_socket);
			m_read_once_timer = std::move(other.m_read_once_timer);
			// m_is_closed = other.m_is_closed;
			return *this;
		}

		~Connection() noexcept
		{
			disconnect();
		}

		void start() noexcept
		{
			read_header<true>();
		}

		void disconnect() noexcept
		{
			if (!is_connected())
				return;
			// m_is_closed = true;
			m_socket.close();
		}

		/**
		 * \brief do async_read only once and have timer on it
		 */
		template<std::invocable<Connection&> F = decltype(empty_timeout_callback<ConnectionType::Client>), std::invocable<Connection&, const Message&> F1 = decltype(empty_complete_callback<ConnectionType::Client>)>
		void read_timed(const std::chrono::milliseconds& timeout_, F1&& complete_callback_ = empty_complete_callback<ConnectionType::Client>, F&& timeout_callback_ = empty_timeout_callback<ConnectionType::Client>) noexcept
		{
			constexpr bool has_handler = !std::is_same_v<F1, decltype(empty_complete_callback<ConnectionType::Client>)>;
			constexpr bool has_timeout_callback = !std::is_same_v<F, decltype(empty_timeout_callback<ConnectionType::Client>)>;
			if (timeout_ == std::chrono::milliseconds::zero())
				return
			read_header<false, true, !has_handler>();

			m_read_once_timer->expires_after(timeout_);
			m_read_once_timer->async_wait([cf = std::forward<F1>(complete_callback_), tf = std::forward<F>(timeout_callback_), this](const asio::error_code& ec_)
				{
					// Timer callback only got called when the timer is expired
					if (!ec_)
					{
						if constexpr (has_timeout_callback)
							std::invoke(tf, *this);
					}
					// 
					else
					{
						if constexpr (has_handler)
							std::invoke(cf, *this, m_input_message);
					}
				});
		}

		/**
		 * \brief do async_read only once
		 */
		template<std::invocable<Connection&, const Message&> F = decltype(empty_complete_callback<ConnectionType::Client>)>
		void read_once(F&& complete_callback_ = empty_complete_callback<ConnectionType::Client>) noexcept
		{
            using namespace std::chrono_literals;
			constexpr bool handle_by_handler = std::is_same_v<F, decltype(empty_complete_callback<ConnectionType::Client>)>;
			read_header<false, false, handle_by_handler>();

			m_read_once_timer->expires_after(10000s);
			m_read_once_timer->async_wait([cb = std::forward<F>(complete_callback_), this](const asio::error_code& ec_)
				{
					// callback will be called when the timer is cancelled, which will be on message_handler
					if (ec_)
					{
						if constexpr (!handle_by_handler)
							std::invoke(cb, *this, m_input_message);
					}
				});
		}
        
		void send(const Message& msg_) noexcept
		{
			send(msg_.serialize());
		}

		template<Serializable T>
		void send(const T& msg_) noexcept {
			send(Message{msg_});
		}

		void connect(const asio::ip::tcp::endpoint& endpoint_) noexcept
		{
			m_socket.async_connect(endpoint_, [&](const asio::error_code& ec_)
				{
					if (ec_)
					{
						m_socket.close();
						return;
					}

					// Validation
					m_validation_handler->start_validation(*this);
				});
		}

		void connect(const asio::ip::tcp::resolver::results_type& endpoints_, asio::ip::tcp::endpoint& result_) noexcept
		{
			asio::async_connect(m_socket, endpoints_, [&](const asio::error_code& ec_, const asio::ip::tcp::endpoint& ep_)
				{
					if (ec_)
						return;
					result_ = ep_;
				});
		}

		socket_type& socket() noexcept { return m_socket; }
		const socket_type& socket() const noexcept { return m_socket; }
		bool is_connected() const noexcept { return m_socket.is_open() /*&& !m_is_closed*/; }

	private:
		void send(std::span<const u8> msg_)
		{
			m_out_messages.emplace(msg_.begin(), msg_.end());

			if (m_on_writing)
				return;
			m_on_writing = true;
			asio::async_write(m_socket, asio::buffer(m_out_messages.front(), m_out_messages.front().size()), [&](const asio::error_code& ec_, size_t) { handle_write(ec_); });
		}

		template<bool Continuous, bool Timed = false, bool Handle = true>
		void read_header() noexcept
		{
			static_assert((Continuous && !Timed) || (!Continuous && Timed) || (!Continuous && !Timed), "Couldn't do continuous read with timed turned on, Timed can only be used for Non-continuous read");
			static_assert((Continuous && Handle) || (!Continuous && (Handle || !Handle)), "Continuous read should be handled by IMessageHandler");

			asio::async_read(m_socket, asio::buffer(m_header_input_buffer, Message::header_size), [&](const asio::error_code& ec_, [[maybe_unused]] size_t a)
			{
				if constexpr (Timed)
				{
					if (m_read_once_timer->expiry() < asio::steady_timer::clock_type::now())
						return;
				}

				handle_read_header<Continuous, Handle>(ec_);
			});
		}

		template<bool Continuous, bool Handle>
		void read_body() noexcept
		{
			m_header_input_buffer.fill(0);
			m_input_message.resize_body();
			asio::async_read(m_socket, asio::buffer(m_input_message.body, m_input_message.header.body_size), [&](const asio::error_code& ec_, [[maybe_unused]] size_t bytes_)
			{
				handle_read_body<Continuous, Handle>(ec_);
			});
		}

		template<bool Continuous, bool Handle>
		void handle_read_header(const asio::error_code& ec_)
		{
			if (ec_)
			{
				// socket closed
				if (is_disconnect_error(ec_))
				{
					disconnect();
					return;
				}

				if constexpr (Continuous)
					read_header<true>();
				return;
			}
			if (!m_input_message.parse_header(m_header_input_buffer))
			{
				if constexpr (Continuous)
					read_header<true>();
				return;
			}
			read_body<Continuous, Handle>();
		}

		template<bool Continuous, bool Handle>
		void handle_read_body(const asio::error_code& ec_)
		{
			if (ec_)
			{
				// socket closed
				if (is_disconnect_error(ec_))
					disconnect();
			}
			else
			{
				m_read_once_timer->cancel();	// Cancel on here, so when on handle_message calls another read_once or read_timed it will not bother the new handler
												// Or maybe better to post it on asio event loop function on handle_message

				if constexpr (Handle)
					handle_message();
			}

			if constexpr (Continuous)
				read_header<true>();
		}

		void handle_message() noexcept
		{
			if (m_input_message.type() == MessageType::Validation)
				m_validation_handler->validate(*this, m_input_message);
			else
				m_message_handler->on_new_in_message(*this, m_input_message);
		}

		void handle_write(const asio::error_code& ec_)
		{
			if (ec_)
			{
				// socket closed
				if (is_disconnect_error(ec_))
					disconnect();

				return;
			}

			m_message_handler->on_new_out_message(*this, m_out_messages.front());

			m_out_messages.pop();
			if (m_out_messages.empty())
			{
				m_on_writing = false;
				return;
			}

			asio::async_write(m_socket, asio::buffer(m_out_messages.front(), m_out_messages.front().size()), [&](const asio::error_code& ec_, size_t) { handle_write(ec_); });
		}

	private:
		bool m_on_writing;
		// bool m_is_closed;

		std::unique_ptr<asio::steady_timer> m_read_once_timer;

		ref<message_handler_type> m_message_handler;
		ref<IConnectionValidator<ConnectionType::Client>> m_validation_handler;

		std::array<u8, Message::header_size> m_header_input_buffer;
		std::queue<std::vector<u8>> m_out_messages;
		Message m_input_message;
		socket_type m_socket;
	};

	using ServerConnection = Connection<ConnectionType::Server>;
	using ClientConnection = Connection<ConnectionType::Client>;
}
