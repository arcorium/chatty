#pragma once
#include <shared_mutex>
#include <deque>

namespace ar
{
	template<typename T>
	class ts_queue
	{
	public:
		ts_queue() = default;

		void push_back(const T& data_);
		void push_back(T&& data_);

		void pop_front();

		bool is_empty() const;

		T& front();
		T& back();

	private:
		std::shared_mutex m_mutex;
		std::deque<T> m_data;
	};

	template <typename T>
	void ts_queue<T>::push_back(const T& data_)
	{
		
		std::scoped_lock{ m_mutex };
		m_data.push_back(data_);
	}

	template <typename T>
	void ts_queue<T>::push_back(T&& data_)
	{
		std::scoped_lock{ m_mutex };
		m_data.push_back(std::forward<T>(data_));
	}

	template <typename T>
	void ts_queue<T>::pop_front()
	{
		std::scoped_lock{ m_mutex };
		m_data.pop_front();
	}

	template <typename T>
	bool ts_queue<T>::is_empty() const
	{
		std::shared_lock{ m_mutex };
		return m_data.empty();
	}

	template <typename T>
	T& ts_queue<T>::front()
	{
		std::shared_lock{ m_mutex };
		auto& item = m_data.front();
		return item;
	}

	template <typename T>
	T& ts_queue<T>::back()
	{
		std::shared_lock{ m_mutex };
		auto& item = m_data.back();
		return item;
	}
}

