#pragma once
#include <mutex>

#include <fmt/std.h>
#include <spdlog/spdlog.h>

#include <ftxui/component/screen_interactive.hpp>

#include "user.h"
#include "simple_client.h"

#include "chat.h"

namespace ar
{
	class ChatRoom;

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
		int m_online_selected{-1};

		std::vector<std::string> m_username_chats;
		std::vector<std::pair<u32, bool>> m_user_details;

		std::unordered_map<u32, std::vector<Chat>> m_chat_database;
		ftxui::Component m_chat_room;
		ftxui::ScreenInteractive m_screen;

		std::string m_send_input_placeholder{};

		SimpleClient m_client;
	};
}
