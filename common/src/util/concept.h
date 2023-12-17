#pragma once
#include <type_traits>

namespace ar
{
	enum class MessageType : u8;
}

namespace ar {
	template<typename T>
	concept Serializable = requires(T t)
	{
		{t.serialize()} noexcept -> std::same_as<std::vector<u8>>;
		{t.size()} noexcept -> std::same_as<usize>;
		{t.type()} noexcept -> std::same_as<MessageType>;
	};

	template<typename T>
	concept Deserializable = requires(T t, std::span<const u8> data_)
	{
		std::is_default_constructible_v<T>;
		{t.deserialize(data_)} noexcept -> std::same_as<bool>;
		// {T::parse_this(a_)} noexcept -> std::same_as<T>;		// TODO: Use static method or constructible by span<const u8> to return itself, instead of default constructed first
	};

	// template<typename T>
	// concept Encryptable = requires(T t)
	// {
	// 	{t.encrypt()} noexcept -> std::same_as<std::vector<u8>>;
	// 	{t.decrypt()} noexcept -> std::same_as<std::vector<u8>>;
	// };
}
