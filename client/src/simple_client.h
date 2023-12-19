#pragma once
#include <mutex>
#include <condition_variable>

#include <cryptopp/elgamal.h>
#include <cryptopp/cryptlib.h>
#include <cryptopp/osrng.h>

#include "chat.h"
#include "client.h"
#include "handler.h"


namespace ar
{
	struct User;
	enum class MessageType : u8;

	namespace cry = CryptoPP;

	enum class ClientState
	{
		Undefined,
		Connecting,
		Validating,
		Authenticating,
		Connected,
		Closed
	};

	struct SignalerMessage
	{
		MessageType type;
		std::mutex mutex;
		std::condition_variable cv;
	};

	class SimpleClient : public IClient, public IConnectionValidator<ConnectionType::Client>
	{
	public:
		using user_container = std::unordered_map<ServerConnection::id_type, User>;
		using user_callback = std::optional<std::function<void(u32, User&)>>;
		using chat_callback = std::optional<std::function<void(u32, Chat&&)>>;

		SimpleClient(const asio::ip::address& addr_, u16 port_);

		bool wait_for_state(ClientState state_, std::chrono::milliseconds timeout_ = std::chrono::milliseconds::zero()) const noexcept;

		bool wait_until_state(ClientState state_, std::chrono::time_point<std::chrono::system_clock> timepoint_) const noexcept;

		void wait_for_message(MessageType type_, std::chrono::milliseconds timeout_ = std::chrono::milliseconds::zero()) noexcept;

		[[nodiscard]] ClientState state() const noexcept { return m_state; }

		template<std::invocable<u32, User&> F>
		void set_new_user_callback(F&& callback_) noexcept;

		template<std::invocable<u32, User&> F>
		void set_disconnect_user_callback(F&& callback_) noexcept;

		template<std::invocable<u32, Chat&&> F>
		void set_new_chat_callback(F&& callback_) noexcept;

		void username(std::string_view username_) noexcept;

		ptr<User> user(ServerConnection::id_type id_) noexcept;
		const user_container& users() const noexcept { return m_users; }

		cry::RandomNumberGenerator& rng() noexcept { return m_rng; }
		const cry::RandomNumberGenerator& rng() const noexcept { return m_rng; }

	private:
		void on_new_in_message(connection_type& conn_, const Message& message_) noexcept override;

		void on_new_out_message(connection_type& conn_, std::span<const u8> message_) noexcept override {}

		void start_validation(Connection<ConnectionType::Client>& conn_) noexcept override;

		void validate(Connection<ConnectionType::Client>& conn_, const Message& msg_) noexcept override;

		template<FeedbackType Type>
		bool expect_feedback(connection_type& conn_, const Message& msg_) noexcept;

		void authenticate(connection_type& conn_) noexcept;

		void message_type(MessageType type_) noexcept;

	private:
		MessageType m_last_type;
		ClientState m_state;

		std::string m_username;
		std::mutex m_username_input_mutex;
		std::condition_variable m_username_input_cv;

		user_container m_users;

		SignalerMessage m_signaler;

		user_callback m_new_user_callback;
		user_callback m_disconnect_user_callback;
		chat_callback m_new_chat_callback;

		cry::AutoSeededRandomPool m_rng{};
		cry::ElGamal::PrivateKey m_private_key;
		cry::ElGamal::PublicKey m_public_key;

		constexpr static inline std::string_view KEY = "n1odah10"sv;
	};

	template <std::invocable<u32, User&> F>
	void SimpleClient::set_new_user_callback(F&& callback_) noexcept
	{
		m_new_user_callback = std::forward<F>(callback_);
	}

	template <std::invocable<u32, User&> F>
	void SimpleClient::set_disconnect_user_callback(F&& callback_) noexcept
	{
		m_disconnect_user_callback = std::forward<F>(callback_);
	}

	template <std::invocable<u32, Chat&&> F>
	void SimpleClient::set_new_chat_callback(F&& callback_) noexcept
	{
		m_new_chat_callback = std::forward<F>(callback_);
	}

	template <FeedbackType Type>
	bool SimpleClient::expect_feedback(connection_type& conn_, const Message& msg_) noexcept
	{
		const auto feedback_message = msg_.body_as<FeedbackMessage>();
		if (feedback_message.data != Type)
		{
			m_state = ClientState::Closed;
			conn_.disconnect();
			return false;
		}
		return true;
	}
}
