#pragma once
#include <mutex>
#include <condition_variable>

#include <spdlog/spdlog.h>

#include <cryptopp/elgamal.h>
#include <cryptopp/cryptlib.h>
#include <cryptopp/osrng.h>
#include <ftxui/component/screen_interactive.hpp>

#include "user.h"
#include "client.h"
#include "message/command.h"
#include "util/util.h"
#include "util/literal.h"

#include "chat.h"


namespace ar
{
	class ChatRoom;
	namespace cry = CryptoPP;

	enum class ClientState
	{
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
		using user_callback = std::optional<std::function<void(u32,User&)>>;

		SimpleClient(const asio::ip::address& addr_, u16 port_)
			: IClient{addr_, port_, *this}
		{
		}

		void wait_for_state(ClientState state_,
		                    std::chrono::milliseconds timeout_ = std::chrono::milliseconds::zero()) const
		{
			if (timeout_ == std::chrono::milliseconds::zero())
				timeout_ = std::chrono::duration_cast<std::chrono::milliseconds>(24h);

			wait_until_state(state_, std::chrono::system_clock::now() + timeout_);
		}

		void wait_until_state(ClientState state_, std::chrono::time_point<std::chrono::system_clock> timepoint_) const
		{
			while (m_state != state_ && std::chrono::system_clock::now() < timepoint_ && connection().is_connected())
				std::this_thread::sleep_for(25ms);
		}

		void wait_for_message(MessageType type_, std::chrono::milliseconds timeout_ = std::chrono::milliseconds::zero())
		{
			std::unique_lock lock{m_signaler.mutex};
			m_signaler.type = type_;

			if (timeout_ == std::chrono::milliseconds::zero())
				m_signaler.cv.wait(lock, [this] { return m_signaler.type == m_last_type; });
			else
				m_signaler.cv.wait_for(lock, timeout_, [this] { return m_signaler.type == m_last_type; });

			m_signaler.type = MessageType::Undefined;
			m_last_type = MessageType::Undefined;
		}

		[[nodiscard]] ClientState state() const noexcept { return m_state; }

		template<std::invocable<u32, User&> F>
		void set_new_user_callback(F&& callback_) noexcept
		{
			m_new_user_callback = std::forward<F>(callback_);
		}

		template<std::invocable<u32, User&> F>
		void set_disconnect_user_callback(F&& callback_) noexcept
		{
			m_disconnect_user_callback = std::forward<F>(callback_);
		}

		template<std::invocable<u32, Chat&&> F>
		void set_new_chat_callback(F&& callback_) noexcept
		{
			m_new_chat_callback = std::forward<F>(callback_);
		}

		void username(std::string_view username_) noexcept
		{
			{
				std::unique_lock lock{m_username_input_mutex};
				m_username.assign(username_);
			}
			m_username_input_cv.notify_one();
		}

		ptr<User> user(ServerConnection::id_type id_) noexcept
		{
			const auto it = m_users.find(id_);
			if (it == m_users.end())
				return {};
			return &it->second;
		}

		const user_container& users() const noexcept
		{
			return m_users;
		}

		cry::RandomNumberGenerator& rng() noexcept { return m_rng; }
		const cry::RandomNumberGenerator& rng() const noexcept { return m_rng; }

	private:
		void on_new_in_message(connection_type& conn_, const Message& message_) noexcept override
		{
			switch (message_.type())
			{
			case MessageType::Chat:
				{
					auto chat = message_.body_as<ChatMessage>();

					chat.decrypt(m_rng, m_private_key);
					if (m_users.contains(chat.opponent_id))
					{
						const CommandMessage cmd{ CommandType::RequestPublicKey, {{chat.opponent_id}} };
						conn_.send(cmd);
					}

					// spdlog::info("[{}]: {}", name, chat.message_str());
					m_new_chat_callback(chat.opponent_id, Chat::from_opponent(chat.message_str()));
					break;
				}
			case MessageType::Command:
				{
					switch (static_cast<CommandType>(message_.body[0]))
					{
					case CommandType::OnlineList:
						{
							auto msg = message_.body_as<OnlineListMessage>();
							for (auto& [id, name] : msg.users)
							{
								m_users[id].name = std::move(name);
							}
							break;
						}
					case CommandType::RequestPublicKey:
						{
							auto msg = message_.body_as<RequestPublicKeyMessage>();
							auto key = load_public_key(msg.public_key);
							m_users[msg.opponent_id].public_key = key;
							m_users[msg.opponent_id].has_key = true;
							break;
						}
					case CommandType::RequestUserProperties:
						{
							auto msg = message_.body_as<RequestUserPropertiesMessage>();
							auto key = load_public_key(msg.public_key);
							m_users[msg.id].name = std::move(msg.username);
							m_users[msg.id].public_key = key;
							m_users[msg.id].has_key = true;
							break;
						}
					}
					break;
				}
			case MessageType::UserDisconnect:
				{
					auto msg = message_.body_as<UserDisconnectMessage>();
					if (m_disconnect_user_callback)
						m_disconnect_user_callback.value()(msg.id, m_users[msg.id]);
					m_users.erase(msg.id);
					break;
				}
			case MessageType::NewUser:
			{
				auto msg = message_.body_as<NewUserMessage>();
				m_users[msg.id].has_key = false;
				m_users[msg.id].name = std::move(msg.name);
				if (m_new_user_callback)
					m_new_user_callback.value()(msg.id, m_users[msg.id]);
				break;
			}
			default: ;
			}
			// Update for signaler
			message_type(message_.type());
		}

		void on_new_out_message(connection_type& conn_, std::span<const u8> message_) noexcept override
		{
		}

		void validate(Connection<ConnectionType::Client>& conn_, const Message& msg_) noexcept override
		{
			m_state = ClientState::Validating;

			const auto msg = msg_.body_as<ValidationMessage>();
			const auto result = encrypt_xor(msg.challenge, KEY);
			const ValidationMessage val_msg{result};
			conn_.send(val_msg);
			conn_.read_once([this](connection_type& conn_, const Message& msg_)
			{
				if (!check_result(conn_, msg_))
					return;
				authenticate(conn_);
			});
		}

		void start_validation(Connection<ConnectionType::Client>& conn_) noexcept override
		{
			m_state = ClientState::Connecting;
			conn_.read_once();
		}

		bool check_result(connection_type& conn_, const Message& msg_)
		{
			const auto result_msg = msg_.body_as<FeedbackMessage>();
			if (!result_msg.result)
			{
				m_state = ClientState::Closed;
				conn_.disconnect();
				return false;
			}
			return true;
		}

		void authenticate(connection_type& conn_)
		{
			m_state = ClientState::Authenticating;

			// Wait until username has value
			std::unique_lock lock{m_username_input_mutex};
			m_username_input_cv.wait(lock, [this] { return !m_username.empty(); });

			// Generate Keys
			auto [private_key, public_key] = generate_keys(m_rng);
			m_private_key = private_key;
			m_public_key = public_key;
			const auto pk = save_public_key(public_key);

			const AuthenticateMessage auth_msg{m_username, pk};
			conn_.send(auth_msg);

			conn_.read_once([this](connection_type& conn_, const Message& msg_)
			{
				if (!check_result(conn_, msg_))
					return;
				m_state = ClientState::Connected;
				connection().start();
			});
		}

		void message_type(MessageType type_) noexcept
		{
			{
				std::unique_lock lock{m_signaler.mutex};
				m_last_type = type_;
			}
			if (m_signaler.type == m_last_type)
				m_signaler.cv.notify_one();
		}

	protected:
		user_container m_users;

	private:
		ClientState m_state;
		MessageType m_last_type;

		user_callback m_new_user_callback;
		user_callback m_disconnect_user_callback;
		std::function<void(u32,Chat&&)> m_new_chat_callback;

		std::condition_variable m_username_input_cv;
		std::mutex m_username_input_mutex;
		std::string m_username;

		cry::AutoSeededRandomPool m_rng{};
		cry::ElGamal::PrivateKey m_private_key;
		cry::ElGamal::PublicKey m_public_key;

		SignalerMessage m_signaler;

		constexpr static inline std::string_view KEY = "n1odah10"sv;
	};

	class Application
	{
	public:
		Application();

		void start();

	private:
		void send_message(std::string_view msg_) noexcept;

		void render_chat() noexcept;

		void send_command_message(CommandType type_, std::optional<u32> argument_, bool wait_feedback = false) noexcept;

		void on_new_user(u32 id_, User& user_) noexcept;

		void on_disconnect_user(u32 id_, User& user_) noexcept;

		void on_new_chat(u32 id_, Chat&& chat_) noexcept;

		void add_chat(u32 user_id_, const Chat& chat_) noexcept;

		void add_user(u32 id, std::string_view name_) noexcept;

		void remove_user(u32 index_);

		std::shared_ptr<ChatRoom> chat_room() noexcept;

	private:
		std::vector<std::string> m_username_chats;
		std::vector<std::pair<u32, bool>> m_user_details;

		std::unordered_map<u32, std::vector<Chat>> m_chat_database;
		ftxui::Component m_chat_room;
		ftxui::ScreenInteractive m_screen;

		std::string m_send_input_placeholder;

		SimpleClient m_client;
	};
}
