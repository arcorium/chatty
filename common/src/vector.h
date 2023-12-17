#pragma once
#include <vector>
#include <shared_mutex>

namespace ar
{
	template<typename T>
	class ts_vector 
	{ 
	public: 
		ts_vector() noexcept = default;

		ts_vector(const ts_vector& other) = delete;
		ts_vector& operator=(const ts_vector& other) = delete;

		ts_vector(ts_vector&& other) noexcept
			: m_mutex(std::move(other.m_mutex)),
			  m_data(std::move(other.m_data))
		{
		}
		ts_vector& operator=(ts_vector&& other) noexcept
		{
			if (this == &other)
				return *this;
			m_mutex = std::move(other.m_mutex);
			m_data = std::move(other.m_data);
			return *this;
		}

		void push_back(const T& val_) noexcept;
		T& back() noexcept;
		T pop_back() noexcept;
		void clear() noexcept;

		bool is_empty() noexcept;

	private:
		std::shared_mutex m_mutex;
		std::vector<T> m_data;
	};
}
