#pragma once
#include <asio/error_code.hpp>

namespace ar
{
	inline bool is_disconnect_error(const asio::error_code& ec_) noexcept
	{
		return ec_ == asio::error::eof
		|| ec_ == asio::error::connection_aborted
		|| ec_ == asio::error::connection_reset
		|| ec_ == asio::error::operation_aborted;
	}
}
