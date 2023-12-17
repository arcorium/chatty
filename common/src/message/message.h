#pragma once
#include <vector>

#include "util/types.h"
#include <span>

#include <fmt/ranges.h>

#include "util/util.h"
#include "util/concept.h"
#include "util/literal.h"

namespace ar
{
	enum class MessageType : u8
	{
		Undefined,
		Validation,			// Used for validation scheme
		Authenticate,		// Used to authenticate each user
		Feedback,
		Chat,
		Command,
		UserDisconnect,		// Used when some user is disconnected
		NewUser,			// Used when some user is connected
		Close,				// Client wanted to close the Connection
	};

	enum class CommandType : u8
	{
		OnlineList,
		RequestPublicKey,
		RequestUserProperties
	};

	struct Message
	{
		struct Header
		{
			MessageType id;
			u32 body_size;
		} header;

		std::vector<u8> body;

		static inline constexpr usize header_size = sizeof(Header);

		explicit Message(MessageType type_, std::span<const u8> body_) : header{type_, static_cast<u32>(body_.size())}, body{body_.begin(), body_.end()}
		{
		}
		//
		// explicit Message(std::string_view raw_msg_) : header{MessageType::Raw, static_cast<u32>(raw_msg_.size())}, body{reinterpret_cast<const u8*>(raw_msg_.data()), reinterpret_cast<const u8*>(raw_msg_.data()) + header.body_size}
		// {
		// }

		template<Serializable T>
		explicit Message(const T& serializable_) : Message(serializable_.type(), serializable_.serialize())
		{
		}

		Message() : header{ MessageType::Undefined, 0 }
		{
		}

		//
		// explicit Message(MessageType type_) : header{ type_, 0}
		// {
		// }
		//


		bool parse_header(std::span<const u8> bytes_) noexcept
		{
			if (bytes_.size() < header_size)
				return false;
			Header temp;
			std::memcpy(&temp, bytes_.data(), header_size);
			header = temp;
			return true;
		}

		void resize_body() noexcept
		{
			body.resize(header.body_size);
		}

		void reset_body(std::span<const u8> bytes_) noexcept
		{
			resize_body();
			const auto begin = bytes_.begin() + header_size;
			body.assign(begin, begin + header.body_size);
		}

		usize total_size() const noexcept
		{
			return body.size() + header_size;
		}

		std::vector<u8> serialize() const noexcept
		{
			std::vector<u8> result{(sizeof(header) + body.size()), std::allocator<u8>{}};
			std::memcpy(result.data(), &header, sizeof(header));
			std::memcpy(result.data() + sizeof(header), body.data(), body.size());
			return result;
		}

		static std::optional<Message> deserialize(std::span<const u8> data_) noexcept
		{
			if (data_.size() < sizeof(Message))
				return std::nullopt;
			Message msg{};
			msg.parse_header(data_);
			msg.reset_body(data_);
			return msg;
		}

		[[nodiscard]] std::string string() const noexcept
		{
			return fmt::format("Id: {} | Body Size: {} | Body: {}", static_cast<i32>(header.id), header.body_size,
			                   body);
		}

		[[nodiscard]] MessageType type() const noexcept
		{
			return header.id;
		}

		template<Deserializable T>
		T body_as() const noexcept
		{
			T t{};
			t.deserialize(body);
			return t;
		}
	};

	struct ValidationMessage {
		u64 challenge;

		[[nodiscard]] std::vector<u8> serialize() const noexcept
		{
			auto span = to_span<u8>(challenge);
			return {span.begin(), span.end()};
		}

		[[nodiscard]] MessageType type() const noexcept { return MessageType::Validation; }

		[[nodiscard]] constexpr usize size() const noexcept { return sizeof(u64); }

		bool deserialize(std::span<const u8> body_) noexcept
		{
			if (body_.size() < sizeof(u64))
				return false;

			const auto temp = reinterpret_cast<const u64*>(body_.data());
			challenge = *temp;
			return true;
		}
	};

	// Payload: ##$...@...
	// #: username_len
	// $: username
	// @: public_key
	struct AuthenticateMessage
	{
		using public_key_type = std::vector<u8>;
		std::string username{};
		public_key_type public_key;

		[[nodiscard]] std::vector<u8> serialize() const noexcept
		{
			const auto size = static_cast<u16>(username.size());
			const auto size_span = to_span<u8>(size);

			std::vector<u8> result{};
			result.insert(result.end(), size_span.begin(), size_span.end());		// Length
			result.insert(result.end(), username.begin(), username.end());		// Username
			result.insert(result.end(), public_key.begin(), public_key.end());	// Public Key

			return result;
		}

		[[nodiscard]] MessageType type() const noexcept { return MessageType::Authenticate;}

		bool deserialize(std::span<const u8> body_) noexcept
		{
			if (body_.empty())
				return false;

			const auto username_len = span_to<u16>(body_);
			if (!username_len)
				return false;

			const auto uname = shrink_span(body_, sizeof(u16), *username_len);
			username.assign(uname.begin(), uname.end());

			const auto pk = shrink_span(body_, sizeof(u16) + *username_len);
			public_key.assign(pk.begin(), pk.end());
			return true;
		}

		constexpr usize size() const noexcept { return username.size() + public_key.size(); }
	};

	struct FeedbackMessage
	{
		u8 result;

		[[nodiscard]] std::vector<u8> serialize() const noexcept
		{
			return {{this->result}};
		}

		[[nodiscard]] MessageType type() const noexcept { return MessageType::Feedback; }

		bool deserialize(std::span<const u8> body_) noexcept
		{
			if (body_.size() < sizeof(u8))
				return false;

			result = body_[0];
			return true;
		}

		constexpr usize size() const noexcept { return sizeof(u8); }
	};

	enum class ChatOpponent : u8
	{
		Server,
		User
	};

	struct ChatMessage
	{
		using id_type = u32;

		ChatOpponent opponent;
		id_type opponent_id;
		std::vector<u8> message;

		static ChatMessage for_server(std::string_view message_) noexcept
		{
			return ChatMessage{ChatOpponent::Server, 0, {message_.begin(), message_.end()}};
		}

		static ChatMessage for_user(id_type id_, std::string_view message_) noexcept
		{
			return ChatMessage{ ChatOpponent::User, id_, {message_.begin(), message_.end() }};
		}

		bool deserialize(std::span<const u8> body_) noexcept
		{
			if (body_.size() < sizeof(u32))
				return false;

			const auto dest_type = span_to<u8>(body_);
			if (!dest_type)
				return false;
			const auto id = span_to<const u32>(body_, sizeof(ChatOpponent));
			if (!id)
				return false;
			opponent = static_cast<ChatOpponent>(*dest_type);
			opponent_id = *id;
			message.assign(body_.begin() + sizeof(ChatOpponent) + sizeof(u32), body_.end());
			return true;
		}

		[[nodiscard]] std::vector<u8> serialize() const noexcept
		{
			const usize len = message.size() + sizeof(opponent_id) + sizeof(ChatOpponent);
			const auto span = to_span<u8>(opponent_id);

			std::vector<u8> result{};
			result.reserve(len);
			result.emplace_back(static_cast<u8>(opponent));
			result.insert(result.end(), span.begin(), span.end());
			result.insert(result.end(), message.begin(), message.end());
			return result;
		}

		[[nodiscard]] MessageType type() const noexcept { return MessageType::Chat; }

		[[nodiscard]] constexpr usize size() const noexcept
		{
			return sizeof(ChatOpponent) + sizeof(opponent_id) + message.size();
		}

		[[nodiscard]] constexpr std::string message_str() const noexcept
		{
			return std::string{ message.begin(), message.end() };
		}

		void encrypt(cry::RandomNumberGenerator& rng_, const cry::ElGamal::PublicKey& public_key_) noexcept
		{
			const cry::ElGamal::Encryptor enc{public_key_};
			auto res = ::ar::encrypt(rng_, enc, message);
			message = std::move(res);
		}

		void decrypt(cry::RandomNumberGenerator& rng_, const cry::ElGamal::PrivateKey& private_key_) noexcept
		{
			const cry::ElGamal::Decryptor dec{private_key_};
			auto result = ::ar::decrypt(rng_, dec, message);
			message = std::move(result);
		}
	};

	struct CommandMessage
	{
		using parameter_type = u32;
		using command_arguments = std::vector<parameter_type>;

		CommandType command_type;
		command_arguments arguments;

		bool deserialize(std::span<const u8> body_) noexcept
		{
			if (body_.size() < size())
				return false;

			command_type = static_cast<CommandType>(body_[0]);
			arguments.assign(body_.begin() + sizeof(CommandType), body_.end());
			return true;
		}

		[[nodiscard]] std::vector<u8> serialize() const noexcept
		{
			const usize len = size();

			std::vector<u8> result{};
			result.reserve(len);
			result.emplace_back(static_cast<u8>(command_type));
			result.insert(result.end(), arguments.begin(), arguments.end());	// TODO: arguments is u32 instead of u8
			return result;
		}

		[[nodiscard]] MessageType type() const noexcept { return MessageType::Command; }

		[[nodiscard]] constexpr usize size() const noexcept { return arguments.size() * sizeof(parameter_type) + sizeof(CommandType); }
	};

	struct UserDisconnectMessage
	{
		using id_type = u32;

		id_type id;

		bool deserialize(std::span<const u8> body_) noexcept
		{
			if (body_.size() < size())
				return false;

			const auto id_span = span_to<u32>(body_);
			if (!id_span)
				return false;

			id = *id_span;
			return true;
		}

		[[nodiscard]] std::vector<u8> serialize() const noexcept
		{
			const auto id_span = to_span<u8>(id);

			std::vector<u8> result{};
			result.reserve(size());
			result.insert(result.end(), id_span.begin(), id_span.end());
			return result;
		}

		[[nodiscard]] MessageType type() const noexcept { return MessageType::UserDisconnect; }

		[[nodiscard]] constexpr usize size() const noexcept { return sizeof(id_type); }
	};

	struct NewUserMessage
	{
		using id_type = u32;

		id_type id;
		std::string name;

		bool deserialize(std::span<const u8> body_) noexcept
		{
			if (body_.size() < size())
				return false;

			const auto id_span = span_to<id_type>(body_);
			if (!id_span)
				return false;

			id = *id_span;

			auto name_span = shrink_span(body_, sizeof(id_type));
			name.assign(name_span.begin(), name_span.end());
			return true;
		}

		[[nodiscard]] std::vector<u8> serialize() const noexcept
		{
			const auto id_span = to_span<u8>(id);

			std::vector<u8> result{};
			result.reserve(size());
			result.insert(result.end(), id_span.begin(), id_span.end());
			result.insert(result.end(), name.begin(), name.end());
			return result;
		}

		[[nodiscard]] MessageType type() const noexcept { return MessageType::NewUser; }

		[[nodiscard]] constexpr usize size() const noexcept { return sizeof(id_type) + name.size(); }
	};
}
