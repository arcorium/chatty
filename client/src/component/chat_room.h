#pragma once
#include <vector>
#include <span>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component_base.hpp>

#include "chat.h"

namespace ar
{
	class ChatRoom : public ftxui::ComponentBase
	{
	public:
		ChatRoom()
			: m_current_user_id{-1}, m_layout{ftxui::Container::Vertical({})}
		{
		}

		// Implements the Component interface.
		ftxui::Element Render() override
		{
			using namespace ftxui;

			if (!m_layout->ChildCount())
				return vbox({
				}) | center;

			return vbox({
				m_layout->Render() | vscroll_indicator | focusPositionRelative(0, y) | yframe
			});
		}

		bool OnEvent(ftxui::Event event_) override
		{
			constexpr float delta = 0.05f;
			if (event_.is_mouse())
			{
				if (event_.mouse().button == ftxui::Mouse::WheelUp)
				{
					y = std::max(0.2f, y - delta);
					return true;
				}
				if (event_.mouse().button == ftxui::Mouse::WheelDown)
				{
					y = std::min(delta + y, 0.8f);
					return true;
				}
				return false;
			}
			// Keyboard
			if (event_ == ftxui::Event::ArrowDown)
			{
				y = std::min(delta + y, 0.8f);
				return true;
			}
			if (event_ == ftxui::Event::ArrowUp)
			{
				y = std::max(0.2f, y - delta);
				return true;
			}

			return false;
		}

		void add_chat(const Chat& chat_) noexcept
		{
			m_chats.push_back(chat_);
			auto element = create_bubble(m_chats.back());
			auto comp = ftxui::Renderer([el = std::move(element)] { return el; });
			m_layout->Add(comp);
		}

		void set(u32 id_, std::span<Chat> chats_) noexcept
		{
			clear();
			m_current_user_id = static_cast<i64>(id_);
			for (const auto& chat : chats_)
				add_chat(chat);
		}

		void clear() noexcept
		{
			m_layout = ftxui::Container::Vertical({});
			m_chats.clear();
			m_current_user_id = -1;
		}

		bool Focusable() const override { return true; }

		i64 user_id() const noexcept { return m_current_user_id; }

	private:
		ftxui::Element create_bubble(const Chat& chat_) noexcept
		{
			using namespace ftxui;

			auto result = hbox({
				text(chat_.message) | border,
				filler()
			});

			if (chat_.sender == Chat::Sender::Self)
				result |= align_right;

			return result;
		}

	private:
		float y = 0.2f;
		i64 m_current_user_id;

		ftxui::Component m_layout;
		std::vector<Chat> m_chats;
	};
}
