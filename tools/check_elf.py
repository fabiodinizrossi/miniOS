#!/usr/bin/env python3
"""Verifica o contrato de carregamento; nao substitui o teste de boot."""
import struct
import subprocess
import sys
from pathlib import Path
p = Path(sys.argv[1])
b = p.read_bytes()
assert b[:4] == b'\x7fELF' and b[4:6] == b'\x01\x01', 'Esperado ELF32 little endian'
assert struct.unpack_from('<H', b, 18)[0] == 3, 'Esperado EM_386'
found = None
for off in range(0, min(len(b), 8192) - 11, 4):
    magic, flags, checksum = struct.unpack_from('<III', b, off)
    if magic == 0x1BADB002 and (magic + flags + checksum) & 0xFFFFFFFF == 0:
        found = off
        break
assert found is not None, 'Cabecalho Multiboot ausente ou invalido'
undefined = subprocess.check_output(['nm', '-u', str(p)], text=True).strip()
assert not undefined, f'Simbolos indefinidos: {undefined}'
print(f'OK: ELF32 i386, Multiboot no offset {found}, sem simbolos indefinidos.')
