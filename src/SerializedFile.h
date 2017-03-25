#include "default.h"
#include "Serialize.h"

namespace unitypack {

struct SerializedFile {
	struct Header {
		int metadataSize;
		int fileSize;
		int version;
		int objectDataOffset;
		bool bigEndian;

		template <typename Serializer>
		void Serialize(Serializer &s) {
			using serialize::Flags;
			SerializeScalarV(int, metadataSize, Flags::BigEndian);
			SerializeScalarV(int, fileSize, Flags::BigEndian);
			SerializeScalarV(int, version, Flags::BigEndian | Flags::Variable);
			SerializeScalarV(int, objectDataOffset, Flags::BigEndian);
			SerializeScalarV(bool, bigEndian, Flags::PostAlign | Flags::BigEndianWhenTrue);
		}
	};

	struct Metadata {
		std::string generatorVersion;
		int platform;
		bool serializeTypeTrees;
		std::vector<TypeMetadata> types;
		int _old_0
		std::vector<ObjectInfo> objects;
		std::vector<PPtr> adds;
		std::vector<FileReference> externalFiles;

		template <typename Serializer>
		void Serialize(Serializer &s) {
			using serialize::Flags;
			SerializeStructV(string, generatorVersion, Flags::CString);
			SerializeScalar(int, platform);
			SerializeIf(version, version >= 13, [](Serializer &s){
				SerializeScalarV(bool, serializeTypeTrees, Flags::Variable);
			}, [](){
				SerializeSetVariable(serializeTypeTrees, true);
			});
			SerializeStruct(vector, types);
			SerializeIf(version, 7 <= version && version <= 13, [](Serializer &s){
				SerializeScalar(int, _old_0);
			}, nullptr);
		}
	};

	struct TypeMetadata {

	};

	struct ObjectInfo {};

	struct FileReference {};

	Header header;
	Metadata metadata;

	template <typename Serializer>
	void Serialize(Serializer &s) {
		SerializeStruct(SerializedFile::Header, header);
		SerializeStruct(SerializedFile::Metadata, metadata);
	}
};

};
