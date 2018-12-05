[BITS 32]

%include "pe.inc"

section .text
global _loader_start
global _loader_code_end
global _loader_end
global _ImageBase
global _OldImageBase
global _XTEAKey
global _DecryptTableVA
global _OriginalEPVA
global _VirtualProtectVA
global _LoadLibraryAVA
global _OriginalITRVA
global _OriginalITSize
global _OriginalRelocRVA
global _OriginalRelocSize
global _ITSectionRVA
global _ITSectionSize
XTEA_DELTA equ 0x9E3779B9

; the first stage loader will jump here
; we will find the same stack as the WinMain
; This function will decrypt all the sections, resolve imports, apply relocations, restore the original entry point
; and finally jump to the original entry point
; [ebp+8] -> hInstance
; [ebp+12] -> hPrevInstance
; [ebp+16] -> lpCmdLine
; [ebp+20] -> nCmdShow

_loader_start:
secondstageLoader:
	push ebp
	mov ebp, esp
	push esi
	push edi
	push ebx
	call .base
.base:
	pop ebx
	
	push ebx ; first parameter will always be the loader base
	call decrypt_everything
	
	;push ebx
	;call apply_relocations
	
	push ebx
	call resolve_imports
	
	mov eax, [ebx+@OriginalEP]
	pop ebx
	pop edi
	pop esi
	mov esp, ebp
	pop ebp
	call .next
.next:
	mov [esp], eax
	ret ; return to the original entry point
	    ; hopefully this code sequence will be enough legit for the euristic engine of an AV
	
; decrypt all sections in the array of DecryptEntry structures
; each element in the structure is made up of two DWORDs: the first DWORD is the rva of the section
; to be decrypted, the second DWORD is the size of the data to decrypt
; [ebp+08] loader base
; eax, ecx and edx trashed
; all other registers preserved, as required by x86 calling conventions
decrypt_everything:
	push ebp
	mov ebp, esp
	sub esp, 0x14 ; allocate some locals space
	push esi
	push edi
	push ebx
	mov ebx, [ebp+08] ; ebx points to the loader base
	mov esi, [ebx+@DecryptTable] ; esi points to the array of DecryptEntry structs
.next:
	push esi
	mov eax, [esi] ; eax = RVA
	test eax, eax
	jz .end
	mov ecx, [esi+4] ; ecx = size
	test ecx, ecx
	jz .end
	
	add eax, [ebx+@ImageBase] ; eax = VA = Virtual Address of section to be decrypted
	
	mov [ebp-08], eax ; [ebp-08] = VA
	mov [ebp-12], ecx ; [ebp-12] = size
	
	movzx ecx, byte [esi+8]
	mov [ebp-16], ecx
	test ecx, ecx
	jz .skip1
	; change protection flag and add writable flag, so we can safely decrypt the sections
	; the old flags will be restored at the end
	lea esi, [ebp-04]
	push esi ; lpflOldProtect
	push 0x40 ; flNewProtect = PAGE_EXECUTE_READWRITE
	push ecx ; dwSize
	push eax ; lpAddress
	mov edx, [ebx+@VirtualProtect]
	call [edx]
	test eax, eax
	jz .end
	
.skip1:
	; now we have PAGE_READWRITE permission, decrypt everything and change the page
	; protection back to the original one
	lea edx, [ebx+@XTEAKey]
	push edx
	push dword [ebp-12]
	push dword [ebp-08]
	call xteaDecrypt
	
	mov ecx, [ebp-16]
	test ecx, ecx
	jz .skip2
	;restore the original protection
	lea esi, [ebp-04]
	push esi
	push dword [esi]
	push dword [ebp-12]
	push dword [ebp-08]
	mov edx, [ebx+@VirtualProtect]
	call [edx]
.skip2:
	pop esi
	add esi, 9
	jmp .next

.end:
	xor eax, eax
	pop esi
	pop ebx
	pop edi
	pop esi
	mov esp, ebp
	pop ebp
	ret 4
	
xteaDecipher:
	mov edx, XTEA_DELTA*64 ; edx = sum
	mov ecx, 64
.next:
	mov eax, [esi] ;eax = v0
	mov ebx, eax ; ebx = v0
	shl eax, 4 ; eax = eax << 4 = v0 << 4
	shr ebx, 5 ; ebx = ebx >> 5 = v0 >> 5
	xor eax, ebx ; eax = eax ^ ebx = (v0 << 4) ^ (v0 >> 5)
	add eax, [esi] ; eax = eax + [esi] = eax + v0 = ((v0 << 4) ^ (v0 >> 5)) + v0
	push edx ; save edx (edx = sum)
	shr edx, 11 ; edx = edx >> 1 = sum >> 11
	and edx, 3 ; edx = edx & 3 = (sum >> 11) & 3
	mov ebx, [edi+edx*4] ; ebx = edi[edx] = key[edx] = key[(sum >> 11) & 3]
	pop edx ; restore edx
	add ebx, edx ; ebx = ebx + edx = key[(sum >> 11) & 3] + sum
	xor eax, ebx ; eax = eax ^ ebx = (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (key[(sum >> 11) & 3] + sum)
	sub [esi+4], eax ; v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (key[(sum >> 11) & 3] + sum)
	
	sub edx, XTEA_DELTA
	
	mov eax, [esi+4] ; eax = v0
	mov ebx, eax
	shl eax, 4
	shr ebx, 5
	xor eax, ebx
	add eax, [esi+4]
	push edx
	and edx, 3
	mov ebx, [edi+edx*4] ; ebx = key[sum & 3]
	pop edx
	add ebx, edx
	xor eax, ebx
	sub [esi], eax
	dec ecx
	jnz .next
	ret
	
xteaDecrypt:
	push ebp
	mov ebp, esp
	push esi
	push edi
	push ebx
	mov esi, [ebp+8]
	mov edi, [ebp+16]
.next:
	call xteaDecipher
	add esi, 8
	sub dword [ebp+12], 8
	jnz .next
	pop ebx
	pop edi
	pop esi
	mov esp, ebp
	pop ebp
	ret 12
	
apply_relocations:
	push ebp
	mov ebp, esp
	sub esp, 0x10
	push esi
	push edi
	push ebx
	mov ebx, [ebp+08] ; ebx = loader base
	mov esi, [ebx+@OriginalRelocRVA]
	add esi, [ebx+@ImageBase]
	mov ecx, [ebx+@OriginalRelocSize]
.nextReloc:
	test ecx, ecx
	jz .relocEnd
	mov eax, [esi+IMAGE_BASE_RELOCATION.VirtualAddress] ; eax = Base RVA for reloc
	lea edx, [ebp-04]
	; make one page from Base RVA writable, so we can apply relocations
	push ecx
	push edx
	push 0x40 ; PAGE_EXECUTE_READWRITE
	push 0x1000 ; 4k page
	add eax, [ebx+@ImageBase]
	push eax
	mov edx, [ebx+@VirtualProtect]
	call [edx]
	pop ecx
	test eax, eax
	jz .relocEnd
	mov eax, [esi+IMAGE_BASE_RELOCATION.VirtualAddress] ; eax = Base RVA for reloc
	add eax, [ebx+@ImageBase] ; eax = Base VA for reloc
	mov edx, [esi+IMAGE_BASE_RELOCATION.SizeOfBlock] ; edx = sizeof(WORD)*numReloc + sizeof(IMAGE_BASE_RELOCATION)
	sub ecx, edx ; update processed size
	sub edx, IMAGE_BASE_RELOCATION_size
	shr edx, 1 ; edx = number of relocation entries for this base RVA
	add esi, IMAGE_BASE_RELOCATION_size ; esi will point to the first reloc entry
	push ecx
.nextEntry:
	test edx, edx
	jz .entriesEnd
	movzx edi, word [esi]
	add esi, 2
	dec edx
	mov ecx, edi
	and ecx, 0x3000 ; test for HIGHLOW base reloc
	test ecx, ecx
	jz  .nextEntry
	and edi, 0xFFF ; retrieve the lowest 12bits
	add edi, eax ; the relocation will be applied to the dword pointed by edi
	mov ecx, [ebx+@ImageBase]
	add [edi], ecx ; add new image base
	mov ecx, [ebx+@OldImageBase]
	sub [edi], ecx ; subtract old image base -> the DWORD at [edi] is now relocated
	jmp .nextEntry
.entriesEnd:
	; restore original protection for this page
	lea edx, [ebp-04]
	push edx
	push dword [ebp-04]
	push 0x1000
	push eax
	mov edx, [ebx+@VirtualProtect]
	call [edx]
	pop ecx
	test eax, eax
	jz .relocEnd
	jmp .nextReloc
.relocEnd:
	xor eax, eax
	pop ebx
	pop edi
	pop esi
	mov esp, ebp
	pop ebp
	ret 04

compare_strings:
	push ebp
	mov ebp, esp
	push esi
	push edi
	push ebx
	mov esi, dword [ebp+08]
	mov edi, dword [ebp+12]
	xor eax, eax
.loop:
	mov al, byte [esi]
	mov bl, byte [edi]
	test al, al
	jz .end
	sub al, bl
	jnz .end
	inc esi
	inc edi
	jmp .loop
	sub al, bl
.end:
	pop ebx
	pop edi
	pop esi
	mov esp, ebp
	pop ebp
	ret 08
	
get_proc_address:
	push ebp
	mov ebp, esp
	sub esp, 0x1C
	push esi
	push edi
	push ebx
	mov ebx, [ebp+08]
	mov esi, [ebp+0x0C]
	add esi, [esi+IMAGE_DOS_HEADER.e_lfanew] ; esi points to IMAGE_NT_HEADERS
	add esi, IMAGE_NT_HEADERS32.OptionalHeader + IMAGE_OPTIONAL_HEADER32.DataDirectory ; the export table is the first entry in the directory array
	mov eax, [esi] ; eax = export directory RVA
	mov [ebp-16], eax ; [ebp-16] = Export directory RVA
	mov eax, [esi+4] ; eax = Export directory virtual size
	mov [ebp-20], eax ; [ebp-20] = Export directory virtual size
	mov esi, [esi]
	add esi, [ebp+0x0C] ; esi now points to export table
	mov eax, [esi+IMAGE_EXPORT_DIRECTORY.AddressOfNames]
	add eax, [ebp+0x0C] ; eax points to name array
	mov [ebp-04], eax
	mov eax, [esi+IMAGE_EXPORT_DIRECTORY.AddressOfFunctions]
	add eax, [ebp+0x0C] ; eax points to functions array
	mov [ebp-08], eax
	mov eax, [esi+IMAGE_EXPORT_DIRECTORY.AddressOfNameOrdinals]
	add eax, [ebp+0x0C] ; eax points to ordinals array
	mov [ebp-12], eax
	mov ecx, [esi+IMAGE_EXPORT_DIRECTORY.NumberOfNames]
	xor edx, edx ; loop counter
	mov esi, [ebp-04] ; esi points to name array
.startLoop:
	test ecx, ecx
	jz .done
	push ecx
	push edx
	mov eax, [esi+edx*4]
	add eax, [ebp+0x0C]
	push eax
	push dword [ebp+16]
	call compare_strings
	pop edx
	pop ecx
	jz .found
	dec ecx
	inc edx
	jmp .startLoop
.found:
	mov esi, [ebp-12] ; eax = index into ordinal array
	movzx eax, word [esi+edx*2] ; eax = ordinal number for exported function
	mov esi, [ebp-08] ; esi = functions array
	mov eax, [esi+eax*4] ; eax = address of function with index eax
	
; check if the export is forwared
; an export is forwarded if RVA >= ET_RVA && RVA <= ET_RVA+ET_Size, that is
; if the export RVA resides within the export table and not the .text section
	mov edx, [ebp-16] ; edx = Export directory RVA
	cmp eax, edx
	jl .notForwarded
	add edx, [ebp-20] ; edx = Export Directory RVA + Export Directory virtual size
	cmp eax, edx
	jg .notForwarded
	; export is forwarded, handle it
	add eax, [ebp+0x0C]
	push eax
	push ebx
	call handle_forwarded_export
	jmp .done
.notForwarded:
	add eax, [ebp+0x0C]
.done:
	pop ebx
	pop edi
	pop esi
	mov esp, ebp
	pop ebp
	ret 0x0C
	
handle_forwarded_export:
	push ebp
	mov ebp, esp
	; reserve 256 bytes on the stack for "DLL_Name\0FunctionName\0"
	sub esp, 0x18+0x100
	push esi
	push edi
	push ebx
	mov ebx, [ebp+08]
	mov esi, [ebp+0x0C]
	lea edi, [ebp-200]
.copyExport:
	mov al, byte [esi]
	mov byte [edi], al
	test al, al
	jz .copyDone
	cmp al, '.'
	jne .continue
	xor al, al
	mov byte [edi], al
.continue:
	inc esi
	inc edi
	jmp .copyExport
.copyDone:
	lea eax, [ebp-200]
	push eax
	mov edx, [ebx+@LoadLibraryA]
	call [edx]
	mov [ebp-04], eax
	lea esi, [ebp-200]
.getName:
	mov al, byte [esi]
	inc esi
	test al, al
	jz .done
	jmp .getName
.done:
	push esi
	push dword [ebp-04]
	push ebx
	call get_proc_address
	pop ebx
	pop edi
	pop esi
	mov esp, ebp
	pop ebp
	ret 08
	
resolve_imports:
	push ebp
	mov ebp, esp
	sub esp, 0x10
	push esi
	push edi
	push ebx
	mov ebx, [ebp+08] ; ebx = loader base
	mov eax, [ebx+@ITSectionRVA]
	add eax, [ebx+@ImageBase]
	lea edx, [ebp-08]
	push edx
	push 0x4
	push dword [ebx+@ITSectionSize]
	push eax
	mov edx, [ebx+@VirtualProtect]
	call [edx]
	test eax, eax
	jz .importEnd_noprotect
	mov esi, [ebx+@OriginalITRVA]
	add esi, [ebx+@ImageBase]
.nextImport:
	cmp dword [esi+IMAGE_IMPORT_DESCRIPTOR.Name], 0
	je .importEnd
	cmp dword [esi+IMAGE_IMPORT_DESCRIPTOR.FirstThunk], 0
	je .importEnd
	mov eax, [esi+IMAGE_IMPORT_DESCRIPTOR.Name]
	add eax, [ebx+@ImageBase]
	push eax
	mov edx, [ebx+@LoadLibraryA]
	call [edx]
	test eax, eax
	jz .importEnd
	mov [ebp-04], eax ; [ebp-04] = library handle
	mov eax, [esi+IMAGE_IMPORT_DESCRIPTOR.OriginalFirstThunk]
	mov edi, [esi+IMAGE_IMPORT_DESCRIPTOR.FirstThunk]
	add edi, [ebx+@ImageBase] ; edi points to the IAT (will be overwrited with function entry points)
	test eax, eax
	jz .useFirstThunk
	jmp .continue
.useFirstThunk:
	mov eax, [esi+IMAGE_IMPORT_DESCRIPTOR.FirstThunk]
.continue:
	add eax, [ebx+@ImageBase] ; eax now points to the thunk array
.nextThunk:
	cmp dword [eax], 0
	je .thunkEnd
	push eax ; save pointer to thunk array
	mov ecx, [eax]
	mov edx, ecx
	and edx, 0x80000000
	jz .importByName
	; the function is imported by ordinal, clear most significant word and resolve import
	and ecx, 0x0000FFFF
	jmp .doImport
.importByName:
	add ecx, 2 ; skip hint (2 bytes), ecx points to the null-terminated import name
	add ecx, [ebx+@ImageBase]
.doImport:
	push ecx
	push dword [ebp-4]
	push ebx
	call get_proc_address
	mov [edi], eax ; store the entry point into the IAT
	add edi, 4 ; next IAT entry
	pop eax
	mov dword [eax], 0
	add eax, 4 ; next OriginalFirstThunk (or FirstThunk if OFT is absent) entry
	jmp .nextThunk
.thunkEnd:
	add esi, IMAGE_IMPORT_DESCRIPTOR_size
	jmp .nextImport
.importEnd:
	lea eax, [ebp-08]
	push eax
	push dword [ebp-08]
	push dword [ebx+@ITSectionSize]
	mov eax, [ebx+@ITSectionRVA]
	add eax, [ebx+@ImageBase]
	push eax
	mov edx, [ebx+@VirtualProtect]
	call [edx]
.importEnd_noprotect:
	xor eax, eax
	pop ebx
	pop edi
	pop esi
	mov esp, ebp
	pop ebp
	ret 04
align 4
_loader_code_end:
	; the loader will insert here the VA with the preferred image base
	; then they will be patched through a fake .reloc section
	; thank you Win32 for making our life easier :D
	_XTEAKey times 4 dd 0
	_ImageBase dd 0 ; after loading, this will contain the new image base
	_OldImageBase dd 0 ; this contains the old image base (used to apply relocations)
	
	; this RVA points to an array of structures, where each structure contains
	; two dwords, the first one is the RVA of the section to decrypt,
	; the second one is the total size of the data to be decrypted
	_DecryptTableVA dd 0
	
	; original entry point
	_OriginalEPVA dd 0
	
	; this will be filled by the packer, with the RVA of the API's thunk
	; this way the system will resolve this apis for us
	_VirtualProtectVA dd 0
	_LoadLibraryAVA dd 0
	_OriginalITRVA dd 0
	_OriginalITSize dd 0 ; useless
	_OriginalRelocRVA dd 0
	_OriginalRelocSize dd 0
	_ITSectionRVA dd 0 ; section which contains the import table
	_ITSectionSize dd 0 ; size of the section that contains the import table
align 4
@XTEAKey equ _XTEAKey - secondstageLoader.base
@ImageBase equ _ImageBase - secondstageLoader.base
@OldImageBase equ _OldImageBase - secondstageLoader.base
@DecryptTable equ _DecryptTableVA - secondstageLoader.base
@OriginalEP equ _OriginalEPVA - secondstageLoader.base
@VirtualProtect equ _VirtualProtectVA - secondstageLoader.base
@LoadLibraryA equ _LoadLibraryAVA - secondstageLoader.base
@OriginalITRVA equ _OriginalITRVA - secondstageLoader.base
@OriginalITSize equ _OriginalITSize - secondstageLoader.base
@OriginalRelocRVA equ _OriginalRelocRVA - secondstageLoader.base
@OriginalRelocSize equ _OriginalRelocSize - secondstageLoader.base
@ITSectionRVA equ _ITSectionRVA - secondstageLoader.base
@ITSectionSize equ _ITSectionSize - secondstageLoader.base
_loader_end: