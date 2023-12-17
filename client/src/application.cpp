#include "application.h"

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

#include "component/text.h"
#include "component/chat_room.h"

namespace ar
{
	Application::Application()
		: m_chat_room{std::make_shared<ChatRoom>()}, m_screen{ftxui::ScreenInteractive::Fullscreen()},
		  m_client{asio::ip::address_v4{{127, 0, 0, 1}}, 9696}
	{
		m_client.set_new_user_callback([this](u32 id_, User& user_) { on_new_user(id_, user_); });
		m_client.set_disconnect_user_callback([this](u32 id_, User& user_) { on_disconnect_user(id_, user_); });
		m_client.set_new_chat_callback([this](u32 id_, Chat&& chat_) { on_new_chat(id_, std::forward<Chat>(chat_)); });

		add_user(0, " ");	// Dummy data to indicate it is not selecting any user
	}

	void Application::start()
	{
		m_client.connect();

		m_client.wait_for_state(ClientState::Authenticating);
		render_chat();

		m_client.disconnect();
	}

	void Application::send_message(std::string_view msg_) noexcept
	{
		const auto _id = chat_room()->user_id();
		if (_id < 0)
			return;

		auto id = static_cast<u32>(_id);

		const auto chat = Chat::from_self(msg_);
		add_chat(id, chat);

		auto opponent = m_client.user(id);
		if (!opponent)
			return;

		if (!opponent->has_key)
			send_command_message(CommandType::RequestPublicKey, id, true);

		auto msg = ChatMessage{(!id) ? ChatOpponent::Server : ChatOpponent::User, id, {msg_.begin(), msg_.end()}};
		msg.encrypt(m_client.rng(), opponent->public_key);
		m_client.connection().send(msg);
	}

	void Application::render_chat() noexcept
	{
		using namespace ftxui;

		int online_selected = -1;

		std::string message{};
		Component title = std::make_shared<DynamicText>(" ");

		auto send_message_fn = [&]
		{
			if (message.empty() || !online_selected)
				return;

			const auto& details = m_user_details[online_selected];
			if (!details.second)
			{
				message.clear();
				return;
			}

			send_message(message);
			message.clear();
		};

		Component msg_input = Input(&message, &m_send_input_placeholder, InputOption{ .multiline = false, .on_enter = send_message_fn });
		Component send_button = Button("Send", send_message_fn);
		// Component close_button = Button("x", [&] { m_screen.Exit(); });

		auto online_list_option = MenuOption::Vertical();
		online_list_option.on_change = [&]
		{
			const auto room_title = std::dynamic_pointer_cast<DynamicText>(title);
			if (!online_selected)
			{
				room_title->text("");
				m_send_input_placeholder = "please select person on left side panel";
				return;
			}

			auto str = m_username_chats[online_selected];
			auto details = m_user_details[online_selected];
			auto& chats = m_chat_database[details.first];

			// Handle offline user?
			if (!details.second)
				m_send_input_placeholder = "user is offline, come back later";	// TODO: Better to have string_view instead and point it into some 'static' string
			else
				m_send_input_placeholder = "write your message here...";

			// Set room chat
			chat_room()->set(details.first, chats);

			// Set room title
			room_title->text(std::move(str));
		};
		online_list_option.entries_option.transform = [](const EntryState& entry_state)
			{
				if (entry_state.label == " ")
					return vbox(
						text("Onlines") | center,
						separator()
					);

				auto res = text(entry_state.label) | center | border;
				if (entry_state.focused)
					res |= dim;
				else if (entry_state.active)
					res |= inverted;
				return res;
			};
		auto online_list = Menu(&m_username_chats, &online_selected, online_list_option);
		online_list |= CatchEvent([&](const Event& event_)
		{
			if (event_ == Event::Tab)
			{
				m_chat_room->TakeFocus();
				return true;
			}
			return false;
		});

		auto container = Container::Vertical(
			{
				online_list,
				m_chat_room,
				msg_input,
				send_button,
				title,
				// close_button,
			}
		);


		auto layout = Renderer(container, [&]
		{
			return hbox(
				{
					vbox({
						online_list->Render() | vscroll_indicator | frame | size(WIDTH, GREATER_THAN, 18)
						/*| size(HEIGHT, LESS_THAN, 43)*/,
						// filler(),
						// separator(),
						// close_button->Render(),
					}),
					separator(),
					vbox({
						title->Render() | center,
						separator(),
						m_chat_room->Render() | size(HEIGHT, LESS_THAN, 40),
						filler(),
						vbox({
							separator(),
							hbox({
								msg_input->Render() | border,
								send_button->Render()
							})
						}) | size(HEIGHT, EQUAL, 4),
					}) | flex
				}
			) | border;
		});

		bool modal_shown = true;
		std::string username{};
		Component error_log = std::make_shared<DynamicText>("");

		auto component = Container::Vertical({
			Container::Horizontal({
				Renderer([] { return text("Username: "); }),
				Input(&username, "username")
			}) | border,
			Container::Horizontal({
				Button("Exit", [&]{ m_screen.Exit(); }),
				Button("Authenticate", [&]
				{
					m_client.username(username);
					m_client.wait_for_state(ClientState::Connected);

					// Get list online
					send_command_message(CommandType::OnlineList, std::nullopt, true);
					const auto& users = m_client.users();

					for (const auto& user : users)
					{
						add_user(user.first, user.second.name);
					}

					modal_shown = false;
				}) /*| align_right*/,
			}) | align_right,
		});

		component |= Renderer([&](Element inner)
		{
			return vbox({
					text("Authentication") | center,
					error_log->Render() | center | color(Color::Orange3),
					separator(),
					inner,
				}) //
				| size(WIDTH, GREATER_THAN, 50)
				| border;
		});

		layout |= Modal(component, &modal_shown);

		m_screen.Loop(layout);
	}

	void Application::send_command_message(CommandType type_, std::optional<u32> argument_, bool wait_feedback) noexcept
	{
		CommandMessage msg{type_};
		if (argument_)
			msg.arguments.emplace_back(*argument_);

		m_client.connection().send(msg);

		if (wait_feedback)
			m_client.wait_for_message(MessageType::Command);
	}

	void Application::on_new_user(u32 id_, User& user_) noexcept
	{
		m_screen.Post([this, id_, name = user_.name]
			{
				add_user(id_, name);
			});
	}

	void Application::on_disconnect_user(u32 id_, User& user_) noexcept
	{
		usize index = 0;
		for (; index < m_user_details.size(); ++index)
		{
			if (m_user_details[index].first == id_)
				break;
		}

		m_screen.Post([this, index]
			{
				remove_user(index);
			});
	}

	void Application::on_new_chat(u32 id_, Chat&& chat_) noexcept
	{
		m_screen.Post([this, id_, chat = std::forward<Chat>(chat_)]
			{
				add_chat(id_, chat);
			});
	}

	void Application::add_chat(u32 user_id_, const Chat& chat_) noexcept
	{
		if (user_id_ == chat_room()->user_id())
			chat_room()->add_chat(chat_);
		m_chat_database[user_id_].push_back(chat_);
	}

	void Application::add_user(u32 id, std::string_view name_) noexcept
	{
		m_user_details.emplace_back(id, true);
		m_username_chats.emplace_back(name_);
	}

	void Application::remove_user(u32 index_)
	{
		// Check if there is chat left
		auto& details = m_user_details.at(index_);
		if (!m_chat_database.contains(details.first) || m_chat_database[details.first].empty())
		{
			// Remove
			m_user_details.erase(m_user_details.begin() + index_);
			m_username_chats.erase(m_username_chats.begin() + index_);
			m_chat_database.erase(details.first);
			return;
		}
		// Set offline
		details.second = false;
	}

	std::shared_ptr<ChatRoom> Application::chat_room() noexcept
	{
		return std::dynamic_pointer_cast<ChatRoom>(m_chat_room);
	}
}
