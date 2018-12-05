[BITS 64]

%include "pe.inc"

section .text
global loader_start
global loader_end
global ImageBase
global OldImageBase
global XTEAKey
global DecryptTableVA
global OriginalEPVA
global GetProcAddressVA
global VirtualProtectVA
global LoadLibraryAVA
global OriginalITRVA
global OriginalITSize
global OriginalRelocRVA
global OriginalRelocSize
global ITSectionRVA
global ITSectionSize
XTEA_DELTA equ 0x9E3779B9

; constants for shadow memory access
; rsp based upon entry (i.e. you can access spill space like this:
; mov [rsp+SPILL1], rcx
; if you allocate some space on the stack (either for locals or for function calls), then
; of course you need to take that space into account, i.e.:
; [...]
; sub rsp, 0x28
; [...]
; mov rax, [rsp+0x28+SPILL1]
SPILL1 equ 08
SPILL2 equ 16
SPILL3 equ 24
SPILL4 equ 32

loader_start:
	sub rsp, 0x28 ; realign the stack (required by x64 calling convention)
	call decrypt_everything
	call apply_relocations
	call resolve_imports
	mov rax, [rel OriginalEPVA]
	add rsp, 0x28
	call .next
.next:
	mov [rsp], rax ; put return address as the first element in the stack
	ret
	
; rcx = VA of DecryptTable array
decrypt_everything:
	mov rax, [rel DecryptTableVA]
	mov [rsp+SPILL1], rax
	sub rsp, 0x38 ; realign the stack
.next:
	xor rcx, rcx
	xor rdx, rdx
	mov ecx, dword [rax]
	test rcx, rcx
	jz .end
	mov edx, dword [rax+4]
	test rdx, rdx
	jz .end
	
	add rcx, [rel ImageBase]
	
	mov r8, 0x40
	lea r9, [rsp+0x28]
	mov r10, [rel VirtualProtectVA]
	call [r10]
	test rax, rax
	jz .end
	
	mov rax, [rsp+0x38+SPILL1]	
	xor rcx, rcx
	mov ecx, dword [rax]
	add rcx, [rel ImageBase]
	xor r8, r8
	mov r8d, dword [rax+4]
	lea rdx, [rel XTEAKey]
	call xteaDecrypt
	
	mov rax, [rsp+0x38+SPILL1]
	xor rcx, rcx
	xor rdx, rdx
	mov ecx, dword [rax]
	mov edx, dword [rax+4]
	add rcx, [rel ImageBase]
	mov r8, [rsp+0x28]
	lea r9, [rsp+0x28]
	mov r10, [rel VirtualProtectVA]
	call [r10]
	mov rax, [rsp+0x38+SPILL1]
	add rax, 8
	mov [rsp+0x38+SPILL1], rax
	jmp .next
.end:
	add rsp, 0x38
	ret

; decrypts two DWORDs
; rcx = address of buffer to decrypt
; rdx = pointer to decryption key buffer
; since this is a leaf function (i.e. a function that does not call other fucntions,
; we do not need to allocate space for shadow parameters and/or realign the stack
xteaDecipher:
	mov [rsp+SPILL1], rcx
	mov [rsp+SPILL2], rdx
	mov [rsp+SPILL3], rsi
	mov [rsp+SPILL4], rdi
	push rbx
	xor rdx, rdx
	mov edx, XTEA_DELTA*64
	mov rcx, 64
	mov rsi, [rsp+0x8+SPILL1]
	mov rdi, [rsp+0x8+SPILL2]
.next:
	mov eax, [rsi] ;eax = v0
	mov ebx, eax ; ebx = v0
	shl eax, 4 ; eax = eax << 4 = v0 << 4
	shr ebx, 5 ; ebx = ebx >> 5 = v0 >> 5
	xor eax, ebx ; eax = eax ^ ebx = (v0 << 4) ^ (v0 >> 5)
	add eax, [rsi] ; eax = eax + [esi] = eax + v0 = ((v0 << 4) ^ (v0 >> 5)) + v0
	xor r10, r10
	mov r10d, edx
	shr r10d, 11 ; edx = edx >> 1 = sum >> 11
	and r10d, 3 ; edx = edx & 3 = (sum >> 11) & 3
	mov ebx, [rdi+r10*4] ; ebx = edi[edx] = key[edx] = key[(sum >> 11) & 3]
	add ebx, edx ; ebx = ebx + edx = key[(sum >> 11) & 3] + sum
	xor eax, ebx ; eax = eax ^ ebx = (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (key[(sum >> 11) & 3] + sum)
	sub [rsi+4], eax ; v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (key[(sum >> 11) & 3] + sum)
	
	sub edx, XTEA_DELTA
	
	mov eax, [rsi+4] ; eax = v1
	mov ebx, eax
	shl eax, 4
	shr ebx, 5
	xor eax, ebx
	add eax, [rsi+4]
	xor r10, r10
	mov r10d, edx
	and r10d, 3
	mov ebx, [rdi+r10*4] ; ebx = key[sum & 3]
	add ebx, edx
	xor eax, ebx
	sub [rsi], eax
	dec ecx
	jnz .next
	mov rsi, [rsp+0x8+SPILL3]
	mov rdi, [rsp+0x8+SPILL4]
	pop rbx
	ret
	
; rcx = address of buffer to decrypt
; rdx = pointer to decryption key buffer
; r8 = size of buffer to decrypt
xteaDecrypt:
	mov [rsp+SPILL1], rcx
	mov [rsp+SPILL2], rdx
	mov [rsp+SPILL3], r8
	sub rsp, 0x28
.next:
	test r8, r8
	jz .end
	mov rcx, [rsp+0x28+SPILL1]
	mov rdx, [rsp+0x28+SPILL2]
	call xteaDecipher
	mov r8, [rsp+0x28+SPILL3]
	sub r8, 8
	mov [rsp+0x28+SPILL3], r8
	mov rcx, [rsp+0x28+SPILL1]
	add rcx, 8
	mov [rsp+0x28+SPILL1], rcx
	jmp .next
.end:
	add rsp, 0x28
	xor rax, rax
	ret
	
apply_relocations:
	push r12
	push r13
	push r14
	push r15
	sub rsp, 0x28
	xor r12, r12
	xor r13, r13
	mov r12d, [rel OriginalRelocRVA]
	add r12, [rel ImageBase]
	mov r13d, [rel OriginalRelocSize] ; r13 = reloc size
	mov r14, [rel ImageBase]
	sub r14, [rel OldImageBase] ; r14 = reloc delta
.nextReloc:
	test r13, r13
	jz .relocEnd
	xor rcx, rcx
	mov ecx, [r12+IMAGE_BASE_RELOCATION.VirtualAddress] ; eax = Base RVA for reloc
	add rcx, [rel ImageBase]
	mov rdx, 0x1000
	mov r8, 0x40
	lea r9, [rsp+0x20]
	mov r10, [rel VirtualProtectVA]
	call [r10]
	test rax, rax
	jz .relocEnd
	xor rax, rax
	xor rdx, rdx
	mov eax, [r12+IMAGE_BASE_RELOCATION.VirtualAddress]
	add rax, [rel ImageBase]
	mov edx, [r12+IMAGE_BASE_RELOCATION.SizeOfBlock]
	sub r13, rdx
	sub rdx, IMAGE_BASE_RELOCATION_size
	shr rdx, 1
	add r12, IMAGE_BASE_RELOCATION_size
.nextEntry:
	test rdx, rdx
	jz .entriesEnd
	movzx r15, word [r12]
	add r12, 2
	dec rdx
	mov rcx, r15
	and rcx, 0xA000
	test rcx, rcx
	jz .nextEntry
	and r15, 0xFFF
	add r15, rax
	mov rcx, [rel ImageBase]
	add [r15], r14
	jmp .nextEntry
.entriesEnd:
	mov rcx, rax
	mov rdx, 0x1000
	mov r8, [rsp+0x20]
	lea r9, [rsp+0x20]
	mov r10, [rel VirtualProtectVA]
	call [r10]
	test rax, rax
	jz .relocEnd
	jmp .nextReloc
.relocEnd:
	add rsp, 0x28
	pop r15
	pop r14
	pop r13
	pop r12
	ret
	
resolve_imports:
	push r12
	push r13
	push r14
	push r15
	sub rsp, 0x38
	xor r12, r12
	xor rdx, rdx
	mov r12d, [rel ITSectionRVA]
	add r12, [rel ImageBase]
	mov rcx, r12
	mov edx, [rel ITSectionSize]
	mov r8, 0x4
	lea r9, [rsp+0x20]
	mov r10, [rel VirtualProtectVA]
	call [r10]
	test rax, rax
	jz .importEnd_noprotect
	mov r12d, [rel OriginalITRVA]
	add r12, [rel ImageBase]
.nextImport:
	cmp dword [r12+IMAGE_IMPORT_DESCRIPTOR.Name], 0
	je .importEnd
	cmp dword [r12+IMAGE_IMPORT_DESCRIPTOR.FirstThunk], 0
	je .importEnd
	xor rcx, rcx
	mov ecx, dword [r12+IMAGE_IMPORT_DESCRIPTOR.Name]
	add rcx, [rel ImageBase]
	mov r10, [rel LoadLibraryAVA]
	call [r10]
	test rax, rax
	jz .importEnd
	mov r13, rax
	xor r14, r14
	xor r15, r15
	mov r15d, [r12+IMAGE_IMPORT_DESCRIPTOR.OriginalFirstThunk]
	mov r14d, [r12+IMAGE_IMPORT_DESCRIPTOR.FirstThunk]
	add r14, [rel ImageBase]
	test r15, r15
	jz .useFirstThunk
	jmp .continue
.useFirstThunk:
	mov r15d, dword [r12+IMAGE_IMPORT_DESCRIPTOR.FirstThunk]
.continue:
	add r15, [rel ImageBase]
.nextThunk:
	cmp qword [r15], 0
	je .thunkEnd
	mov rdx, [r15]
	mov r10, rdx
	mov r8, 0x8000000000000000
	and r10, r8
	jz .importByName
	; the function is imported by ordinal, clear most significant word and resolve import
	mov r8, 0x000000000000FFFF
	and rdx, r8
	jmp .doImport
.importByName:
	add rdx, 2
	add rdx, [rel ImageBase]
.doImport:
	mov rcx, r13
	mov r10, [rel GetProcAddressVA]
	call [r10]
	mov [r14], rax
	add r14, 8
	mov qword [r15], 0
	add r15, 8
	jmp .nextThunk
.thunkEnd:
	add r12, IMAGE_IMPORT_DESCRIPTOR_size
	jmp .nextImport
.importEnd:
	mov rcx, [rel ITSectionRVA]
	add rcx, [rel ImageBase]
	mov rdx, [rel ITSectionSize]
	mov r8, [rsp+0x20]
	lea r9, [rsp+0x20]
	mov r10, [rel VirtualProtectVA]
	call [r10]
.importEnd_noprotect:
	add rsp, 0x38
	pop r15
	pop r14
	pop r13
	pop r12
	xor rax, rax
	ret
align 4
loader_code_end:
	XTEAKey times 4 dd 0
	ImageBase dq 0
	OldImageBase dq 0
	
	DecryptTableVA dq 0
	OriginalEPVA dq 0
	
	GetProcAddressVA dq 0
	VirtualProtectVA dq 0
	LoadLibraryAVA dq 0
	OriginalITRVA dd 0
	OriginalITSize dd 0
	OriginalRelocRVA dd 0
	OriginalRelocSize dd 0
	ITSectionRVA dd 0
	ITSectionSize dd 0
align 4
loader_end: