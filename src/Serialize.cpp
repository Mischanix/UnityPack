#include "Serialize.h"
#include <string.h>

namespace unitypack {
namespace serialize {

template <> const char *type_string<int>() { return "int"; }

void SerializerBase::Begin(const char *typeName, const char *name, int flags) {
	stack.push_back(SerializerBase::Node{ typeName, name, nullptr, (flags & Flags::Array) ? -1 : 0, flags });
}

void SerializerBase::End() {
	auto &node = stack[stack.size() - 1];
	if (node.flags & Flags::Variable) {
		int value = 0;
		switch (node.size) {
		case 4: value = *(int32_t *)node.data; break;
		case 2: value = *(int16_t *)node.data; break;
		case 1: value = *(int8_t *)node.data; break;
		}
		SetVariable(node.name, value);
	}
	if (node.flags & Flags::BigEndianWhenTrue) {
		bool value = false;
		if (node.size >= 1 && node.size <= 8) {
			for (int i = 0; i < node.size; i++) {
				if (((uint8_t *)node.data)[i] != 0) {
					value = true;
				}
			}
		}
		bigEndian = value;
	}
	if (node.flags & Flags::ConditionalIf) {
		conditionWasTrue = true;
	}
	if (node.flags & Flags::ConditionalElse) {
		conditionWasTrue = false;
	}
	stack.pop_back();
}

bool SerializerBase::BeginIf(const char *varName, const char *condStr, std::function<bool(int)> condition) {
	conditionWasTrue = condition(variables[varName]);
	if (conditionWasTrue) {
		Begin("if", condStr, Flags::ConditionalIf);
		return true;
	}
	return false;
}

bool SerializerBase::BeginElse() {
	if (conditionWasTrue) {
		return false;
	}
	Begin("else", "", Flags::ConditionalElse);
	return true;
}

void SerializerBase::RecordScalar(void *data, int size) {
	int stackSize = (int)stack.size();
	for (int i = stackSize - 1; i >= 0; i--) {
		auto &node = stack[i];
		if (node.size < 0) break;
		if (i == stackSize - 1) {
			node.data = data;
		}
		node.size += size;
	}
}

bool SerializerBase::IsBigEndian() const {
	if (bigEndian) {
		return true;
	}
	if (stack.size() > 0) {
		auto &node = stack[stack.size() - 1];
		if (node.flags & Flags::BigEndian) return true;
	}
	return false;
}

};
};
