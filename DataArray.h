#pragma once
/*
	UNFINISHED and UNUSED
	Experiemental* Contiguous memory storage of arbitrary types with resusable stable indices.

	Insert: O(m) worst case, constant on average
	Free:   O(m) worst case, constant on average
	TryGet: O(m) worst case, constant on average

	Where m is the size of m_FreeList (std::unordered_set)

	Note: 
		- double frees do nothing

	TODO: 
		- easier looping/iteration 
		- add Clear() function

	Inspired by: https://gamedev.stackexchange.com/a/33918

	-KHY
*/

#include <vector>
#include <unordered_set>
#include <mutex>

template<class T>
class DataArray {
public:
	DataArray(uint32_t maxSize) : m_MaxSize(maxSize), m_NextMaxIndex(0) {
		// Do not use reserve, we want to make sure indices in [0, maxSize) are valid
		m_Data.resize(maxSize);
	}

	// tArgs parameter are construction arguments for T
	// m_Data (std::vector) will throw out of range exception when it is full and no ids are available in m_FreeList
	template<class ...TArgs>
	T& Insert(TArgs&&... tArgs) {
		std::lock_guard<std::mutex> lock(m_Mutex);

		int newIndex {};
		if(m_FreeList.size() == 0) {
			newIndex = m_NextMaxIndex++;
		}
		else {
			newIndex = *(m_FreeList.begin());
			m_FreeList.erase(newIndex);
		}

		m_Count++;

		// Use subscript operator here and not emplace to utilize range checking from std::vector
		return m_Data[newIndex] = T { std::forward<TArgs>(tArgs)... };
	}

	// obj is assumed to be stored in m_Data.
	// Note: Does not check for double frees.
	void Free(const T& obj) {
		std::lock_guard<std::mutex> lock(m_Mutex);

		uint32_t index = static_cast<uint32_t>(&obj - m_Data.data());

		assert(index < m_MaxSize);

		m_Count--;

		m_FreeList.insert(index); // nothing happens if index is already in m_FreeList
	}

	// Intended for validating objects while looping
	T* TryGet(uint32_t index) {
		if(m_FreeList.find(index) != m_FreeList.end()) {
			return nullptr;
		}
		else {
			return &m_Data[index];
		}
	}

	// For looping, m_NextIndex represents max index ever used, no need to loop past that value.
	// Each indexed element in loop should be validated with TryGet(...) function.
	// NOTE: because m_NextIndex never decrements in current implementation unnecesary looping will occur
	uint32_t Size() const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_NextMaxIndex;
	}

	uint32_t Count() const {
		std::lock_guard<std::mutex> lock(m_Mutex);
		return m_Count;
	}

private:
	uint32_t m_NextMaxIndex; // next available index that's not in m_FreeList, m_NextIndex never decrements
	uint32_t m_MaxSize;      // Total size of internal data array (m_Data)
	uint32_t m_Count;        // number of active items

	std::vector<T> m_Data {};
	std::unordered_set<uint32_t> m_FreeList {}; // stores indices with invalidated objects (i.e. index is ready to be reused)

	mutable std::mutex m_Mutex;
};