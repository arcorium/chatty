#include "simple_client.h"

#include "connection.h"

#include "message/message.h"
#include "message/command.h"
#include "user.h"


namespace ar
{
	SimpleClient::SimpleClient(const asio::ip::address& addr_, u16 port_)
		: IClient{addr_, port_, *this}, m_last_type{MessageType::Undefined}, m_state{ClientState::Undefined},
		  m_signaler{MessageType::Undefined}
	{
	}

	bool SimpleClient::wait_for_state(ClientState state_, std::chrono::milliseconds timeout_) const noexcept
	{
		if (timeout_ == std::chrono::milliseconds::zero())
			timeout_ = std::chrono::duration_cast<std::chrono::milliseconds>(24h);

		return wait_until_state(state_, std::chrono::system_clock::now() + timeout_);
	}

	bool SimpleClient::wait_until_state(ClientState state_,
	                                    std::chrono::time_point<std::chrono::system_clock> timepoint_) const noexcept
	{
		while (m_state != state_ && std::chrono::system_clock::now() < timepoint_)
		{
			if (m_state == ClientState::Closed || !connection().is_connected())
				return false;
			std::this_thread::sleep_for(25ms);
		}
		return true;
	}

	void SimpleClient::wait_for_message(MessageType type_, std::chrono::milliseconds timeout_) noexcept
	{
		std::unique_lock lock{m_signaler.mutex};
		m_signaler.type = type_;

		if (timeout_ == std::chrono::milliseconds::zero())
			m_signaler.cv.wait(lock, [this] { return m_signaler.type == m_last_type || !connection().is_connected(); });
		else
			m_signaler.cv.wait_for(lock, timeout_, [this]
			{
				return m_signaler.type == m_last_type || !connection().is_connected();
			});

		m_signaler.type = MessageType::Undefined;
		m_last_type = MessageType::Undefined;
	}

	void SimpleClient::username(std::string_view username_) noexcept
	{
		{
			std::unique_lock lock{m_username_input_mutex};
			m_username.assign(username_);
		}
		m_username_input_cv.notify_one();
	}

	ptr<User> SimpleClient::user(ServerConnection::id_type id_) noexcept
	{
		const auto it = m_users.find(id_);
		if (it == m_users.end())
			return {};
		return &it->second;
	}

	void SimpleClient::on_new_in_message(connection_type& conn_, const Message& message_) noexcept
	{
		switch (message_.type())
		{
		case MessageType::Chat:
			{
				auto chat = message_.body_as<ChatMessage>();

				chat.decrypt(m_rng, m_private_key);
				if (m_users.contains(chat.opponent_id))
				{
					const CommandMessage cmd{CommandType::RequestPublicKey, {{chat.opponent_id}}};
					conn_.send(cmd);
				}

				if (m_new_chat_callback)
					m_new_chat_callback.value()(chat.opponent_id, Chat::from_opponent(chat.message_str()));
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
				if (!m_users.contains(msg.id))
					break;
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

	void SimpleClient::start_validation(Connection<ConnectionType::Client>& conn_) noexcept
	{
		m_state = ClientState::Connecting;
		conn_.read_once();
	}

	void SimpleClient::validate(Connection<ConnectionType::Client>& conn_, const Message& msg_) noexcept
	{
		m_state = ClientState::Validating;

		const auto msg = msg_.body_as<ValidationMessage>();
		const auto result = encrypt_xor(msg.challenge, KEY);
		const ValidationMessage val_msg{result};
		conn_.send(val_msg);
		conn_.read_once([this](connection_type& conn_, const Message& msg_)
		{
			if (!expect_feedback<FeedbackType::ValidationSucceed>(conn_, msg_))
			{
				m_state = ClientState::Closed;
				connection().disconnect();
				return;
			}
			authenticate(conn_);
		});
	}

	void SimpleClient::authenticate(connection_type& conn_) noexcept
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
		auto str_pk = fmt::format("{}", pk);


		const AuthenticateMessage auth_msg{m_username, pk};
		conn_.send(auth_msg);

		conn_.read_once([this](connection_type& conn_, const Message& msg_)
		{
			if (!expect_feedback<FeedbackType::AuthenticationSucceed>(conn_, msg_))
			{
				m_state = ClientState::Closed;
				connection().disconnect();
				return;
			}
			m_state = ClientState::Connected;
			connection().start();
		});
	}

	void SimpleClient::message_type(MessageType type_) noexcept
	{
		{
			std::unique_lock lock{m_signaler.mutex};
			m_last_type = type_;
		}
		if (m_signaler.type == m_last_type)
			m_signaler.cv.notify_one();
	}
}
