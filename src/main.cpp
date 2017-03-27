#include <stdio.h>
#include "SerializedFile.h"

int main() {
	FILE *stream = fopen("globalgamemanagers", "rb");
	unitypack::SerializedFile f{};
	unitypack::serialize::BinaryReader rd{};
	rd.stream = stream;
	f.Serialize(rd);
	printf("num types: %d\n", f.metadata.types.size());
	return 0;
}
