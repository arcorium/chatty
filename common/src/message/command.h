#pragma once
#include <ranges>
#include <string>
#include <vector>

#include "message.h"
#include "util/util.h"
#include "util/types.h"

namespace ar {

	// Payload: +@@####**$...####**$..., etc
	// + = command_id (1 byte)
	// @ = total_len (2 bytes)
	// # = id_type (4 bytes)
	// * = string_len (2 bytes)
	// $... = name (unspecified)
	struct OnlineListMessage
	{
		using id_type = u32;
		using user_type = std::pair<id_type, std::string>;
		using user_container = std::vector<user_type>;

		CommandType command_id = CommandType::OnlineList;
		user_container users;

		bool deserialize(std::span<const u8> body_) noexcept
		{
			if (body_.size() < 8)
				return false;

			command_id = static_cast<CommandType>(body_[0]);

			const auto len = span_to<u16>(body_, sizeof(CommandType));
			if (!len)
				return false;

			users.reserve(*len);

			usize last_bytes = sizeof(u16) + sizeof(CommandType);	// count bytes
			for (u16 i = 0; i < *len; ++i)
			{
				const auto id_p = span_to<u32>(body_, last_bytes);
				if (!id_p)
					return false;
				last_bytes += sizeof(u32);

				const auto name_len = span_to<u16>(body_, last_bytes);
				if (!name_len)
					return false;
				last_bytes += sizeof(u16);


				auto name = shrink_span(body_, last_bytes, *name_len);
				auto id = *id_p;
				last_bytes += *name_len;

				users.emplace_back(std::make_pair<id_type, std::string>(std::move(id), { name.begin(), name.end() }));
			}

			return true;
		}

		[[nodiscard]] std::vector<u8> serialize() const noexcept
		{
			// const usize len = size();

			const auto count = static_cast<u16>(users.size());
			const auto count_span = to_span<u8>(count);

			std::vector<u8> result{};
			result.emplace_back(static_cast<u8>(command_id));
			result.insert(result.end(), count_span.begin(), count_span.end());

			for (const auto& [id, name] : users)
			{
				// id
				auto id_span = to_span<u8>(id);
				result.insert(result.end(), id_span.begin(), id_span.end());

				// string len
				auto name_len = static_cast<u16>(name.size());
				auto name_len_span = to_span<u8>(name_len);
				result.insert(result.end(), name_len_span.begin(), name_len_span.end());

				// name
				result.insert(result.end(), name.begin(), name.end());
			}

			return result;
		}

		[[nodiscard]] MessageType type() const noexcept { return MessageType::Command; }

		[[nodiscard]] constexpr usize size() const noexcept
		{
			usize result = 2;	// 2 for user count
			for (const auto& str : users | std::views::values)
				result += str.size() + 2;	// 2 for string len

			return result + (sizeof(id_type) * users.size()) + sizeof(command_id);
		}
	};

	struct RequestPublicKeyMessage {
		using id_type = u32;
		using key_type = std::vector<u8>;

		CommandType command_id = CommandType::RequestPublicKey;
		id_type opponent_id;
		key_type public_key;

		bool deserialize(std::span<const u8> body_) noexcept
		{
			if (body_.size() < size())
				return false;

			command_id = static_cast<CommandType>(body_[0]);

			const auto id_p = span_to<id_type>(body_, sizeof(CommandType));
			if (!id_p)
				return false;

			opponent_id = *id_p;
			auto key_span = shrink_span(body_, sizeof(id_type) + sizeof(CommandType));
			public_key.assign(key_span.begin(), key_span.end());
			return true;
		}

		[[nodiscard]] std::vector<u8> serialize() const noexcept
		{
			std::vector<u8> result{};
			result.reserve(size());

			auto id_span = to_span<u8>(opponent_id);
			result.emplace_back(static_cast<u8>(command_id));
			result.insert(result.end(), id_span.begin(), id_span.end());
			result.insert(result.end(), public_key.begin(), public_key.end());

			return result;
		}

		[[nodiscard]] MessageType type() const noexcept { return MessageType::Command; }

		[[nodiscard]] constexpr usize size() const noexcept { return sizeof(CommandType) + sizeof(id_type) + public_key.size(); }
	};

	// Payload: +##@@$...*...
	// # = id (4 bytes)
	// @ = username_len (2 bytes)
	// $... = name (username_len)
	// * = public_key (2 bytes)
	struct RequestUserPropertiesMessage
	{
		using id_type = u32;
		using key_type = std::vector<u8>;

		CommandType command_id = CommandType::RequestUserProperties;
		id_type id;
		std::string username;
		key_type public_key;

		bool deserialize(std::span<const u8> body_) noexcept
		{
			if (body_.size() < size())
				return false;

			command_id = static_cast<CommandType>(body_[0]);

			const auto id_p = span_to<id_type>(body_, sizeof(CommandType));
			if (!id_p)
				return false;

			id = *id_p;

			const auto username_len = span_to<u16>(body_, sizeof(id_type) + sizeof(CommandType));
			if (!username_len)
				return false;
			const auto uname = shrink_span(body_, sizeof(u16) + sizeof(id_type) + sizeof(CommandType), *username_len);
			username.assign(uname.begin(), uname.end());

			const auto pk = shrink_span(body_, sizeof(CommandType) + sizeof(id_type) + sizeof(u16) + *username_len);
			public_key.assign(pk.begin(), pk.end());
			return true;
		}

		[[nodiscard]] std::vector<u8> serialize() const noexcept
		{
			std::vector<u8> result{};
			result.reserve(size());

			auto id_span = to_span<u8>(id);
			auto username_len = static_cast<u16>(username.size());
			auto username_span = to_span<u8>(username_len);

			result.emplace_back(static_cast<u8>(command_id));
			result.insert(result.end(), id_span.begin(), id_span.end());
			result.insert(result.end(), username_span.begin(), username_span.end());
			result.insert(result.end(), public_key.begin(), public_key.end());

			return result;
		}

		[[nodiscard]] MessageType type() const noexcept { return MessageType::Command; }

		[[nodiscard]] constexpr usize size() const noexcept { return sizeof(id_type) + username.size() + public_key.size(); }
	};
}