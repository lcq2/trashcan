#include "pefile.h"
#include "utils.h"
#include <ImageHlp.h>

using namespace Utils;

PEFile::PEFile(const std::string& filename)
  : m_filename(filename),
  m_pFilebase(nullptr),
  m_hFile(INVALID_HANDLE_VALUE),
  m_hMappedFile(INVALID_HANDLE_VALUE)
{

}

bool PEFile::map()
{
  if (m_pFilebase != nullptr)
    return true;

  WCHAR filename[MAX_PATH] = { 0 };
  bool success = false;
  __try {
    int cchCount = MultiByteToWideChar(CP_UTF8, 0, m_filename.c_str(), -1, filename, MAX_PATH);
    if (cchCount == 0) {
      fprintf(stderr, "PEFile::map - could not convert filename %s to UCS-2", m_filename.c_str());
      __leave;
    }
    m_hFile = CreateFileW(filename, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (m_hFile == INVALID_HANDLE_VALUE) {
      fprintf(stderr, "PEFile::map - could not open file %s, CreateFile returned %08x", m_filename.c_str(), GetLastError());
      __leave;
    }
    if (GetFileSizeEx(m_hFile, &m_filesize) == FALSE) {
      fprintf(stderr, "PEFile::map - could not determinte file size, GetFileSizeEx returned %08x", GetLastError());
      __leave;
    }
    m_hMappedFile = CreateFileMappingW(m_hFile, nullptr, PAGE_READWRITE, m_filesize.HighPart, m_filesize.LowPart, nullptr);
    if (m_hMappedFile == INVALID_HANDLE_VALUE) {
      fprintf(stderr, "PEFile::map - could not create file mapping, CreateFileMappingW returned %08x\n", GetLastError());
      __leave;
    }
    m_pFilebase = reinterpret_cast<uint8_t*>(MapViewOfFile(m_hMappedFile, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, m_filesize.LowPart));
    if (m_pFilebase == nullptr) {
      fprintf(stderr, "PEFile::map - could not map file, MapViewOfFile returned %08x\n", GetLastError());
      __leave;
    }
    m_pDOSHeader = make_ptr<PIMAGE_DOS_HEADER>(m_pFilebase, 0);
    if (m_pDOSHeader->e_magic != IMAGE_DOS_SIGNATURE) {
      fprintf(stderr, "PEFile::map - invalid file format, missing DOS signature, found %04x\n", m_pDOSHeader->e_magic);
      __leave;
    }
    //some sanity checks...
    {
      const uint8_t *pBegin = m_pFilebase + sizeof(IMAGE_DOS_HEADER);
      const uint8_t *pEnd = m_pFilebase + m_filesize.LowPart;
      const uint8_t *pValue = m_pFilebase + m_pDOSHeader->e_lfanew;
      if (pValue < pBegin || pValue > pEnd) {
        fprintf(stderr, "PEFile::map - invalid PE file, e_lfanew points to wrong location, current value = 0x%08x\n", m_pDOSHeader->e_lfanew);
        __leave;
      }
    }

    m_pNTHeaders = make_ptr<PIMAGE_NT_HEADERS>(m_pDOSHeader, m_pDOSHeader->e_lfanew);
    if (m_pNTHeaders->Signature != IMAGE_NT_SIGNATURE) {
      fprintf(stderr, "PEFile::map - invalid PE file, missing valid NT signature, found %08x\n", m_pNTHeaders->Signature);
      __leave;
    }
    success = true;
    m_pFirstSection = IMAGE_FIRST_SECTION(m_pNTHeaders);
  }
  __finally {
    if (!success) {
      if (m_pFilebase != nullptr) {
        if (m_hMappedFile != INVALID_HANDLE_VALUE)
          UnmapViewOfFile(m_pFilebase);
        m_pFilebase = nullptr;
      }
      if (m_hMappedFile != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hMappedFile);
        m_hMappedFile = INVALID_HANDLE_VALUE;
      }
      if (m_hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hFile);
        m_hFile = INVALID_HANDLE_VALUE;
      }
    }
  }

  return success;
}

void PEFile::unmap(bool checksum)
{
  if (checksum && !updateChecksum()) {
    fprintf(stderr, "PEFile::unmap - could not update image checksum, updateChecksum failed\n");
  }

  if (m_pFilebase != nullptr) {
    UnmapViewOfFile(m_pFilebase);
    m_pFilebase = nullptr;
  }

  if (m_hMappedFile != INVALID_HANDLE_VALUE) {
    CloseHandle(m_hMappedFile);
    m_hMappedFile = INVALID_HANDLE_VALUE;
  }

  if (m_hFile != INVALID_HANDLE_VALUE) {
    CloseHandle(m_hFile);
    m_hFile = INVALID_HANDLE_VALUE;
  }
}

bool PEFile::updateChecksum()
{
  DWORD oldsum = 0;
  DWORD newsum = 0;
  if (m_pFilebase != nullptr && m_hMappedFile != INVALID_HANDLE_VALUE) {
    //the file is still mapped, so we update the checksum manually
    if (CheckSumMappedFile(m_pFilebase, m_filesize.LowPart, &oldsum, &newsum) == nullptr) {
      fprintf(stderr, "PEFile::updateChecksum - could not checksum mapped file, CheckSumMappedFile returned %08x\n", GetLastError());
      return false;
    }
    printf("PEFile::updateChecksum - old checksum = 0x%08x, new checksum = 0x%08x\n", oldsum, newsum);
    m_pNTHeaders->OptionalHeader.CheckSum = newsum;
  }
  else {
    fprintf(stderr, "PEFIle::updateChecksum  file unmapped, cannot update checksum\n");
    return false;
  }

  return true;
}
uint32_t PEFile::rvaToOffset(uint32_t rva)
{
  uint32_t offset = 0;
  PIMAGE_SECTION_HEADER pSection = m_pFirstSection;
  for (uint16_t i = 0; i < m_pNTHeaders->FileHeader.NumberOfSections; ++i, ++pSection) {
    uint32_t limit = pSection->SizeOfRawData != 0 ? pSection->SizeOfRawData : pSection->Misc.VirtualSize;
    if (rva >= pSection->VirtualAddress && (rva < (pSection->VirtualAddress + limit))) {
      if (pSection->PointerToRawData != 0) {
        offset = (rva - pSection->VirtualAddress) + pSection->PointerToRawData;
        break;
      }
    }
  }
  if (offset == 0) {
    fprintf(stderr, "PEFile::rvaToOffset - could not convert 0x%08x to a valid offset\n", rva);
  }

  return offset;
}

uint32_t PEFile::offsetToRva(uint32_t offset)
{
  uint32_t rva = 0;
  PIMAGE_SECTION_HEADER pSection = m_pFirstSection;
  for (uint16_t i = 0; i < m_pNTHeaders->FileHeader.NumberOfSections; ++i, ++pSection) {
    if ((offset >= pSection->PointerToRawData) && (offset < (pSection->PointerToRawData + pSection->SizeOfRawData))) {
      rva = (offset - pSection->PointerToRawData) + pSection->VirtualAddress;
      break;
    }
  }
  if (rva == 0) {
    fprintf(stderr, "PEFile::offsetToRva - could not convert 0x%08x to a valid rva\n", offset);
  }

  return rva;
}

PIMAGE_SECTION_HEADER PEFile::section(uint16_t index) const
{
  if (index < numberOfSections()) {
    return &m_pFirstSection[index];
  }
  else {
    fprintf(stderr, "PEFile::section - index (%d) out of bound\n", index);
  }
  return nullptr;
}

PIMAGE_SECTION_HEADER PEFile::sectionByName(const char *name) const
{
  PIMAGE_SECTION_HEADER pSection = m_pFirstSection;
  bool found = false;
  for (uint16_t i = 0; i < m_pNTHeaders->FileHeader.NumberOfSections; ++i, ++pSection) {
    if (strcmp((const char *)pSection->Name, name) == 0) {
      found = true;
      break;
    }
  }
  return found ? pSection : nullptr;
}

PIMAGE_SECTION_HEADER PEFile::enclosingSectionHeader(uint32_t rva) const
{
  PIMAGE_SECTION_HEADER pSection = m_pFirstSection;
  bool found = false;
  for (uint16_t i = 0; i < m_pNTHeaders->FileHeader.NumberOfSections; ++i, ++pSection) {
    uint32_t limit = pSection->SizeOfRawData != 0 ? pSection->SizeOfRawData : pSection->Misc.VirtualSize;
    if (rva >= pSection->VirtualAddress && (rva <= pSection->VirtualAddress + limit)) {
      found = true;
      break;
    }
  }
  return found ? pSection : nullptr;
}

uint32_t PEFile::directoryRva(uint32_t directoryIndex) const
{
  if (directoryIndex > IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR) {
    fprintf(stderr, "PEFile::directoryRva - directoryIndex (%d) out of bound\n", directoryIndex);
    return 0;
  }

  return m_pNTHeaders->OptionalHeader.DataDirectory[directoryIndex].VirtualAddress;
}

uint32_t PEFile::directorySize(uint32_t directoryIndex) const
{
  if (directoryIndex > IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR) {
    fprintf(stderr, "PEFile::directorySize - directoryIndex (%d) out of bound\n", directoryIndex);
    return 0;
  }

  return m_pNTHeaders->OptionalHeader.DataDirectory[directoryIndex].Size;
}

void PEFile::setDirectoryRva(uint32_t directoryIndex, uint32_t rva)
{
  if (directoryIndex > IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR) {
    fprintf(stderr, "PEFile::setDirectoryRva - directoryIndex (%d) out of bound\n", directoryIndex);
  }
  else {
    m_pNTHeaders->OptionalHeader.DataDirectory[directoryIndex].VirtualAddress = rva;
  }
}

void PEFile::setDirectorySize(uint32_t directoryIndex, uint32_t size)
{
  if (directoryIndex > IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR) {
    fprintf(stderr, "PEFile::setDirectorySize - directoryIndex (%d) out of bound\n", directoryIndex);
  }
  else {
    m_pNTHeaders->OptionalHeader.DataDirectory[directoryIndex].Size = size;
  }
}