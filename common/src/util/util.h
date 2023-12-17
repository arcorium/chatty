#pragma once
#include <random>
#include <type_traits>

#include <cryptopp/elgamal.h>
#include <cryptopp/cryptlib.h>

#include "types.h"

namespace cry = ::CryptoPP;

namespace ar
{
	template<typename T, typename U>
	constexpr std::span<T, sizeof(U) / sizeof(T)> to_span(U& data_) noexcept
	{
		return std::span<T, sizeof(U) / sizeof(T)>{(T*)(&data_), sizeof(U) / sizeof(T)};
	}

	template<typename T>
	constexpr std::span<T> sv_to_span(std::string_view str_) noexcept
	{
		return {reinterpret_cast<T*>(str_.data()), reinterpret_cast<T*>(str_.data() + str_.size())};
	}

	template<typename T>
	constexpr T const* span_to(std::span<const u8> bytes_, usize offset_ = 0) noexcept
	{
		if (bytes_.size() < sizeof(T) + offset_)
			return nullptr;

		return (T const*)(bytes_.data() + offset_);
	}

	template<typename T>
	constexpr std::span<T> shrink_span(std::span<T> bytes_, usize offset_, usize count_ = 0) noexcept
	{
		if (!count_)
			return { bytes_.begin() + offset_ , bytes_.end() };
		return {bytes_.begin() + offset_, bytes_.begin() + offset_ + count_};	// TODO: Need + 1 for end?
	}

	template<typename T> requires std::is_integral_v<T> || std::is_floating_point_v<T>
	inline T generate_random_numbers(T min_ = std::numeric_limits<T>::min(), T max_ = std::numeric_limits<T>::max()) noexcept
	{
		static std::random_device device{};
		std::mt19937_64 rng{ device() };

		std::uniform_int_distribution<T> distribution{ min_, max_ };
		return distribution(rng);
	}

	template<typename T> requires std::is_integral_v<T> || std::is_floating_point_v<T>
	inline T encrypt_xor(T plain_, std::string_view key_) noexcept
	{
		if (key_.size() < sizeof(T))
			return plain_;

		auto plain = to_span<u8>(plain_);
		for (u8 i = 0; i < sizeof(T); ++i)
		{
			plain[i] ^= key_[i];
		}
		return plain_;
	}

	static std::tuple<cry::ElGamal::PrivateKey, cry::ElGamal::PublicKey> generate_keys(cry::RandomNumberGenerator& rng_)
	{
		cry::ElGamal::PrivateKey private_key;
		cry::ElGamal::PublicKey public_key;
		private_key.GenerateRandomWithKeySize(rng_, 512);
		private_key.MakePublicKey(public_key);

		return std::make_tuple(std::move(private_key), std::move(public_key));
	}

	static std::vector<u8> encrypt(cry::RandomNumberGenerator& rng_, const cry::ElGamal::Encryptor& encryptor_, std::string_view plain_text_) noexcept
	{
		const auto cipher_len = encryptor_.CiphertextLength(plain_text_.size());
		// cry::SecByteBlock cipher{ cipher_len };
		std::vector<u8> cipher{ cipher_len, std::allocator<u8>{} };
		encryptor_.Encrypt(rng_, reinterpret_cast<const byte*>(plain_text_.data()), plain_text_.size(), cipher.data());
		return cipher;
	}

	static std::vector<u8> encrypt(cry::RandomNumberGenerator& rng_, const cry::ElGamal::Encryptor& encryptor_, std::span<const u8> plain_) noexcept
	{
		const auto cipher_len = encryptor_.CiphertextLength(plain_.size());
		// cry::SecByteBlock cipher{ cipher_len };
		std::vector<u8> cipher{ cipher_len, std::allocator<u8>{} };
		encryptor_.Encrypt(rng_, plain_.data(), plain_.size(), cipher.data());
		return cipher;
	}

	static std::vector<u8> decrypt(cry::RandomNumberGenerator& rng_, const cry::ElGamal::Decryptor& decryptor_, std::span<const u8> cipher_) noexcept
	{
		const auto max_len = decryptor_.MaxPlaintextLength(cipher_.size());
		std::vector<u8> block{ max_len, std::allocator<u8>{} };

		const auto result = decryptor_.Decrypt(rng_, cipher_.data(), cipher_.size(), block.data());
		block.resize(result.messageLength);
		return block;
	}

	static std::vector<u8> save_public_key(const cry::ElGamal::PublicKey& key_) noexcept
	{
		std::vector<u8> output;
		key_.Save(cry::VectorSink{output}.Ref());
		return output;
	}

	static cry::ElGamal::PublicKey load_public_key(const std::vector<u8> key_) noexcept
	{
		cry::ElGamal::PublicKey key{};
		key.Load(cry::VectorSource{ key_, true }.Ref());
		return key;
	}

	inline std::string get_current_time() noexcept
	{
		auto time = std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
		return std::format("{:%X}", time);
	}
}
