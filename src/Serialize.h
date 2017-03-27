// This header contains our serialization system
//
// It is a declarative system which makes reading and writing serialized data
// a matter of building a tree which models the serialized data's structure.
#pragma once

#include "default.h"
#include <functional>
#include <unordered_map>
#include <type_traits>

namespace unitypack {
namespace serialize {

struct SerializerBase {
	// Begin visits a new child node in the tree
	void Begin(const char *typeName, const char *name, int flags);
	// End goes up a level in the tree
	void End();

	// BeginIf returns true and visits a new child node when conditon(varName) is true.
	bool BeginIf(const char *varName, const char *condStr, std::function<bool(int)> condition);

	// BeginElse return true and visits a new child node when the previous sibling if node was false.
	bool BeginElse();

	// SetVariable configures the current node to set a variable to a constant value.
	void SetVariable(const char *varName, int value) {
		variables[varName] = value;
	}

	// Scalar makes the current node serialize a scalar value such as int or float.
	template <typename T>
	void Scalar(T &value) {}

	// Called by Scalar to give the node a data pointer and size so that the data may be used later.
	void RecordScalar(void *data, int size);

	bool IsBigEndian() const;
	bool IsErrored() const {
		return errored;
	}

	struct Node {
		const char *typeName;
		const char *name;
		void *data;
		int size;
		int flags;
	};
	std::vector<SerializerBase::Node> stack;
	std::unordered_map<const char *, int> variables;
	uint32_t bigEndian : 1;
	uint32_t conditionWasTrue : 1;
	uint32_t errored : 1;
	uint32_t eof : 1;
};

// This macro declares the Serialize member function, making a struct serializable.
#define SerializeFn(typeName) \
	static constexpr const char *type_string = #typeName; \
	template <typename Serializer> \
	void Serialize(Serializer &s)

// These macros are used primarily to get strings for typeName and name.
// They assume "Serializer &s" is present in the scope.
#define SerializeScalarV(typeName, name, ...)  unitypack::serialize::scalar_val(s, name, #typeName, #name, __VA_ARGS__)
#define SerializeScalar(typeName, name)        unitypack::serialize::scalar_val(s, name, #typeName, #name)
#define SerializeStructV(typeName, name, ...)  unitypack::serialize::struct_val(s, name, #typeName, #name, __VA_ARGS__)
#define SerializeStruct(typeName, name)        unitypack::serialize::struct_val(s, name, #typeName, #name)

// Usage: SerializeIf(variable, variable > 0, [&](){ ... }, [&](){ ... })
#define SerializeIf(varName, condition, ...) \
	unitypack::serialize::ifcond(s, #varName, \
	#condition, [](int varName) -> bool { return condition; }, \
	__VA_ARGS__)

enum Flags {
	// This scalar value should always be serialized big-endian
	BigEndian = 0x1,
	// This integral value is a variable with which conditional serialization logic is defined,
	// such as a version number.
	Variable = 0x2,
	// This boolean value determines the endianness of the rest of the stream.
	BigEndianWhenTrue = 0x4,
	// This std::string value should be serialized as a null-terminated string, not as an array of char.
	CString = 0x8,
	// This node is host to an array
	Array = 0x10,
	// This node is host to the taken branch of a conditional expression
	ConditionalIf = 0x20,
	// This node is host to the taken branch of a conditional expression
	ConditionalElse = 0x40,
	// This node has a scalar value which should serialize exactly 4 bytes
	ValueIs32Bit = 0x80,
	// Align the stream to the next 4-byte boundary before serializing this node
	PreAlign = 0x2000,
	// Align the stream to the next 4-byte boundary after serializing this node
	PostAlign = 0x4000,
};

template <typename T>
void ByteSwap(void *v) {
	static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8,
		"ByteSwap expects sizeof(T) to be 1, 2, 4, or 8");
	static_assert(std::is_integral<T>::value || std::is_floating_point<T>::value,
		"T should be an integer or a float");
	switch (sizeof(T)) {
	case 2:
		uint16_t v16;
		memcpy(&v16, v, sizeof(v16));
		v16 =
			((v16 & 0x00ffu) << 8u) |
			((v16 & 0xff00u) >> 8u);
		memcpy(v, &v16, sizeof(v16));
		break;
	case 4:
		uint32_t v32;
		memcpy(&v32, v, sizeof(v32));
		v32 =
			((v32 & 0x000000ffu) << 24u) |
			((v32 & 0x0000ff00u) <<  8u) |
			((v32 & 0x00ff0000u) >>  8u) |
			((v32 & 0xff000000u) >> 24u);
		memcpy(v, &v32, sizeof(v32));
		break;
	case 8:
		uint64_t v64;
		memcpy(&v64, v, sizeof(v64));
		v64 =
			((v64 & 0x00000000000000ffu) << 56u) |
			((v64 & 0x000000000000ff00u) << 40u) |
			((v64 & 0x0000000000ff0000u) << 24u) |
			((v64 & 0x00000000ff000000u) <<  8u) |
			((v64 & 0x000000ff00000000u) >>  8u) |
			((v64 & 0x0000ff0000000000u) >> 24u) |
			((v64 & 0x00ff000000000000u) >> 40u) |
			((v64 & 0xff00000000000000u) >> 56u);
		memcpy(v, &v64, sizeof(v64));
		break;
	}
}

struct BinaryReader : SerializerBase {
	FILE *stream;
	std::string cstring;
	int stringIndex;

	template <typename T>
	void Scalar(T &value) {
		RecordScalar(&value, sizeof(T));
		size_t stackSize = stack.size();
		if (stackSize >= 3) {
			// CString handling:
			// std::string str
			//   Array Array
			//     int size
			//     char data
			// the string data is stored in this->cstring
			auto &stringNode = stack[stackSize - 3];
			if (stringNode.flags & Flags::CString) {
				if (std::is_same<T, int>::value) {
					// we read the string here
					cstring.clear();
					while (true) {
						char c = getc(stream);
						if (c > 0) {
							cstring.push_back(c);
						} else {
							break;
						}
					}
					int size = (int)cstring.size();
					stringIndex = 0;
					// Use memcpy to avoid compile errors with other types
					memcpy(&value, &size, sizeof(int));
					return;
				}
				if (std::is_same<T, char>::value) {
					char c = cstring[stringIndex++];
					memcpy(&value, &c, sizeof(char));
					return;
				}
			}
		}

		size_t sz = sizeof(T);
		if (stackSize >= 1) {
			auto &node = stack[stackSize - 1];
			if (node.flags & Flags::PreAlign) {
				Align();
			}
			if (node.flags & Flags::ValueIs32Bit) {
				assert(sz >= 4);
				sz = 4;
			}
		}
		if (!fread(&value, sz, 1, stream)) {
			errored = true;
			if (feof(stream)) {
				eof = true;
			}
		} else {
			if (IsBigEndian()) {
				if (sz == 4) {
					ByteSwap<uint32_t>(&value);
				} else {
					ByteSwap<T>(&value);
				}
			}
		}

		if (stackSize >= 1) {
			auto &node = stack[stackSize - 1];
			if (node.flags & Flags::PostAlign) {
				Align();
			}
		}
	}

	void Align() {
		int offset = ftell(stream);
		int alignment = ((offset + 3) & -4) - offset;
		if (alignment > 0) {
			fseek(stream, alignment, SEEK_CUR);
		}
	}
};

template <typename T, typename Serializer>
void serialize(Serializer &s, T &field) {
	field.Serialize(s);
}

template <typename T, typename Serializer>
void scalar_val(Serializer &s, T &field, const char *typeName, const char *name, int flags = 0) {
	s.Begin(typeName, name, flags);
	s.Scalar(field);
	s.End();
}

template <typename T, typename Serializer>
void struct_val(Serializer &s, T &field, const char *typeName, const char *name, int flags = 0) {
	s.Begin(typeName, name, flags);
	serialize(s, field);
	s.End();
}

template <typename Serializer>
void ifcond(Serializer &s, const char *varName, const char *condStr,
	std::function<bool(int)> condition,
	std::function<void()> thenLambda,
	std::function<void()> elseLambda = nullptr)
{
	if (s.BeginIf(varName, condStr, condition)) {
		thenLambda();
		s.End();
	}
	if (elseLambda && s.BeginElse()) {
		elseLambda();
		s.End();
	}
}

template <typename Serializer>
void begin_array(Serializer &s, int &size) {
	s.Begin("Array", "Array", Flags::Array);
	s.Begin("int", "size", 0);
	s.Scalar(size);
	s.End();
}

template <typename Serializer>
void serialize(Serializer &s, std::string &field) {
	int size = (int)field.size();
	begin_array(s, size);
	field.resize(size);
	for (int i = 0; i < size; i++) {
		if (s.IsErrored()) break;
		s.Begin("char", "data", 0);
		s.Scalar(field[i]);
		s.End();
	}
	s.End();
}

template <typename T>
const char *type_string() {
	return T::type_string;
}

template <> const char *type_string<int>();

template <typename U, typename Serializer>
void serialize(Serializer &s, std::vector<U> &field) {
	int size = (int)field.size();
	begin_array(s, size);
	field.resize(size);
	for (int i = 0; i < size; i++) {
		if (s.IsErrored()) break;
		s.Begin(type_string<U>(), "data", 0);
		serialize(s, field[i]);
		s.End();
	}
	s.End();
}

};
};
