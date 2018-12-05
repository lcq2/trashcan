#ifndef PEFILE_H
#define PEFILE_H
#include <stdint.h>
#include <string>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

class PEFile
{
public:
  PEFile(const std::string& filename);

  bool map();
  void unmap(bool checksum = false);
  bool updateChecksum();
  uint8_t *filebase() const { return m_pFilebase; }
  template<typename T> uint32_t rvaToOffset(T rva) { return rvaToOffset(static_cast<uint32_t>(rva & 0xFFFFFFFF)); }
  template<typename T> uint32_t offsetToRva(T offset) { return offsetToRva(static_cast<uint32_t>(offset & 0xFFFFFFFF)); }
  uint32_t rvaToOffset(uint32_t rva);
  uint32_t offsetToRva(uint32_t offset);
  template<typename T> uint32_t pointerToRva(T *ptr)
  {
    return offsetToRva(reinterpret_cast<uint8_t*>(ptr)-m_pFilebase);
  }
  //some helpers
  uint16_t numberOfSections() const { return m_pNTHeaders->FileHeader.NumberOfSections; }
  PIMAGE_SECTION_HEADER section(uint16_t index) const;
  PIMAGE_SECTION_HEADER sectionByName(const char *name) const;
  PIMAGE_SECTION_HEADER enclosingSectionHeader(uint32_t rva) const;
  uint32_t directoryRva(uint32_t directoryIndex) const;
  uint32_t directorySize(uint32_t directoryIndex) const;
  void setDirectoryRva(uint32_t directoryIndex, uint32_t rva);
  void setDirectorySize(uint32_t directoryIndex, uint32_t size);

  PIMAGE_NT_HEADERS ntHeaders() const { return m_pNTHeaders; }
  PIMAGE_SECTION_HEADER firstSection() const { return m_pFirstSection; }

private:
  std::string m_filename;
  uint8_t *m_pFilebase;
  HANDLE m_hFile;
  HANDLE m_hMappedFile;
  LARGE_INTEGER m_filesize;
  PIMAGE_DOS_HEADER m_pDOSHeader;
  PIMAGE_NT_HEADERS m_pNTHeaders;
  PIMAGE_SECTION_HEADER m_pFirstSection;
};

#endif