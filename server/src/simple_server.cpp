#include "simple_server.h"

#include "connection_manager.h"

namespace ar
{
	SimpleServer::SimpleServer(const asio::ip::tcp::endpoint& ep_)
		: IServer{ ep_, ConnectionManager::get() }, m_connection_manager{ ConnectionManager::get() }, m_private_key{generate_private_key(m_rng)}, m_public_key{generate_public_key(m_private_key)}
	{
	}

	void SimpleServer::on_new_in_message(connection_type& conn_, const Message& message_) noexcept
	{
		switch (message_.type())
		{
		case MessageType::Chat:
		{
			auto chat = message_.body_as<ChatMessage>();

			if (chat.opponent == ChatOpponent::Server)
			{
				chat.decrypt(m_rng, m_private_key);
				spdlog::info("Chat: {} :: {}", conn_.id(), chat.message_str());
				break;
			}

			const auto conn = m_connection_manager->connection(chat.opponent_id);
			if (!conn)
				break;

			spdlog::info("Chat: [{}] -> [{}]", conn_.id(), chat.opponent_id);
			chat.opponent_id = conn_.id();
			conn->send(chat);
			break;
		}
		case MessageType::Command:
		{
			const auto command = message_.body_as<CommandMessage>();
			switch (command.command_type)
			{
			case CommandType::OnlineList:
			{
				const auto conn_id = conn_.id();
				OnlineListMessage::user_container container{};

				for (const auto& conn : m_connection_manager->connections() | std::ranges::views::filter(
					[=](const connection_type* conn2_) { return conn_id != conn2_->id(); }))
				{
					auto user = m_connection_manager->user(conn->id());
					if (!user)
						continue;

					container.emplace_back(conn->id(), user->name);
				}

				const OnlineListMessage respond_msg{ CommandType::OnlineList, std::move(container) };
				conn_.send(respond_msg);
				break;
			}
			case CommandType::RequestPublicKey:
			{
				// Check argument size
				if (command.arguments.empty())
					break;

				const auto id = command.arguments[0];
				// Send server public key
				if (!id)
				{
					const auto pk = save_public_key(m_public_key);
					const RequestPublicKeyMessage respond_msg{ CommandType::RequestPublicKey, id, pk };
					conn_.send(respond_msg);
					break;
				}

				const auto user = m_connection_manager->user(id);
				if (!user)
					break;

				const RequestPublicKeyMessage respond_msg{
					CommandType::RequestPublicKey, id, user->public_key
				};
				conn_.send(respond_msg);

				break;
			}
			case CommandType::RequestUserProperties:
			{
				// Check argument size
				if (command.arguments.empty())
					break;

				const auto id = command.arguments[0];
				if (!id)
					break;

				const auto user = m_connection_manager->user(id);
				if (user)
					break;

				const RequestUserPropertiesMessage respond_msg{
					CommandType::RequestUserProperties, id, user->name, user->public_key
				};
				conn_.send(respond_msg);
			}
			}
		}
		}
	}

}
