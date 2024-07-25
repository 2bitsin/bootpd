#pragma once

#include <span>
#include <stdexcept>

template <typename T, typename Q>
requires (sizeof (Q) == 1)
auto take_one(std::span<const Q>& bits) -> const T&
{	
	if (sizeof(T) > bits.size())
		throw std::runtime_error("not enough bytes in span");
	T const *const value = reinterpret_cast<const T*>(bits.data());
	bits = bits.subspan(sizeof(T));
	return *value;
}