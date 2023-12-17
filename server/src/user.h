#pragma once
#include <string>

#include "util/types.h"

namespace ar
{
	struct User
	{
		using public_key_type = std::vector<u8>;
		u64 key;
		std::string name;
		public_key_type public_key;
	};
}
