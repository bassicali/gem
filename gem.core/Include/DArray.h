
#pragma once

#include <exception>
#include <algorithm>
#include <iterator>

#define _DARRAY_INITIAL_ELEM_CAPACITY 2

// Used as a general purpose container. It can act like a normal array
// with auto adjusted size or as a list where indices can't be accessed
// unless PushBack() is used to add something to that index.

#define CHECK(expr, msg) if (!(expr)) { throw std::exception(msg); } while(0)

template <class T>
class DArray
{

protected:
	uint32_t numElements;
	uint32_t numSpots;
	T* pData;

public:
	DArray() 
		: pData(nullptr)
		, numElements(0)
		, numSpots(0)
	{
	}

	DArray(uint32_t numSpots) 
		: pData(nullptr)
		, numElements(0)
		, numSpots(numSpots)
	{
	}

	DArray(uint32_t numSpots, bool reserve_now) 
		: pData(nullptr)
		, numElements(0)
		, numSpots(numSpots)
	{
		if (reserve_now)
		{
			Allocate();
			Reserve();
		}
	}

	DArray(const DArray& other)
	{
		pData = (T*)malloc(other.Size());
		CHECK(pData != nullptr, "Could not allocate memory for DArray");
		
		memcpy(pData, other.pData, other.Size());
		numSpots = other.numSpots;
	}

	DArray(DArray&& other)
	{
		pData = other.pData;
		numSpots = other.numSpots;
		numElements = other.numElements;
		other.pData = nullptr;
		other.numSpots = 0;
		other.numElements = 0;
	}

	DArray& operator=(const DArray& other)
	{
		pData = (T*)malloc(other.Size());
		CHECK(pData != nullptr, "Could not allocate memory for DArray");
		
		memcpy(pData, other.pData, other.Size());
		numSpots = other.numSpots;
	}

	DArray& operator=(DArray&& other)
	{
		pData = other.pData;
		numSpots = other.numSpots;
		numElements = other.numElements;
		other.pData = nullptr;
		other.numSpots = 0;
		other.numElements = 0;
		return *this;
	}

	void Swap(DArray& other)
	{
		T* temp_pData = pData;
		uint32_t temp_numElements = numElements;
		uint32_t temp_spots = numSpots;

		pData = other.pData;
		numSpots = other.numSpots;
		numElements = other.numElements;

		other.pData = temp_pData;
		other.numSpots = temp_numElements;
		other.numElements = temp_spots;
	}

	T& operator[](int index)
	{
		CHECK(pData != nullptr, "No memory allocated");
		CHECK(IsIndexValid(index), "Index out of range");

		return pData[index];
	}

	const T& operator[](int index) const
	{
		CHECK(pData != nullptr, "No memory allocated");
		CHECK(IsIndexValid(index), "Index out of range");

		return pData[index];
	}

	~DArray()
	{
		if (pData != nullptr)
		{
			delete[] pData;
		}
	}

	void Reserve()
	{
		numElements = numSpots;
	}

	void Allocate()
	{
		CHECK(pData == nullptr, "Memory already allocated");
		CHECK(numSpots > 0, "Allocation size must be greater than 0");

		pData = new T[numSpots];
		CHECK(pData != nullptr, "Could not allocate memory for DArray");
	}

	void Free()
	{
		if (!pData)
			return;

		delete[] pData;
		pData = nullptr;
		numElements = 0;
		numSpots = 0;
	}

	bool SetCapacity(uint32_t new_spots)
	{
		if (new_spots > numSpots)
		{
			T* new_ptr = new T[new_spots];

			memcpy(new_ptr, pData, numSpots * sizeof(T));
			delete[] pData;

			pData = new_ptr;
			numSpots = new_spots;

			return true;
		}
		else if (new_spots < numSpots)
		{
			T* new_ptr = new T[new_spots];

			memcpy(new_ptr, pData, new_spots * sizeof(T));
			delete[] pData;

			numElements = min(numElements, new_spots);
			pData = new_ptr;
			numSpots = new_spots;

			return true;
		}

		return false;
	}

	bool Grow(uint32_t additional_spots)
	{
		return SetCapacity(numSpots + additional_spots);
	}

	// Reduce size of allocation while potentially also discarding elements;
	bool Shrink(uint32_t remove_spots)
	{
		return SetCapacity(numSpots - remove_spots);
	}

	void PushBack(T value)
	{
		CHECK(pData != nullptr, "No memory allocated");

		if (numElements >= numSpots)
		{
			Grow(2);
		}

		pData[numElements++] = value;
	}

	// Discard elements (i.e. Count() returns new_numElements from now on)
	void Truncate(uint32_t new_numElements)
	{
		CHECK(pData != nullptr, "No memory allocated");
		CHECK(new_numElements <= numSpots, "Allocation is smaller than truncation size");

		numElements = new_numElements;
	}

	void Fill(const T& value)
	{
		CHECK(pData != nullptr, "No memory allocated");

		for (uint32_t i = 0; i < numSpots; i++)
			pData[i] = value;
		
		numElements = numSpots;
	}

	bool const IsAllocated() const { return pData != nullptr; }
	bool const IsIndexValid(uint32_t index) const { return index >= 0 && index < numElements; }
	const uint32_t Count() const { return numElements; }
	const void SetCount(int new_numElements) { numElements = new_numElements; }
	const uint32_t Capacity() const { return numSpots; }
	const uint32_t Size() const { return numSpots * sizeof(T); }
	const T* Ptr() const { return pData; }
	T* Ptr() { return pData; }
};
