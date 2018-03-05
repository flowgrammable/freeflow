
#include <stdlib.h>
#include <stdio.h>
#include <string.h> // for strcat
#include <dlfcn.h>  // for Dynamic Loader API
#include <unistd.h> // for getcwd
#include "util.h"
#include "test_packets.h"
#include "key.h" // remove eventually, now just for testing...

// Extract Library 1
const char LIB_EXTRACT_1[] = "extract-2.1.so";

int main(int argc, char* argv[]) {
	// Get current working directory
	char cwd[1024];
	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		printf("Error: Unable to determine current working directory\n");
	}
	strcat(cwd, "/");
	strcat(cwd, LIB_EXTRACT_1);	// append extract so lib filename (hardcoded)

	// Prepare test packet:
	u8 buf[sizeof(testTcp)/2];
	int i;
	const char* pos = testTcp;
	for (i = 0; i < sizeof(buf)/sizeof(buf[0]); i++) {
		sscanf(pos, "%2hhx", &(buf[i]));
		pos += 2 * sizeof(char);
	}
	printf("Prepared test packet\n");

	// Dynamically load Field Extractor stage:
	void* dl_fieldExtract;
	dl_fieldExtract = dlopen(cwd, RTLD_NOW);
	if (!dl_fieldExtract) {
		printf("Error: Dynamically Loading Library\n%s\n", dlerror());
		return EXIT_FAILURE;
	}
	printf("Dynamically Loaded %s Library\n", LIB_EXTRACT_1);

	// Extract function definition:
	void (*stageFieldExtract)(View*, Key*, void*, u16);
	stageFieldExtract = dlsym(dl_fieldExtract, "extract");
	char* error = dlerror();
	if (error != NULL) {
		printf("Error: Resolving symbol: extract\n%s\n", error);
		return EXIT_FAILURE;
	}
	printf("Resolved symbol: extract\n");

	// Dynamically load Field Printer stage:


	// Perform actual Extraction on test packet:
	Key k;	// obviously hardcoded here... (just here for alloc and printing)
	View v;
	printf("Before calling extract\n");
	stageFieldExtract(&v, &k, buf, sizeof(buf));
	if (v.end == NULL) {
		printf("Error: failed to properly extract test packet\n");
	}
	printf("ethType: %x\n"\
			   "ethSrc: %llx\n"\
				 "ethDst: %llx\n"\
				 "ipv4Proto: %x\n"\
				 "ipv4Src: %lx\n"\
				 "ipv4Dst: %lx\n"\
				 "tcpSrcPort: %i\n"\
				 "tcpDstPort: %i\n",
				 k.ethType,
				 k.ethSrc,
				 k.ethDst,
				 k.ipv4Proto,
				 k.ipv4Src,
				 k.ipv4Dst,
				 k.tcpSrcPort,
				 k.tcpDstPort
	);
	printf("After calling extract\n");
  return EXIT_SUCCESS;
}

