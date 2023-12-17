#pragma once
#include <string>

#include "util/util.h"

namespace ar
{
	struct Chat
	{
		enum class Sender
		{
			Self,
			Opponent
		} sender;
		std::string message;
		std::string time;

		static Chat from_opponent(std::string_view message_)
		{
			return Chat{ Sender::Opponent, std::string{message_}, get_current_time() };
		}

		static Chat from_self(std::string_view message_)
		{
			return Chat{ Sender::Self, std::string{message_}, get_current_time() };
		}
	};
}
