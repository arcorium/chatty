#pragma once
#include "types.h"
#include <type_traits>

namespace ar
{
	template <typename T>
	class ptr
	{
		static_assert(!std::is_reference_v<T>, "Type should not be a reference");

		using pointer = std::add_pointer_t<T>;
		using ref = std::add_lvalue_reference_t<T>;

	public:
		constexpr ptr() : m_ptr{nullptr} {}

		constexpr explicit ptr(ref ref_) : m_ptr{&ref_} {}

		constexpr ptr(pointer ptr_) : m_ptr{ptr_} {}

		constexpr explicit ptr(usize raw_) : m_ptr{reinterpret_cast<pointer>(raw_)} {}

		~ptr() = default;

		ptr(const ptr& other)
			: m_ptr(other.m_ptr)
		{
		}

		ptr(ptr&& other) noexcept
			: m_ptr(other.m_ptr)
		{
			other.m_ptr = nullptr;
		}

		ptr& operator=(const ptr& other)
		{
			if (this == &other)
				return *this;
			m_ptr = other.m_ptr;
			return *this;
		}

		ptr& operator=(ptr&& other) noexcept
		{
			if (this == &other)
				return *this;
			m_ptr = other.m_ptr;
			other.m_ptr = nullptr;
			return *this;
		}

		constexpr bool is_null() const
		{
			return m_ptr == nullptr;
		}

		constexpr operator bool() const
		{
			return !is_null();
		}

		constexpr bool operator!() const
		{
			return is_null();
		}

		pointer operator->()
		{
			return m_ptr;
		}

		const T* operator->() const
		{
			return m_ptr;
		}

		constexpr T& operator*() noexcept
		{
			return *m_ptr;
		}

		constexpr const T& operator*() const noexcept
		{
			return *m_ptr;
		}

	private:
		pointer m_ptr;
	};

	template<typename T>
	class ref
	{
		static_assert(!std::is_reference_v<T>, "Type should not be a reference.");
		static_assert(std::is_object_v<T>, "ref<T> requires T to be an object type.");

		using pointer = std::add_pointer_t<T>;
		using reference = std::add_lvalue_reference_t<T>;
		using const_reference = std::add_lvalue_reference_t<std::add_const_t<T>>;

	public:
		constexpr ref(T& ref_) : m_ptr{&ref_} {}
		~ref() = default;

		ref(ar::ref<T>&& other) noexcept = delete;
		ar::ref<T>& operator=(ar::ref<T>&& other) noexcept = delete;

		ref(const ar::ref<T>& other)
			: m_ptr(other.m_ptr)
		{
		}

		ar::ref<T>& operator=(const ar::ref<T>& other)
		{
			if (this == &other)
				return *this;
			m_ptr = other.m_ptr;
			return *this;
		}

		constexpr reference operator*() noexcept
		{
			return *m_ptr;
		}

		constexpr const_reference operator*() const noexcept
		{
			return *m_ptr;
		}

		constexpr operator reference() noexcept
		{
			return *m_ptr;
		}

		constexpr operator const_reference() const noexcept
		{
			return *m_ptr;
		}

		constexpr pointer operator->() noexcept
		{
			return m_ptr;
		}

		constexpr const T* operator->() const noexcept
		{
			return m_ptr;
		}

	private:
		pointer m_ptr;
	};

	template <typename T, bool RAII = false>
	class heap_ptr
	{
	public:
		using raw = std::remove_cvref_t<std::remove_pointer_t<T>>;
		using pointer = std::add_pointer_t<raw>;
		using const_pointer = std::add_pointer_t<std::add_const_t<raw>>;
		using ref = std::add_lvalue_reference_t<raw>;

	public:
		constexpr heap_ptr() : m_ptr{nullptr} {}

		constexpr heap_ptr(pointer heap_allocated_ptr_) : m_ptr{heap_allocated_ptr_} {}

		template <typename... Args>
		constexpr heap_ptr(Args&&... args_) : m_ptr{new T{std::forward<Args>(args_)...}} {}

		~heap_ptr()
		{
			if constexpr (RAII)
			{
				free();
			}
		}

		heap_ptr(const heap_ptr& other)
			: m_ptr{new T{}}
		{
			*m_ptr = *other.m_ptr; // deep copy
		}

		heap_ptr& operator=(const heap_ptr& other)
		{
			if (this == &other)
				return *this;
			m_ptr = new T{};
			*m_ptr = *other.m_ptr; // deep copy
			return *this;
		}

		heap_ptr(heap_ptr&& other) noexcept
			: m_ptr{other.m_ptr}
		{
			other.m_ptr = nullptr; // prevent delete on move
		}


		heap_ptr& operator=(heap_ptr&& other) noexcept
		{
			if (this == &other)
				return *this;
			m_ptr = other.m_ptr;
			other.m_ptr = nullptr; // prevent delete on move
			return *this;
		}

		void free()
		{
			if constexpr (std::is_array_v<raw>)
				delete[] m_ptr;
			else
				delete m_ptr;
			m_ptr = nullptr;
		}

		constexpr bool is_null() const
		{
			return m_ptr == nullptr;
		}

		operator bool() const
		{
			return !is_null();
		}

		pointer operator->()
		{
			return m_ptr;
		}

		const_pointer operator->() const
		{
			return m_ptr;
		}

	private:
		pointer m_ptr;
	};

	template <typename T, bool RAII>
	class heap_ptr<T[], RAII>
	{
		using raw = std::remove_cvref_t<std::remove_pointer_t<T>>;
		using pointer = std::add_pointer_t<raw>;
		using const_pointer = std::add_pointer_t<std::add_const_t<raw>>;
		using ref = std::add_lvalue_reference_t<raw>;
		using const_ref = std::add_lvalue_reference_t<std::add_const_t<raw>>;

	public:
		constexpr heap_ptr() : m_ptr{nullptr} {}

		constexpr heap_ptr(pointer heap_allocated_ptr_) : m_ptr{heap_allocated_ptr_} {}

		template <typename... Args>
		constexpr heap_ptr(Args&&... args_) : m_ptr{new T[sizeof...(Args)]{}}
		{
			static auto fn = [i = 0, this](auto&& param_) mutable
			{
				m_ptr[i++] = param_;
			};

			(fn(args_), ...);
		}

		constexpr heap_ptr(usize len_) : m_ptr{new T[len_]{}} {}

		template <usize Size>
		constexpr heap_ptr(T (&array_)[Size]) : m_ptr{new T[Size]{}}
		{
			for (usize i = 0; i < Size; ++i)
			{
				m_ptr[i] = array_[i];
			}
		}

		~heap_ptr()
		{
			if constexpr (RAII)
			{
				free();
			}
		}

		heap_ptr(const heap_ptr& other) noexcept = delete;

		heap_ptr& operator=(const heap_ptr& other) = delete;

		heap_ptr(heap_ptr&& other) noexcept
			: m_ptr{other.m_ptr}
		{
			other.m_ptr = nullptr; // prevent delete on move
		}


		heap_ptr& operator=(heap_ptr&& other) noexcept
		{
			if (this == &other)
				return *this;
			m_ptr = other.m_ptr;
			other.m_ptr = nullptr; // prevent delete on move
			return *this;
		}

		void free()
		{
			if constexpr (std::is_array_v<raw>)
				delete[] m_ptr;
			else
				delete m_ptr;
			m_ptr = nullptr;
		}

		constexpr bool is_null()
		{
			return m_ptr == nullptr;
		}

		operator bool() const
		{
			return !is_null();
		}

		pointer operator->()
		{
			return m_ptr;
		}

		const_pointer operator->() const
		{
			return m_ptr;
		}

		ref operator[](isize index_)
		{
			return m_ptr[index_];
		}

		const_ref operator[](isize index_) const
		{
			return m_ptr[index_];
		}

	private:
		pointer m_ptr;
	};
}
