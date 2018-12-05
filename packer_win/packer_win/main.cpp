#include <asmjit.h>
#include <stdio.h>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <string>
#include <sstream>
#include <algorithm>
#include <functional>
#include <unordered_set>
#include <time.h>
#include "pepacker.h"
#include "polyengine.h"
#include "getopt.h"
using namespace asmjit;
using namespace asmjit::host;

#pragma comment(linker, "/SECTION:.text,RWE")

extern "C" DWORD XTEAKey[4];
extern "C" ULONG_PTR loader_start;
extern "C" ULONG_PTR loader_end;

void __stdcall decipher(uint32_t *v, uint32_t const *key)
{
	unsigned int i;

	uint32_t v0 = v[0], v1 = v[1], delta = 0x9E3779B9, sum = delta*64;
	for (i = 0; i < 64; i++) {
		v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum >> 11) & 3]);
		sum -= delta;
		v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
	}
	v[0] = v0; v[1] = v1;
}

void XTEADecrypt(uint32_t *buf, uint32_t bufSize, uint32_t const key[4])
{
	uint32_t *pbuf = buf;
	for (; bufSize > 0; bufSize -= 8, pbuf += 2) {
		decipher(pbuf, key);
	}
}


#define MODE_PACK 0
#define MODE_ENCRYPT 1

#define PARAM_ITMODE 1
#define PARAM_IMPORTS 2

enum EITMode {
	eITM_Original,
	eITM_Random,
	eITM_List
};

int main(int argc, char *argv[])
{
	srand((unsigned int)time(NULL));
	/*
	EITMode itMode = eITM_Original;
	int mode = -1;

	static const struct option_a opts[] = {
		{ "pack", no_argument, &mode, MODE_PACK },
		{ "encrypt", no_argument, &mode, MODE_ENCRYPT },
		{ "itmode", required_argument, NULL, PARAM_ITMODE },
		{ "imports", required_argument, NULL, PARAM_IMPORTS },
		{ 0, 0, 0, 0 }
	};

	char itmode[32] = "original";
	WCHAR imports[MAX_PATH] = { 0 };
	while (TRUE) {
		int optIdx = 0;
		int res = getopt_long_a(argc, argv, "", opts, &optIdx);
		if (res == -1 || res == '?')
			break;

		switch (res) {
		case 0:
			break;

		case PARAM_ITMODE:
			strcpy_s(itmode, sizeof(itmode), optarg_a);
			break;

		case PARAM_IMPORTS:
			MultiByteToWideChar(CP_UTF8, 0, optarg_a, -1, imports, MAX_PATH);
			break;
		}
	}

	if (strcmp(itmode, "original") == 0)
		itMode = eITM_Original;
	else if (strcmp(itmode, "random") == 0)
		itMode = eITM_Random;
	else if (strcmp(itmode, "list") == 0)
		itMode = eITM_List;
		*/
	PEPacker packer("test.exe");
	packer.loadImportFile("api_32.txt");
	packer.setImportMode(eIM_Random);
	packer.process();
	/*PolyEngine p;
	uint8_t *buf = (uint8_t *)malloc(4 * 20);
	size_t bufsize = 4 * 20;
	p.AssembleX86Loader(0x00401000, buf, bufsize, 0x00405456);*/

	return 0;
}
