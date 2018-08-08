#pragma once

#include <memory>

#define ART_ENABLE_GUI

// Assertion that can be disabled using a compiled macro
#   ifdef ART_NOASSERT
#       define ART_ASSERT(x)
#   else
#       include <cassert>
#       define ART_ASSERT(x) assert(x)
#   endif


// Declare shared ptr types for non-templated type
#define DECLARE_PTR_TYPES(typeName) \
	typedef std::shared_ptr<typeName>			typeName ## _ptr; \
	typedef std::shared_ptr<const typeName>		typeName ## _cptr;

// Declare shared ptr types for templated type
#define DECLARE_PTR_TYPES_T1(typeName) \
	template<typename T> using typeName ## _ptr = std::shared_ptr<typeName<T>>; \
	template<typename T> using typeName ## _cptr = std::shared_ptr<const typeName<T>>;

#define DECLARE_PTR_TYPES_T2(typeName) \
	template<typename T1, typename T2> using typeName ## _ptr = std::shared_ptr<typeName<T1, T2>>; \
	template<typename T1, typename T2> using typeName ## _cptr = std::shared_ptr<const typeName<T1, T2>>;

#define STRMATCH(x, y) (strcmp(x, y) == 0)

