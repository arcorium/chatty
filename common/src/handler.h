#pragma once
#include "connection_status.h"
#include "util/pointer.h"

namespace ar
{
	template<ConnectionType Owner>
	class Connection;
	
	struct Message;
	
	template<ConnectionType Owner>
	class IMessageHandler
	{
	public:
		virtual ~IMessageHandler() = default;
		virtual void on_new_in_message(Connection<Owner>& conn_, const Message& message_) noexcept = 0;
		virtual void on_new_out_message(Connection<Owner>& conn_, std::span<const u8> message_) noexcept = 0;
	};


	template<ConnectionType Owner>
	class IConnectionValidator
	{
	public:
		virtual ~IConnectionValidator() = default;

		// Send challenge
		virtual void start_validation(Connection<Owner>& conn_) noexcept = 0;
		// Retrieve answer and send 0 or 1
		virtual void validate(Connection<Owner>& conn_, const Message& msg_) noexcept = 0;
	};
	
	class IConnectionHandler : public IConnectionValidator<ConnectionType::Server>
	{
	public:
		using connection_type = Connection<ConnectionType::Server>;
		using connection_ptr = std::add_pointer_t<connection_type>;

		virtual connection_ptr add_connection(asio::ip::tcp::socket&& socket_, ref<IMessageHandler<ConnectionType::Server>> message_handler_) noexcept = 0;
		virtual void remove_connection(connection_type& conn_) noexcept = 0;

		virtual std::span<connection_ptr> connections() noexcept = 0;
		virtual connection_ptr connection(u32 id_) noexcept = 0;
	};

	template<typename T, ConnectionType Owner>
	concept Validator = std::is_default_constructible_v<T> && requires(T t, Connection<Owner>& conn_, const Message& msg_)
	{
		{t.on_validation(conn_, msg_)} -> std::same_as<void>;
		{t.validate(conn_, msg_)} -> std::same_as<bool>;
	};

	template<ConnectionType Owner>
	class EmptyValidator : public IConnectionValidator<Owner>
	{
	public:
		void start_validation(Connection<Owner>& conn_) noexcept override {}
		void validate(Connection<Owner>& conn_, const Message& msg_) noexcept override {}
	};
}
