#ifndef UTILS_H
#define UTILS_H
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <stdint.h>
#include <stdint.h>

namespace Utils {
	inline std::string trim(const std::string& str, const std::string& whitespace = " \t")
	{
		const auto strBegin = str.find_first_not_of(whitespace);
		if (strBegin == std::string::npos)
			return "";

		const auto strEnd = str.find_last_not_of(whitespace);
		const auto strRange = strEnd - strBegin + 1;

		return str.substr(strBegin, strRange);
	}

	inline std::vector<std::string> split(const std::string& s, char delim)
	{
		std::vector<std::string> elems;
		std::stringstream ss(s);
		std::string item;
		while (std::getline(ss, item, delim)) {
			if (!item.empty())
				elems.emplace_back(item);
		}
		return elems;
	}

	inline std::vector<std::string> random_set(const std::vector<std::string>& list, size_t n)
	{
		std::vector<std::string> perm(list);
		std::random_shuffle(perm.begin(), perm.end());
		return std::vector<std::string>(perm.begin(), perm.begin() + n);
	}

	template<typename T> inline T make_ptr(void *ptr, size_t displ)
	{
		return reinterpret_cast<T>((uint8_t *)ptr + displ);
	}

	template<typename T1, typename T2> off_t make_offset(T1 p1, T2 p2)
	{
		return (off_t)(reinterpret_cast<uint8_t*>(p1)-reinterpret_cast<uint8_t*>(p2));
	}

	inline uint32_t rand_dword()
	{
		return (uint32_t)rand() << 16 | (uint16_t)rand();
	}

	inline uint64_t rand_qword()
	{
		return (uint64_t)rand_dword() << 32 | rand_dword();
	}
};

#endif