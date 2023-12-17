#pragma once
#include <string>
#include <cryptopp/elgamal.h>

namespace ar
{
	namespace cry = CryptoPP;

	struct User
	{
		using public_key_type = cry::ElGamal::PublicKey;

		bool has_key{false};
		std::string name;
		public_key_type public_key;
	};
}
