IMAGE_DIRECTORY_ENTRY_EXPORT                equ  0
IMAGE_DIRECTORY_ENTRY_IMPORT                equ  1
IMAGE_DIRECTORY_ENTRY_RESOURCE              equ  2
IMAGE_DIRECTORY_ENTRY_EXCEPTION             equ  3
IMAGE_DIRECTORY_ENTRY_SECURITY              equ  4
IMAGE_DIRECTORY_ENTRY_BASERELOC             equ  5
IMAGE_DIRECTORY_ENTRY_DEBUG					equ  6
IMAGE_DIRECTORY_ENTRY_COPYRIGHT             equ  7
IMAGE_DIRECTORY_ENTRY_GLOBALPTR             equ  8
IMAGE_DIRECTORY_ENTRY_TLS					equ  9
IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG           equ 10
IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT          equ 11
IMAGE_DIRECTORY_ENTRY_IAT					equ 12
IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT			equ 13
IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR		equ 14
IMAGE_NUMBEROF_DIRECTORY_ENTRIES            equ 16

STRUC IMAGE_DATA_DIRECTORY
	.VirtualAddress: resd 1
	.iSize: resd 1
ENDSTRUC

STRUC IMAGE_OPTIONAL_HEADER32
	.Magic:							resw 1
	.MajorLinkerVersion:			resb 1
	.MinorLinkerVersion: 			resb 1
	.SizeOfCode:					resd 1
	.SizeOfInitializedData:			resd 1
	.SizeOfUninitializedData:		resd 1
	.AddressOfEntryPoint:			resd 1
	.BaseOfCode:					resd 1
	.BaseOfData:					resd 1
	.ImageBase:						resd 1
	.SectionAlignment:				resd 1
	.FileAlignment:					resd 1
	.MajorOperatingSystemVersion:	resw 1
	.MinorOperatingSystemVersion:	resw 1
	.MajorImageVersion:				resw 1
	.MinorImageVersion:				resw 1
	.MajorSubsystemVersion:			resw 1
	.MinorSubsystemVersion:			resw 1
	.Win32VersionValue:				resd 1
	.SizeOfImage:					resd 1
	.SizeOfHeaders:					resd 1
	.CheckSum:						resd 1
	.Subsystem:						resw 1
	.DllCharacteristics:			resw 1
	.SizeOfStackReserve:			resd 1
	.SizeOfStackCommit:				resd 1
	.SizeOfHeapReserve:				resd 1
	.SizeOfHeapCommit:				resd 1
	.LoaderFlags:					resd 1
	.NumberOfRvaAndSizes:			resd 1
	.DataDirectory:					resb 16*IMAGE_DATA_DIRECTORY_size	
ENDSTRUC

STRUC IMAGE_FILE_HEADER
	.Machine:				resw 1
	.NumberOfSections:		resw 1
	.TimeDateStamp:			resd 1
	.PointerToSymbolTable:	resd 1
	.NumberOfSymbols:		resd 1
	.SizeOfOptionalHeader:	resw 1
	.Characteristics:		resw 1
ENDSTRUC

STRUC IMAGE_NT_HEADERS32
	.Signature:			resd 1
	.FileHeader:		resb IMAGE_FILE_HEADER_size
	.OptionalHeader:	resb IMAGE_OPTIONAL_HEADER32_size
ENDSTRUC

STRUC IMAGE_DOS_HEADER
	.e_magic:		resw 1
	.e_cblp:		resw 1
	.e_cp:			resw 1
	.e_crlc:		resw 1
	.e_cparhdr:		resw 1
	.e_minalloc:	resw 1
	.e_maxalloc: 	resw 1
	.e_ss:       	resw 1
	.e_sp:       	resw 1
	.e_csum:     	resw 1
	.e_ip:       	resw 1
	.e_cs:       	resw 1
	.e_lfarlc:   	resw 1
	.e_ovno:     	resw 1
	.e_res:      	resw 4
	.e_oemid:    	resw 1
	.e_oeminfo:  	resw 1
	.e_res2:     	resw 10
	.e_lfanew:   	resd 1
ENDSTRUC

STRUC IMAGE_BASE_RELOCATION
	.VirtualAddress:	resd 1
	.SizeOfBlock:		resd 1
ENDSTRUC

STRUC IMAGE_IMPORT_DESCRIPTOR
	.OriginalFirstThunk:	resd 1
	.TimeDateStamp:			resd 1
	.ForwarderChain:		resd 1
	.Name:					resd 1
	.FirstThunk:			resd 1
ENDSTRUC

STRUC IMAGE_EXPORT_DIRECTORY
	.Characteristics:		resd 1
	.TimeDateStamp:			resd 1
	.MajorVersion:			resw 1
	.MinorVersion:			resw 1
	.Name:					resd 1
	.Base:					resd 1
	.NumberOfFunctions:		resd 1
	.NumberOfNames:			resd 1
	.AddressOfFunctions:	resd 1
	.AddressOfNames:		resd 1
	.AddressOfNameOrdinals: resd 1
ENDSTRUC