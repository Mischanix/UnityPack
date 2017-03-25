#include <functional>

#define SerializeScalarV(typeName, name, ...)  unitypack::serialize::scalar(s, name, #typeName, #name, __VA_ARGS__)
#define SerializeScalar(typeName, name)        unitypack::serialize::scalar(s, name, #typeName, #name)
#define SerializeStructV(typeName, name, ...)  unitypack::serialize::struct(s, name, #typeName, #name, __VA_ARGS__)
#define SerializeStruct(typeName, name)        unitypack::serialize::struct(s, name, #typeName, #name)

#define SerializeArray(typeName, name, lambda) unitypack::serialize::array(s, name, #typeName, #name, lambda)

#define SerializeIf(varName, condition, thenLambda, elseLambda) \
	unitypack::serialize::ifcond(s, #varName, \
	#condition, [](varName) -> bool { return condition; }, \
	thenLambda, elseLambda)

namespace unitypack {
namespace serialize {

enum class Flags {
	// Align the stream to the next 4-byte boundary after serializing this node
	PostAlign = 1,
	// This scalar value should always be serialized big-endian
	BigEndian = 0x10000,
	// This integral value is a variable with which conditional serialization logic is defined,
	// such as a version number.
	Variable = 0x20000,
	// This boolean value determines the endianness of the rest of the stream.
	BigEndianWhenTrue = 0x40000,
	// This std::string value should be serialized as a null-terminated string, not as an array of char.
	CString = 0x80000,
	// This node is host to a conditional expression
	Conditional = 0x100000,
};

template <typename T, typename Serializer>
void serialize(Serializer &s, T &field) {
	s.Serialize(field);
}

template <typename T, typename Serializer>
void scalar(Serializer &s, T &field, const char *typeName, const char *name, int flags = 0) {
	s.Begin(typeName, name, flags);
	s.Scalar(field);
	s.End();
}

template <typename T, typename Serializer>
void struct(Serializer &s, T &field, const char *typeName, const char *name, int flags = 0) {
	s.Begin(typeName, name, flags);
	serialize(s, field);
	s.End();
}

template <typename Serializer>
void ifcond(Serializer &s, const char *varName, const char *condStr,
	std::function<bool(int)> condition,
	std::function<void(Serializer &)> thenLambda,
	std::function<void(Serializer &)> elseLambda = nullptr)
{
	if (s.BeginIf(varName, condStr, condition)) {
		thenLambda(s);
		s.End();
	}
	if (elseLambda && s.BeginElse(varName, condStr, condition)) {
		elseLambda(s);
		s.End();
	}
}


/*
Array<Something> stuff;
int arraySize = stuff.m_size;
// in proxy this will be 1?
s.BeginArray(arraySize);
// and this doesn't matter?
stuff.resize(arraySize);
for (int i = 0; i < arraySize; i++) {
	stuff[i].Serialize(s);
}

... or
SerializeArray(Something, stuff, [](Serializer &s, Something &item) {
	item.Serialize(s);
});

...ideally:
SerializeStruct(vector, stuff);
*/
template <typename T, typename Serializer>
void array(Serializer &s, T &field, const char *typeName, const char *name, std::function<void(Serializer &, T &)> itemLambda) {
	s.BeginArray("Array", "Array");

	s.End();
}

};
};
