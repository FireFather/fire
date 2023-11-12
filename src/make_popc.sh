#!/bin/bash
# makefire_popc.sh

# Fire is a freeware UCI chess playing engine authored by Norman Schmidt.
#  
# Fire is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or any later version.
# 
# You should have received a copy of the GNU General Public License with
# this program: copying.txt.  If not, see <http://www.gnu.org/licenses/>.

# cpu architecture set to x86 64-bit w/ Intel popcount instruction

arch_cpu=x86-64-popc
make --no-print-directory -j profile-build ARCH=${arch_cpu} COMP=mingw
strip fire.exe
mv fire.exe fire-9.2_x64_popc.exe
make clean 
