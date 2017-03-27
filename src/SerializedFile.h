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

struct TypeTree {
	SerializeFn(TypeTree) {
		assert(0);
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
