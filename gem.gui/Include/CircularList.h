
#pragma once

#include <vector>
#include <optional>

template <class TElem>
class CircularList
{
public:
	CircularList();
	CircularList(int capacity);
	void Add(TElem& elem);
	void Clear();
	const TElem& operator[](int idx) const;

	bool IsWrappedAround() const { return idxStart > idxEnd; }
	int Count() const { return count; }
	int Capacity() const { return list.size(); }
	int StartIndex() const { return idxStart; }
	int EndIndex() const { return idxEnd; }

private:
	int idxStart;
	int idxEnd;
	int count;
	std::vector<std::optional<TElem>> list;
};

template <class TElem>
CircularList<TElem>::CircularList()
	: idxStart(0)
	, idxEnd(0)
	, count(0)
{
}

template <class TElem>
CircularList<TElem>::CircularList(int capacity)
	: list(capacity)
	, idxStart(0)
	, idxEnd(0)
	, count(0)
{
}

template <class TElem>
void CircularList<TElem>::Add(TElem& elem)
{
	if (count == 0)
	{
		list[idxEnd] = std::move(elem);
		idxEnd++;
		count++;
	}
	else if (idxEnd == idxStart)
	{
		// Destroy elem at idxStart and overwrite with new elem
		list[idxStart].reset();
		idxStart = (idxStart + 1) % list.size();

		list[idxEnd] = std::move(elem);
	}
	else
	{
		list[idxEnd] = std::move(elem);
		idxEnd = (idxEnd + 1) % list.size();
		count++;
	}
}

template <class TElem>
void CircularList<TElem>::Clear()
{
	for (std::optional<TElem>& elem : list)
	{
		elem.reset();
	}
}

template <class TElem>
const TElem& CircularList<TElem>::operator[](int idx) const
{
	return list[idx].value();
}