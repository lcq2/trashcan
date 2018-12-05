# trashcan
Research projects I'm working on, but not really mature enough

A collection of research projects I'm working on, some of them will be promoted to a proper repo, some of them will most likely die.

## Current status
riscv-emu - simple RISC-V emulator written in C++, still work in progress

packer_win - personal research project on Win32/Win64 PE packers. Polymorphic first stage loader + polymorphic obfuscation based on x86 long instructions disalignment (i.e. jumping in the middle of a long x86 instruction and decoding it as something else); undetected by almost 100% of AV in 2013/2014, of course things changed and most of the techniques are not applicable anymore. x86/x64 code generation through asmjit; nasm required for build (second stage loader is written in assembly); win32 support almost production ready, win64 preliminary but working;

xarfcn_scan - SDR research project, scans for GMSK "looking" modulation over a 200 kHz bandwidth, using a simple power ratio approach, nothing particuarly complex or fool-proof, however it can detect available channels in my city (but of course they coudld be whatever GMSK-looking modulation); tested with Ettus B210
