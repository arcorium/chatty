#pragma once
#include "ftxui/component/component_base.hpp"

namespace ar
{
	class DynamicText : public ftxui::ComponentBase
	{
	public:
		DynamicText(const std::string& data_)
		: m_text(data_)
		{}

		// Set the text dynamically.
		void text(std::string&& new_val_)
		{
			m_text = std::forward<std::string>(new_val_);
		}

		// Implements the Component interface.
		ftxui::Element Render() override
		{
			return ftxui::text(m_text);
		}

	private:
		std::string m_text;
	};
}
