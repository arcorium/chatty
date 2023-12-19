#pragma once

#include <asio.hpp>
#include <cryptopp/osrng.h>
#include <spdlog/spdlog.h>

#include "connection.h"

#include "server.h"

namespace ar
{
	class ConnectionManager;

	class SimpleServer : public IServer
	{
	public:
		SimpleServer(const asio::ip::tcp::endpoint& ep_);

		void on_new_in_message(connection_type& conn_, const Message& message_) noexcept override;
		
	private:
		void on_new_out_message(connection_type& conn_, std::span<const u8> message_) noexcept override {}

		bool on_new_connection(connection_type& conn_) noexcept override { return true; }

	private:
		ref<ConnectionManager> m_connection_manager;

		cry::AutoSeededRandomPool m_rng{};

		cry::ElGamal::PrivateKey m_private_key;
		cry::ElGamal::PublicKey m_public_key;
	};

}