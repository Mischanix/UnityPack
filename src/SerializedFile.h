#pragma once

#include "default.h"
#include "Serialize.h"

namespace unitypack {

struct Hash {
	uint32_t hash[4];

	SerializeFn(Hash) {
		SerializeScalar(uint32_t, hash[0]);
		SerializeScalar(uint32_t, hash[1]);
		SerializeScalar(uint32_t, hash[2]);
		SerializeScalar(uint32_t, hash[3]);
	}
};

struct TypeTreeNode {
	uint16_t version;
	uint8_t depth;
	bool isArray;
	uint32_t type;
	uint32_t name;
	int byteSize;
	int index;
	uint32_t metaFlag;

	SerializeFn(TypeTreeNode) {
		SerializeScalar(uint16_t, version);
		SerializeScalar(uint8_t, depth);
		SerializeScalar(bool, isArray);
		SerializeScalar(uint32_t, type);
		SerializeScalar(uint32_t, name);
		SerializeScalar(int, byteSize);
		SerializeScalar(int, index);
		SerializeScalar(uint32_t, metaFlag);
	}
};

struct TypeTree {
	std::vector<TypeTreeNode> nodes;
	std::vector<char> buffer;

	const char *GetString(uint32_t index) const {
		if (index & 0x80000000u) {
			return globalBuffer + (index & 0x7fffffffu);
		} else {
			if (buffer.size() <= index) {
				return "";
			}
			return buffer.data() + index;
		}
	}

	uint32_t GetIndex(const char *str) {
		for (const char *g = globalBuffer; *g;) {
			if (strcmp(str, g) == 0) {
				return 0x80000000u | (uint32_t)(g - globalBuffer);
			}
			g += 1 + strlen(g);
		}
		for (size_t i = 0; i < buffer.size();) {
			if (strcmp(str, buffer.data() + i) == 0) {
				return (uint32_t)i;
			}
			i += 1 + strlen(buffer.data());
		}
		size_t idx = buffer.size();
		size_t sz = 1 + strlen(str);
		buffer.resize(idx + sz);
		for (size_t i = 0; i < sz; i++) {
			buffer[idx + i] = str[i];
		}
		return (uint32_t)idx;
	}

	static const char globalBuffer[];

	SerializeFn(TypeTree) {
		using serialize::Flags;

		// This is ugly because the new format is so different than the old.
		SerializeIf(version, version == 10 || version >= 12, [&](){
			int numNodes = (int)nodes.size();
			SerializeScalar(int, numNodes);
			nodes.resize(numNodes);

			int bufferSize = (int)buffer.size();
			SerializeScalar(int, bufferSize);
			buffer.resize(bufferSize);

			s.Begin("Array", "Array", Flags::Array);
			for (int i = 0; i < numNodes; i++) {
				unitypack::serialize::struct_val(s, nodes[i], "TypeTreeNode", "data");
			}
			s.End();

			s.Begin("Array", "Array", Flags::Array);
			for (int i = 0; i < bufferSize; i++) {
				unitypack::serialize::scalar_val(s, buffer[i], "char", "data");
			}
			s.End();
		}, [&](){
			if (nodes.size() < 1) {
				nodes.resize(1);
			}
			int numNodesKnown = 1;
			int i = 0;
			SerializeRecursiveNode(s, &nodes[i], i, 0, numNodesKnown);
		});
	}

	template <typename Serializer>
	void SerializeRecursiveNode(Serializer &s, TypeTreeNode *node, int &i, int depth, int &numNodesKnown) {
		using serialize::Flags;

		s.Begin("TypeTreeNode", "node", Flags::Recursive);

		std::string type = GetString(node->type);
		std::string name = GetString(node->name);
		SerializeStruct(string, type);
		SerializeStruct(string, name);
		node->type = GetIndex(type.c_str());
		node->name = GetIndex(name.c_str());

		unitypack::serialize::scalar_val(s, node->byteSize, "int", "byteSize");
		unitypack::serialize::scalar_val(s, node->index, "int", "index");
		int isArray = node->isArray;
		unitypack::serialize::scalar_val(s, isArray, "int", "isArray");
		node->isArray = isArray != 0;
		int version = node->version;
		unitypack::serialize::scalar_val(s, version, "int", "version");
		node->version = (uint16_t)version;
		unitypack::serialize::scalar_val(s, node->metaFlag, "int", "metaFlag");
		node->depth = (uint8_t)depth;

		int numChildren = 0;
		for (int j = i + 1; j < (int)nodes.size(); j++) {
			if (nodes[j].depth <= depth) break;
			if (nodes[j].depth == depth + 1) numChildren++;
		}
		s.Begin("Array", "Array", Flags::Array);
		SerializeScalarV(int, numChildren, Flags::TreeNodeChildCount);
		numNodesKnown += numChildren;
		if (numNodesKnown > (int)nodes.size()) {
			nodes.resize(numNodesKnown);
		}
		for (int j = 0; j < numChildren; j++) {
			i++;
			SerializeRecursiveNode(s, &nodes[i], i, depth + 1, numNodesKnown);
		}
		s.End();

		s.End();
	}
};

struct SerializedFile {
	struct Header {
		int metadataSize;
		int fileSize;
		int version;
		int objectDataOffset;
		bool bigEndian;

		SerializeFn(SerializedFile::Header) {
			using serialize::Flags;
			SerializeScalarV(int, metadataSize, Flags::BigEndian);
			SerializeScalarV(int, fileSize, Flags::BigEndian);
			SerializeScalarV(int, version, Flags::BigEndian | Flags::Variable);
			SerializeScalarV(int, objectDataOffset, Flags::BigEndian);
			SerializeScalarV(bool, bigEndian, Flags::PostAlign | Flags::BigEndianWhenTrue);
		}
	};

	struct TypeMetadata {
		int oldClassID;
		int classID;
		uint8_t unk0;
		int16_t scriptID;
		Hash scriptHash;
		Hash typeHash;
		TypeTree tree;

		SerializeFn(SerializedFile::TypeMetadata) {
			using serialize::Flags;
			SerializeIf(version, version >= 17, [&](){
				s.SetVariable("oldClassID", 0);
				SerializeScalarV(int, classID, Flags::Variable);
				SerializeScalar(uint8_t, unk0);
				SerializeScalar(int16_t, scriptID);
			}, [&](){
				SerializeScalarV(int, oldClassID, Flags::Variable);
				s.SetVariable("classID", 0);
			});
			SerializeIf(version, version >= 13, [&](){
				SerializeIf(oldClassID, oldClassID < 0, [&](){
					SerializeStruct(Hash, scriptHash);
				});
				SerializeIf(classID, classID == 114, [&](){
					SerializeStruct(Hash, scriptHash);
				});
				SerializeStruct(Hash, typeHash);
			});
			SerializeIf(serializeTypeTrees, serializeTypeTrees != 0, [&](){
				SerializeStruct(TypeTree, tree);
			});
		}
	};

	struct ObjectPtr {
		int fileID;
		uint64_t pathID;

		SerializeFn(SerializedFile::ObjectPtr) {
			using serialize::Flags;
			SerializeScalar(int, fileID);
			SerializeIf(version, version >= 14, [&](){
				SerializeScalarV(uint64_t, pathID, Flags::PreAlign);
			}, [&](){
				SerializeScalarV(uint32_t, pathID, Flags::ValueIs32Bit);
			});
		}
	};

	struct ObjectInfo {
		uint64_t objectID;
		int dataOffset;
		int dataSize;
		int typeID;
		int16_t classID;
		int typeIndex;
		int16_t scriptID;
		uint8_t unk0;

		SerializeFn(SerializedFile::ObjectInfo) {
			using serialize::Flags;
			SerializeIf(version, version >= 14, [&](){
				SerializeScalarV(uint64_t, objectID, Flags::PreAlign);
			}, [&](){
				SerializeScalarV(uint32_t, objectID, Flags::ValueIs32Bit);
			});
			SerializeScalar(int, dataOffset);
			SerializeScalar(int, dataSize);
			SerializeIf(version, version >= 17, [&](){
				SerializeScalar(int, typeIndex);
			}, [&](){
				SerializeScalar(int, typeID);
				SerializeScalar(int16_t, classID);
			});
			SerializeIf(version, version <= 16, [&](){
				SerializeScalar(int16_t, scriptID);
			});
			SerializeIf(version, 15 <= version && version <= 16, [&](){
				SerializeScalar(uint8_t, unk0);
			});
		}
	};

	struct FileReference {
		std::string assetName;
		Hash guid;
		int type;
		std::string fileName;

		SerializeFn(SerializedFile::FileReference) {
			using serialize::Flags;
			SerializeIf(version, version >= 6, [&](){
				SerializeStructV(string, assetName, Flags::CString);
			});
			SerializeIf(version, version >= 5, [&](){
				SerializeStruct(Hash, guid);
				SerializeScalar(int, type);
			});
			SerializeStructV(string, fileName, Flags::CString);
		}
	};

	struct Metadata {
		std::string generatorVersion;
		int platform;
		bool serializeTypeTrees;
		std::vector<TypeMetadata> types;
		int unk0;
		std::vector<ObjectInfo> objects;
		std::vector<ObjectPtr> adds;
		std::vector<FileReference> externalFiles;
		std::string unk1;

		SerializeFn(SerializedFile::Metadata) {
			using serialize::Flags;
			SerializeStructV(string, generatorVersion, Flags::CString);
			SerializeScalar(int, platform);
			SerializeIf(version, version >= 13, [&](){
				SerializeScalarV(bool, serializeTypeTrees, Flags::Variable);
			}, [&](){
				s.SetVariable("serializeTypeTrees", true);
			});
			SerializeStruct(vector, types);
			SerializeIf(version, 7 <= version && version <= 13, [&](){
				SerializeScalar(int, unk0);
			});
			SerializeStruct(vector, objects);
			SerializeIf(version, version >= 11, [&](){
				SerializeStruct(vector, adds);
			});
			SerializeStruct(vector, externalFiles);
			SerializeIf(version, version >= 5, [&](){
				SerializeStructV(string, unk1, Flags::CString);
			});
		}
	};

	Header header;
	Metadata metadata;

	SerializeFn(SerializedFile) {
		SerializeStruct(SerializedFile::Header, header);
		SerializeStruct(SerializedFile::Metadata, metadata);
	}
};

};
