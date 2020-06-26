
#pragma once

#include <exception>

#define _DARRAY_INITIAL_ELEM_CAPACITY 2

// Used as a general purpose container. It can act like a normal array
// with auto adjusted size or as a list where indices can't be accessed
// unless PushBack() is used to add something to that index.

template <class T>
class DArray
{
public:
	DArray() : data(nullptr), count(0), capacity(0)
	{
	}

	DArray(uint32_t capacity) : data(nullptr), count(0), capacity(capacity)
	{
	}

	DArray(uint32_t capacity, bool reserve_now) : data(nullptr), count(0), capacity(capacity)
	{
		if (reserve_now)
		{
			Allocate();
			Reserve();
		}
	}

	DArray(const DArray& other)
	{
		data = (T*)malloc(other.Size());
		if (data == nullptr)
			throw std::exception("Could not allocate memory for DArray");
		
		memcpy(data, other.data, other.Size());
		capacity = other.capacity;
	}

	DArray(DArray&& other)
	{
		data = other.data;
		capacity = other.capacity;
		count = other.count;
		other.data = nullptr;
		other.capacity = 0;
		other.count = 0;
	}

	DArray& operator=(const DArray& other)
	{
		data = (T*)malloc(other.Size());
		if (data == nullptr)
			throw std::exception("Could not allocate memory for DArray");
		
		memcpy(data, other.data, other.Size());
		capacity = other.capacity;
	}

	DArray& operator=(DArray&& other)
	{
		data = other.data;
		capacity = other.capacity;
		count = other.count;
		other.data = nullptr;
		other.capacity = 0;
		other.count = 0;
		return *this;
	}

	void Swap(DArray& other)
	{
		T* temp_data = data;
		uint32_t temp_count = count;
		uint32_t temp_capacity = capacity;

		data = other.data;
		capacity = other.capacity;
		count = other.count;

		other.data = temp_data;
		other.capacity = temp_count;
		other.count = temp_capacity;
	}

	T& operator[](int index)
	{
		if (data == nullptr)
			throw std::exception("No memory allocated");

		if (!IsIndexValid(index))
			throw std::exception("Index out of range");

		return data[index];
	}

	const T& operator[](int index) const
	{
		if (data == nullptr)
			throw std::exception("No memory allocated");

		if (!IsIndexValid(index))
			throw std::exception("Index out of range");

		return data[index];
	}

	~DArray()
	{
		if (data != nullptr)
		{
			delete[] data;
		}
	}

	void Reserve()
	{
		count = capacity;
	}

	void Allocate()
	{
		if (data != nullptr)
			throw std::exception("Memory already allocated");

		if (capacity == 0)
			throw std::exception("Allocation size must be greater than 0");

		data = new T[capacity];
		if (data == nullptr)
			throw std::exception("Could not allocate memory for DArray");
	}

	void PushBack(T value)
	{
		if (data == nullptr)
			throw std::exception("No memory allocated");

		if (count >= capacity)
		{
			T* new_ptr = new T[2 * capacity];
			memcpy(new_ptr, data, capacity * sizeof(T));
			delete [] data;

			data = new_ptr;
			capacity *= 2;
		}

		data[count++] = value;
	}

	void Truncate(uint32_t remainder)
	{
		if (data == nullptr)
			throw std::exception("No memory allocated");

		if (remainder > capacity)
			throw std::exception("Allocation is smaller than truncation size");

		count = remainder;
	}

	void Fill(T value)
	{
		if (data == nullptr)
			throw std::exception("No memory allocated");

		for (uint32_t i = 0; i < capacity; i++)
			data[i] = value;
		
		count = capacity;
	}

	bool const IsAllocated() const { return data != nullptr; }
	bool const IsIndexValid(uint32_t index) const { return index >= 0 && index < count; }
	const uint32_t Count() const { return count; }
	const uint32_t Capacity() const { return capacity; }
	const uint32_t Size() const { return capacity * sizeof(T); }
	const T* Ptr() const { return data; }
	T* Ptr() { return data; }

protected:
	uint32_t count;
	uint32_t capacity;
	T* data;
};
