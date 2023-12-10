arch_cpu=x86-64-pext
make --no-print-directory -j profile-build ARCH=${arch_cpu} COMP=mingw
strip fire.exe
mv fire.exe fire-9.3_x64_pext.exe 
make gcc-profile-clean
