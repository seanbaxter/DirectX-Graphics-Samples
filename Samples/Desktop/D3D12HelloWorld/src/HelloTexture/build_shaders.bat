wsl circle -shader -emit-dxil -c -E vert_main -o vert.dxil shaders.cxx
dxil_signage.exe vert.dxil
wsl circle -shader -emit-dxil -c -E frag_main -o frag.dxil shaders.cxx
dxil_signage.exe frag.dxil