#include "pepacker.h"
#include "polyengine.h"
#include "utils.h"
#include <fstream>
#include <functional>
#include <numeric>
#include <asmjit.h>

extern "C" ULONG_PTR loader_start;
extern "C" ULONG_PTR loader_code_end;
extern "C" ULONG_PTR loader_end;
extern "C" uint32_t XTEAKey[4];
extern "C" DWORD OriginalITRVA;
extern "C" DWORD OriginalITSize;
extern "C" DWORD OriginalRelocRVA;
extern "C" DWORD OriginalRelocSize;
extern "C" DWORD ITSectionRVA;
extern "C" DWORD ITSectionSize;


//RVA are always DWORD sized (i.e. unsigned 32bit integers)
//VA are either 32bit o 64bit bits (depends on the architecture)
#ifdef _WIN64
extern "C" ULONGLONG DecryptTableVA;
extern "C" ULONGLONG ImageBase;
extern "C" ULONGLONG OldImageBase;
extern "C" ULONGLONG OriginalEPVA;
extern "C" ULONGLONG VirtualProtectVA;
extern "C" ULONGLONG LoadLibraryAVA;
#else
extern "C" DWORD DecryptTableVA;
extern "C" DWORD ImageBase;
extern "C" DWORD OldImageBase;
extern "C" DWORD OriginalEPVA;
extern "C" DWORD VirtualProtectVA;
extern "C" DWORD LoadLibraryAVA;
#endif

#pragma pack(push)
#pragma pack(1)
struct DecryptTableEntry
{
  DWORD Rva;
  DWORD Size;
  BYTE ChangeProtection;
};
#pragma pack(pop)

using namespace Utils;
PEPacker::PEPacker(const std::string& filename, EPackerOptions options)
: m_pefile(filename),
m_importMode(eIM_Original)
{

}

bool PEPacker::loadImportFile(const std::string& filename)
{
  std::ifstream infile;
  infile.open(filename);
  if (!infile.is_open()) {
    fprintf(stderr, "PEPacker::loadImports - could not open import file %s\n", filename.c_str());
    return false;
  }
  std::string line;
  while (std::getline(infile, line)) {
    line.erase(std::remove_if(line.begin(), line.end(), [](char c) { return c == '\r' || c == '\n'; }));
    auto it = line.find_first_of('#');
    if (it != std::string::npos) {
      line.erase(line.begin() + it, line.end());
    }
    //remove trailing and leading spaces
    line = trim(line, " \t");
    if (line.empty())
      continue;
    //split incoming line and build import array
    std::string::size_type idx = line.find_first_of('=');
    if (idx == std::string::npos)
      continue;

    const std::string libName = line.substr(0, idx);
    std::vector<std::string> imports = split(line.substr(idx + 1), ',');
    if (imports.size() > 0) {
      std::transform(imports.begin(), imports.end(), imports.begin(), std::bind(trim, std::placeholders::_1, " \t"));
      m_imports.emplace(libName, imports);
    }
  }
  infile.close();

  return true;
}

//XTEA inner loop
static void encipher(unsigned int num_rounds, uint32_t v[2], uint32_t const key[4])
{
  uint32_t v0 = v[0];
  uint32_t v1 = v[1];
  uint32_t sum = 0;
  const uint32_t delta = 0x9E3779B9;
  for (unsigned int i = 0; i < num_rounds; ++i) {
    v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
    sum += delta;
    v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum >> 11) & 3]);
  }
  v[0] = v0;
  v[1] = v1;
}

bool PEPacker::xteaEncrypt(uint8_t *data, size_t len, uint32_t const key[4])
{
  if (len % (sizeof(uint32_t)*2) != 0) {
    fprintf(stderr, "PEPacker::xteaEncrypt - len should be a multiple of 8\n");
    return false;
  }
  uint32_t *pBuf = reinterpret_cast<uint32_t*>(data);
  for (; len > 0; len -= 8, pBuf += 2) {
    encipher(64, pBuf, key);
  }

  return true;
}
bool PEPacker::process()
{
  m_sectionKey[0] = rand_dword();
  m_sectionKey[1] = rand_dword();
  m_sectionKey[2] = rand_dword();
  m_sectionKey[3] = rand_dword();

  printf("PEPacker::process - mapping input file into memory...\n");
  if (!m_pefile.map()) {
    fprintf(stderr, "PEPacker::process - could not map PE file.\n");
    return false;
  }
  printf("PEPacker::process - input file successfully mapped, address = %p\n", m_pefile.filebase());
  
  if (m_importMode != eIM_Original) {
    printf("PEPacker::process - building a fake IT\n");
    printf("PEPacker::process - original it: RVA = %08x, size = %04x\n", m_pefile.directoryRva(IMAGE_DIRECTORY_ENTRY_IMPORT), m_pefile.directorySize(IMAGE_DIRECTORY_ENTRY_IMPORT));
    std::vector<std::string> dlls;
    std::transform(m_imports.begin(), m_imports.end(), std::back_inserter(dlls), std::bind(&ImportMap::value_type::first, std::placeholders::_1));

    std::random_shuffle(dlls.begin(), dlls.end());

    if (!buildImportTable(dlls)) {
      fprintf(stderr, "PEPacker::process - error while building fake import table.\n");
      return false;
    }
    printf("PEPacker::process - fake import table built: RVA = %08x, size = %04x\n", m_pefile.directoryRva(IMAGE_DIRECTORY_ENTRY_IMPORT), m_pefile.directorySize(IMAGE_DIRECTORY_ENTRY_IMPORT));
  }


  printf("PEPacker::process - processing .text section...\n");
  PIMAGE_SECTION_HEADER pTextSection = m_pefile.sectionByName(".text");
  //pTextSection->Characteristics |= IMAGE_SCN_MEM_WRITE; //set .text section writable, AVs seems to not care about this...
  uint8_t *pDataPtr = make_ptr<uint8_t*>(m_pefile.filebase(), pTextSection->PointerToRawData);
  uint8_t *pTextStart = pDataPtr;
  size_t len = pTextSection->SizeOfRawData != 0 ? pTextSection->SizeOfRawData : pTextSection->Misc.VirtualSize;
  while (len) {
    if (strcmp((const char *)pDataPtr, "__LOADER") == 0)
      break;
    ++pDataPtr;
    --len;
  }

  if (len == 0) {
    fprintf(stderr, "PEPacker::process - could not find __LOADER marker in the .text section.\n");
    return false;
  }

  size_t encryptLen = pDataPtr - pTextStart;
  size_t loaderSpace = 4096 * 4;
  if (encryptLen > 0) {
    //align for XTEA
    while (encryptLen % 8 != 0) {
      ++pDataPtr;
      --loaderSpace;
      encryptLen = pDataPtr - pTextStart;
    }
    fprintf(stderr, "PEPacker::process - __LOADER marker not at the beginning, splitting encrypted zones in two parts...\n");
    
    if (!encryptData(pTextStart, encryptLen, m_sectionKey)) {
      fprintf(stderr, "PEPacker::process - could not encrypt first part of .text section, from %08x to %08x\n", (DWORD)pTextStart, (DWORD)(pTextStart + encryptLen));
      return false;
    }
    m_decryptEntries.emplace(m_pefile.offsetToRva(pTextStart - m_pefile.filebase()), std::make_pair((DWORD)encryptLen, TRUE));
    
    printf("PEPacker::process - frist part of .text section encrypted, from %08x to %08x.\n", (DWORD)pTextStart, (DWORD)(pTextStart + encryptLen));
  }
  
  uint8_t *pRealTextSection = make_ptr<uint8_t*>(pDataPtr, loaderSpace);
  size_t textSectionSize = pTextSection->SizeOfRawData - loaderSpace - encryptLen;
  while (textSectionSize % 8 != 0) {
    ++textSectionSize;
    --pRealTextSection; //we have enough empty space
  }
  
  if (!encryptData(pRealTextSection, textSectionSize, m_sectionKey)) {
    fprintf(stderr, "PEPacker::process - could not encrypt .text section.\n");
    return false;
  }
  m_decryptEntries.emplace(m_pefile.offsetToRva(pRealTextSection - m_pefile.filebase()), std::make_pair((DWORD)textSectionSize, TRUE));

  memcpy(XTEAKey, m_sectionKey, sizeof(m_sectionKey));

  const size_t loadersize = (size_t)((uint8_t *)&loader_end - (uint8_t *)&loader_start);
  const size_t loaderCodeSize = (size_t)((uint8_t *)&loader_code_end - (uint8_t *)&loader_start);

  const ULONG_PTR dwImageBase = m_pefile.ntHeaders()->OptionalHeader.ImageBase;

  ImageBase = m_pefile.ntHeaders()->OptionalHeader.ImageBase;
  OldImageBase = ImageBase;
  OriginalITRVA = m_packerData.OriginalITRva;
  OriginalITSize = m_packerData.OriginalITSize;
  OriginalRelocRVA = m_pefile.directoryRva(IMAGE_DIRECTORY_ENTRY_BASERELOC);
  OriginalRelocSize = m_pefile.directorySize(IMAGE_DIRECTORY_ENTRY_BASERELOC);
  
  const PIMAGE_SECTION_HEADER pITSection = m_pefile.enclosingSectionHeader(m_packerData.OriginalITRva);
  ITSectionRVA = pITSection->VirtualAddress;
  ITSectionSize = pITSection->Misc.VirtualSize;
  /*
  PIMAGE_SECTION_HEADER pDataSection = m_pefile.sectionByName(".data");
  if (!encryptSection(pDataSection, m_sectionKey)) {
    fprintf(stderr, "PEPacker::process - could not encrypt .data section.\n");
    return false;
  }
  m_decryptEntries.emplace(pDataSection->VirtualAddress, std::make_pair(pDataSection->SizeOfRawData, TRUE));
  */
  std::vector<uint8_t> loaderBuf(loadersize + (m_decryptEntries.size() + 1)*sizeof(DecryptTableEntry));

  DecryptTableEntry *pEntries = make_ptr<DecryptTableEntry*>(loaderBuf.data(), loadersize);
  for (auto it = m_decryptEntries.begin(); it != m_decryptEntries.end(); ++it) {
    pEntries->Rva = it->first;
    pEntries->Size = (it->second).first;
    pEntries->ChangeProtection = (it->second).second;
    ++pEntries;
  }
  pEntries->Rva = 0;
  pEntries->Size = 0;

  size_t availableSpace = (size_t)(pRealTextSection - pDataPtr) - loaderBuf.size();
  uint8_t *loaderAddress = make_ptr<uint8_t*>(pDataPtr, (size_t)(rand() % availableSpace));
  const uint32_t loaderRVA = m_pefile.offsetToRva((DWORD)(loaderAddress - m_pefile.filebase()));

  DecryptTableVA = m_pefile.offsetToRva(loaderAddress + loadersize - m_pefile.filebase()) + dwImageBase;
  OriginalEPVA = m_pefile.ntHeaders()->OptionalHeader.AddressOfEntryPoint + dwImageBase;
  VirtualProtectVA = m_packerData.VirtualProtectRVA + dwImageBase;
  LoadLibraryAVA = m_packerData.LoadLibraryARVA + dwImageBase;
  addCodeRelocation(make_offset(&ImageBase, &loader_start), loaderAddress);
  addCodeRelocation(make_offset(&DecryptTableVA, &loader_start), loaderAddress);
  addCodeRelocation(make_offset(&OriginalEPVA, &loader_start), loaderAddress);
  addCodeRelocation(make_offset(&VirtualProtectVA, &loader_start), loaderAddress);
  addCodeRelocation(make_offset(&LoadLibraryAVA, &loader_start), loaderAddress);

  memcpy(loaderBuf.data(), &loader_start, loadersize);

  //assemble first stage polymorphic loader
  PolyEngine pengine;
  std::vector<ULONG_PTR> needReloc;
  pengine.setImageBase(m_pefile.ntHeaders()->OptionalHeader.ImageBase);
  pengine.assembleX86Loader(loaderRVA, loaderBuf.data(), loaderCodeSize, m_packerData.VirtualProtectRVA + dwImageBase, needReloc);
  memcpy(loaderAddress, loaderBuf.data(), loaderBuf.size());
  
  availableSpace = (size_t)(loaderAddress - pDataPtr);
  fillWithRandom(pDataPtr, availableSpace);

  uint8_t *firstStageAddress = nullptr;
  availableSpace = (size_t)(loaderAddress - pDataPtr);
  if (availableSpace >= pengine.getCodeSize())
    firstStageAddress = pDataPtr;
  else
    firstStageAddress = make_ptr<uint8_t*>(loaderAddress, loaderBuf.size());
  
  memcpy(firstStageAddress, pengine.getCode(), pengine.getCodeSize());
  /*for (auto i = needReloc.begin(), e = needReloc.end(); i != e; ++i) {
    addCodeRelocation(*i, firstStageAddress);
  }*/
  /*
  if (!buildRelocSection()) {
    fprintf(stderr, "PEPacker::process - could not build .reloc section.\n");
    return false;
  }*/
  m_pefile.ntHeaders()->OptionalHeader.AddressOfEntryPoint = m_pefile.offsetToRva((DWORD)(pDataPtr - m_pefile.filebase()));
  //m_pefile.ntHeaders()->OptionalHeader.AddressOfEntryPoint = m_pefile.offsetToRva((DWORD)(loaderAddress - m_pefile.filebase()));

  return true;
}

void PEPacker::fillWithRandom(uint8_t *buf, size_t len)
{
  /*
  uint8_t *ptr = buf;
  while (len--) {
    *ptr++ = (uint8_t)(rand() % 0x256);
  }*/
}

bool PEPacker::encryptData(uint8_t *pData, size_t dataSize, uint32_t const key[4])
{
  printf("PEPacker::encryptData - encrypting region at %p of %u bytes...\n", pData, dataSize);
  if (!xteaEncrypt(pData, dataSize, key)) {
    fprintf(stderr, "PEPacker::encryptData - could not encrypt region at %p.\n");
    return false;
  }
  printf("PEPacker::encryptData - region at %p encrypted.\n", pData);
  return true;
}

bool PEPacker::encryptSection(PIMAGE_SECTION_HEADER pSection, uint32_t const key[4])
{
  printf("PEPacker::encryptSection - encrypting %s section...\n", pSection->Name);
  if (!xteaEncrypt(make_ptr<uint8_t*>(m_pefile.filebase(), pSection->PointerToRawData), pSection->SizeOfRawData, key)) {
    fprintf(stderr, "PEPacker::encryptSection - could not encrypt %s section.\n", pSection->Name);
    return false;
  }
  printf("PEPacker::encryptSection - section %s encrypted.\n", pSection->Name);
  return true;
}

bool PEPacker::buildImportTable(const std::vector<std::string>& modules)
{
  printf("PEPacker::buildImportTable - looking for __FAKEIT marker...\n");
  //import table should be in the .rdata section, avoid looking over the entire file
  PIMAGE_SECTION_HEADER pSection = m_pefile.sectionByName(".rdata");
  if (pSection == nullptr) {
    fprintf(stderr, "PEPacker::buildImportTable - missing .rdata section, please check your build settings.\n");
    return false;
  }

  uint8_t *pDataPtr = make_ptr<uint8_t *>(m_pefile.filebase(), pSection->PointerToRawData);
  uint8_t *pEncryptStart = pDataPtr;
  uint32_t datasize = pSection->SizeOfRawData;
  while (datasize--) {
    if (strcmp((const char *)pDataPtr, "__FAKEIT") == 0)
      break;
    ++pDataPtr;
  }

  if (datasize == 0) {
    fprintf(stderr, "PEPacker::buildImportTable - could not find __FAKEIT marker.\n");
    return false;
  }
  //points to the end of the fake it section
  uint8_t *pFakeITEnd = make_ptr<uint8_t*>(pDataPtr, 4096 * 2);

  //store the load config descriptor for later use
  const PIMAGE_LOAD_CONFIG_DIRECTORY pLoadConfig = make_ptr<PIMAGE_LOAD_CONFIG_DIRECTORY>(m_pefile.filebase(), m_pefile.rvaToOffset(m_pefile.directoryRva(IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG)));
  size_t origLoadConfigSize = pLoadConfig->Size > m_pefile.directorySize(IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG) ? pLoadConfig->Size : m_pefile.directorySize(IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG);
  uint8_t *pOrigLoadConfig = new uint8_t[origLoadConfigSize];
  memcpy(pOrigLoadConfig, pLoadConfig, origLoadConfigSize);

  //encrypt the first part of the .radata section
  uint32_t encryptLen = (uint32_t)(pDataPtr - pEncryptStart);
  while (encryptLen % 8 != 0) {
    *pDataPtr++ = rand() % 255; //random padding instead of 0 padding
    encryptLen = (uint32_t)(pDataPtr - pEncryptStart);
  }

  /*
  if (!encryptData(pEncryptStart, encryptLen, m_sectionKey)) {
    fprintf(stderr, "PEPacker::buildImportTable - could not encrypt first part of .rdata section.\n");
    return false;
  }
  m_decryptEntries.emplace(m_pefile.offsetToRva(pEncryptStart - m_pefile.filebase()), std::make_pair(encryptLen, TRUE));
  */

  //now encrypt the second part of the .rdata section
  uint8_t *pSectionEnd = make_ptr<uint8_t *>(m_pefile.filebase(), pSection->PointerToRawData + pSection->SizeOfRawData);
  encryptLen = (uint32_t)(pSectionEnd - pFakeITEnd);
  while (encryptLen % 8 != 0) {
    pFakeITEnd--;
    *pFakeITEnd = rand() % 255;
    encryptLen = (uint32_t)(pSectionEnd - pFakeITEnd);
  }

  /*
  if (!encryptData(pFakeITEnd, encryptLen, m_sectionKey)) {
    fprintf(stderr, "PEPacker::buildImportTable - could not encrypt second part of .rdata section.\n");
    return false;
  }
  m_decryptEntries.emplace(m_pefile.offsetToRva(pFakeITEnd - m_pefile.filebase()), std::make_pair(encryptLen, TRUE));
  */

  //now pDataPtr will point to the beginning of the new import table
  printf("PEPacker::buildImportTable - __FAKEIT marker found at offset %d\n", (uint32_t)(pDataPtr - m_pefile.filebase()));

  //now we can build the fake import table, but before that we need to store the old import table rva.
  //section shrinking due to packing will not change the in-memory structure of the file, so theese values
  //are correct
  m_packerData.OriginalITRva = m_pefile.directoryRva(IMAGE_DIRECTORY_ENTRY_IMPORT);
  m_packerData.OriginalITSize = m_pefile.directorySize(IMAGE_DIRECTORY_ENTRY_IMPORT);

  //build an array with the number of functions from each import
  //this way we know how much space to allocate for FTs and OFTs
  std::unordered_map<std::string, std::vector<std::string>> importList;
  size_t thunkCount = 0;
  for (size_t i = 0; i < modules.size(); ++i) {
    const std::vector<std::string>& functList = m_imports.at(modules[i]);
    const size_t maxsize = functList.size() < 25 ? functList.size() : 25;
    std::vector<std::string> imports;
    if (modules[i] == "kernel32")
      imports = functList;
    else
      imports = random_set(functList, m_importMode == eIM_Random ? ((rand() % maxsize) + 1) : functList.size());

    if (modules[i] == "user32") {
      if (std::find(imports.begin(), imports.end(), "CreateWindowExW") == imports.end())
        imports.emplace_back("CreateWindowExW");
      if (std::find(imports.begin(), imports.end(), "ShowWindow") == imports.end())
        imports.emplace_back("ShowWindow");
      if (std::find(imports.begin(), imports.end(), "UpdateWindow") == imports.end())
        imports.emplace_back("UpdateWindow");
      if (std::find(imports.begin(), imports.end(), "GetMessageW") == imports.end())
        imports.emplace_back("GetMessageW");
      if (std::find(imports.begin(), imports.end(), "TranslateMessage") == imports.end())
        imports.emplace_back("TranslateMessage");
      if (std::find(imports.begin(), imports.end(), "DispatchMessageW") == imports.end())
        imports.emplace_back("DispatchMessageW");
    }
    importList.emplace(modules[i], imports);
    thunkCount += imports.size();
  }

  //size of the import table descriptors
  //+1 because we need an empty descriptor at the end of the import table
  const size_t itsize = sizeof(IMAGE_IMPORT_DESCRIPTOR)*(modules.size() + 1);

  //update the RVA and size in the directory entry
  m_pefile.setDirectoryRva(IMAGE_DIRECTORY_ENTRY_IMPORT, m_pefile.pointerToRva(pDataPtr));
  m_pefile.setDirectorySize(IMAGE_DIRECTORY_ENTRY_IMPORT, (uint32_t)itsize);

  //thunkCount is the total number of thunks needed
  //thunkSize is the size (in byte) for a set of thunk entries, plus one empty thunk
  //for each import descriptor, thus the total number of thunks required
  //is thunkCount + modules.size()
  const size_t thunkSize = sizeof(ULONG_PTR)*(thunkCount + modules.size());

  m_pefile.setDirectorySize(IMAGE_DIRECTORY_ENTRY_IAT, thunkSize / sizeof(ULONG_PTR));

  ULONG_PTR *pFts = make_ptr<ULONG_PTR*>(pDataPtr, itsize);
  ULONG_PTR *pOfts = make_ptr<ULONG_PTR*>(pFts, thunkSize);
  PIMAGE_IMPORT_BY_NAME pImports = make_ptr<PIMAGE_IMPORT_BY_NAME>(pOfts, thunkSize);

  m_pefile.setDirectoryRva(IMAGE_DIRECTORY_ENTRY_IAT, m_pefile.pointerToRva(pFts));
  m_pefile.setDirectorySize(IMAGE_DIRECTORY_ENTRY_IAT, thunkSize);

  PIMAGE_IMPORT_DESCRIPTOR pCurrentDescriptor = make_ptr<PIMAGE_IMPORT_DESCRIPTOR>(pDataPtr, 0);
  for (size_t i = 0; i < modules.size(); ++i) {
    const std::vector<std::string>& curImports = importList.at(modules[i]);

    pCurrentDescriptor->ForwarderChain = 0x00000000;
    pCurrentDescriptor->TimeDateStamp = 0x00000000;
    pCurrentDescriptor->OriginalFirstThunk = m_pefile.pointerToRva(pOfts);
    pCurrentDescriptor->FirstThunk = m_pefile.pointerToRva(pFts);

    for (size_t j = 0; j < curImports.size(); ++j) {
      const std::string& importName = curImports.at(j);
      pImports->Hint = 0x0000;
      strcpy_s(pImports->Name, 32, importName.c_str());
      pImports->Name[importName.size()] = '\0';
      const DWORD thunkRVA = m_pefile.pointerToRva(pImports);
      *pOfts = (ULONG_PTR)thunkRVA;
      *pFts = (ULONG_PTR)thunkRVA;
      //advance the IMAGE_IMPORT_BY_NAME pointer to next entry
      pImports = make_ptr<PIMAGE_IMPORT_BY_NAME>(pImports, sizeof(IMAGE_IMPORT_BY_NAME)+importName.size()-1);

      //ugly, store the RVA of the thunk element in the thunk arrays for
      //APIs that will be needed by the loader
      if (modules[i] == "kernel32") {
        DWORD importRVA = m_pefile.offsetToRva((uint8_t *)pFts - m_pefile.filebase());
        if (importName == "VirtualProtect")
          m_packerData.VirtualProtectRVA = importRVA;
        else if (importName == "LoadLibraryA")
          m_packerData.LoadLibraryARVA = importRVA;
      }
      ++pOfts;
      ++pFts;
    }
    //one empty thunk for each array of thunks
    *pOfts++ = 0x00000000;
    *pFts++ = 0x00000000;

    //store dll name
    char *pModuleName = make_ptr<char*>(pImports, 0);
    std::string name = modules[i];
    if (((float)rand() / (float)RAND_MAX) > 0.0f)
      std::transform(name.begin(), name.end(), name.begin(), toupper);
    name += ".dll";

    strcpy_s(pModuleName, 32, name.c_str());
    pModuleName[name.size()] = '\0';
    pCurrentDescriptor->Name = m_pefile.pointerToRva(pModuleName);
    pImports = make_ptr<PIMAGE_IMPORT_BY_NAME>(pImports, name.size()+1);
    ++pCurrentDescriptor;
  }
  //last descriptor is 0
  memset(pCurrentDescriptor, 0, sizeof(IMAGE_IMPORT_DESCRIPTOR));

  printf("PEPacker::process - moving load configuration directory.\n");
  //move the configuration 
  uint8_t *pDestLoadConfig = make_ptr<uint8_t*>(pImports, 1);
  while (((ULONG_PTR)pDestLoadConfig & 0xF) != 0)
    ++pDestLoadConfig;

  memcpy(pDestLoadConfig, pOrigLoadConfig, origLoadConfigSize);
  delete[] pOrigLoadConfig;

  //m_pefile.setDirectoryRva(IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG, m_pefile.offsetToRva((uint8_t *)pDestLoadConfig - m_pefile.filebase()));

  return true;
}

void PEPacker::addCodeRelocation(off_t offset, void *baseAddress)
{
  const DWORD entryRVA = m_pefile.offsetToRva((uint8_t *)baseAddress + offset - m_pefile.filebase());
  const DWORD relocBase = entryRVA & 0xFFFFF000;
  if (m_relocEntries.find(relocBase) != m_relocEntries.end())
    m_relocEntries.at(relocBase).push_back(entryRVA);
  else
    m_relocEntries.emplace(relocBase, std::vector<DWORD>({ entryRVA }));
}

bool PEPacker::buildRelocSection()
{
  printf("PEPacker::buildRelocSection - building a new .reloc section for loader data and code...\n");
  const DWORD relocRVA = m_pefile.directoryRva(IMAGE_DIRECTORY_ENTRY_BASERELOC);
  const size_t relocSize = m_pefile.directorySize(IMAGE_DIRECTORY_ENTRY_BASERELOC);

#ifdef _WIN64
  const uint32_t relType = IMAGE_REL_BASED_DIR64;
#else
  const uint32_t relType = IMAGE_REL_BASED_HIGHLOW;
#endif

  printf("PEPacker::buildRelocSection - old RVA: %08x, old size: %u\n", relocRVA, relocSize);
  PIMAGE_BASE_RELOCATION pReloc = make_ptr<PIMAGE_BASE_RELOCATION>(m_pefile.filebase(), m_pefile.rvaToOffset(relocRVA));
  pReloc = make_ptr<PIMAGE_BASE_RELOCATION>(pReloc, relocSize);
  const PIMAGE_BASE_RELOCATION pFirstReloc = pReloc;
  
  size_t newRelocSize = 0;
  for (auto it = m_relocEntries.begin(), et = m_relocEntries.end(); it != et; ++it) {
    const DWORD relocBase = it->first;
    const std::vector<DWORD>& entries = it->second;
    pReloc->VirtualAddress = relocBase;
    pReloc->SizeOfBlock = sizeof(WORD)*entries.size() + sizeof(IMAGE_BASE_RELOCATION);
    PWORD pRelocEntry = make_ptr<PWORD>(pReloc, sizeof(IMAGE_BASE_RELOCATION));
    for (auto i = entries.begin(), e = entries.end(); i != e; ++i) {
      *pRelocEntry++ = (relType << 12) | (WORD)((*i - pReloc->VirtualAddress) & 0xFFF);
    }
    newRelocSize += pReloc->SizeOfBlock;
    pReloc = make_ptr<PIMAGE_BASE_RELOCATION>(pReloc, sizeof(IMAGE_BASE_RELOCATION)+sizeof(WORD)*entries.size());
  }

  m_pefile.setDirectoryRva(IMAGE_DIRECTORY_ENTRY_BASERELOC, m_pefile.offsetToRva((uint8_t *)pFirstReloc - m_pefile.filebase()));
  m_pefile.setDirectorySize(IMAGE_DIRECTORY_ENTRY_BASERELOC, newRelocSize);
  printf("PEPacker::buildRelocSection - new .reloc section built, new RVA: %08x, new size: %u.\n", m_pefile.directoryRva(IMAGE_DIRECTORY_ENTRY_BASERELOC), m_pefile.directorySize(IMAGE_DIRECTORY_ENTRY_BASERELOC));

  return true;
}