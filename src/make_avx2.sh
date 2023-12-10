arch_cpu=x86-64-avx2
make --no-print-directory -j profile-build ARCH=${arch_cpu} COMP=mingw
strip fire.exe
mv fire.exe fire-9.2_x64_avx2.exe 
make gcc-profile-clean
