#ifndef PEPACKER_H
#define PEPACKER_H
#include "pefile.h"
#include <unordered_map>
#include <map>
#include <vector>
#include <asmjit.h>

enum EPackerOptions
{
	ePO_Encrypt,
	ePO_Pack,
};

enum EImportMode
{
	eIM_Original,
	eIM_List,
	eIM_Random
};

class PEPacker
{
private:
	struct PackerData {
		uint32_t OrignalEPRVA;
		uint32_t VirtualProtectRVA;
		uint32_t GetProcAddressRVA;
		uint32_t LoadLibraryARVA;
		uint32_t OriginalITRva;
		uint32_t OriginalITSize;
		uint32_t OriginalRelocRVA;
		uint32_t OriginalRelocSize;
	};

public:
	PEPacker(const std::string& filename, EPackerOptions options = ePO_Encrypt);

	bool loadImportFile(const std::string& filename);
	void setImportMode(EImportMode importMode = eIM_Original) { m_importMode = importMode; }

	bool process();

	PEFile& pe() { return m_pefile; }
	const PEFile& pe() const { return m_pefile; }

private:
	bool encryptOriginalIT(uint32_t const key[4]);
	bool buildImportTable(const std::vector<std::string>& modules);
	bool buildRelocSection();
	bool xteaEncrypt(uint8_t *data, size_t len, uint32_t const key[4]);
	bool encryptSection(PIMAGE_SECTION_HEADER pSection, uint32_t const key[4]);
	bool encryptData(uint8_t *pData, size_t dataSize, uint32_t const key[4]);
	void addCodeRelocation(off_t offset, void *baseAddress);
	void fillWithRandom(uint8_t *buf, size_t len);

private:
	typedef std::unordered_map<std::string, std::vector<std::string>> ImportMap;
	PEFile m_pefile;
	bool m_pack;
	bool m_encrypt;
	ImportMap m_imports;
	EImportMode m_importMode;
	PackerData m_packerData;
	uint32_t m_sectionKey[4];
	std::unordered_map<DWORD, std::pair<DWORD,BOOL>> m_decryptEntries;
	std::map<DWORD, std::vector<DWORD>> m_relocEntries;
};

#endif