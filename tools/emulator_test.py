#!/usr/bin/env python3
"""Teste opcional em Unicorn: pip install unicorn. Nao substitui QEMU/GRUB.
O hardware de E/S e simulado por callbacks, nao por um chipset completo.
"""
from collections import deque
from pathlib import Path
import struct
import sys
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_INSN, UC_HOOK_CODE
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_INS_IN, UC_X86_INS_OUT,
)

def run(path, serial_input=b'', keyboard=()):
    image = Path(path).read_bytes()
    uc = Uc(UC_ARCH_X86, UC_MODE_32)
    uc.mem_map(0, 16 * 1024 * 1024)
    entry, phoff = struct.unpack_from('<II', image, 24)
    phentsize, phnum = struct.unpack_from('<HH', image, 42)
    for i in range(phnum):
        typ, off, va, _, filesz, _, _, _ = struct.unpack_from('<8I', image, phoff + i * phentsize)
        if typ == 1 and filesz:
            uc.mem_write(va, image[off:off + filesz])
    uc.mem_write(0x8000, struct.pack('<III', 1, 640, 64512))
    uc.reg_write(UC_X86_REG_EAX, 0x2BADB002)
    uc.reg_write(UC_X86_REG_EBX, 0x8000)
    serial = deque(serial_input)
    keys = deque(keyboard)
    output = bytearray()
    state = {'ready': False, 'halted': False}
    def inp(cpu, port, size, data):
        if port == 0x3FD:
            return 0x20 | int(state['ready'] and bool(serial))
        if port == 0x3F8:
            return serial.popleft() if serial else 0
        if port == 0x64:
            return int(state['ready'] and bool(keys))
        if port == 0x60:
            return keys.popleft() if keys else 0
        return 0
    def out(cpu, port, size, value, data):
        if port == 0x3F8:
            # Ignorar divisor de baud inicial (0x03) configurado antes do texto.
            if value != 3:
                output.append(value & 255)
            if output.endswith(b'minios> '):
                state['ready'] = True
    def instruction(cpu, address, size, data):
        if bytes(cpu.mem_read(address, 1)) == b'\xf4':
            state['halted'] = True
            cpu.emu_stop()
    uc.hook_add(UC_HOOK_INSN, inp, None, 1, 0, UC_X86_INS_IN)
    uc.hook_add(UC_HOOK_INSN, out, None, 1, 0, UC_X86_INS_OUT)
    uc.hook_add(UC_HOOK_CODE, instruction)
    uc.emu_start(entry, 0, count=10000000)
    assert state['halted'], 'CPU nao chegou a halt'
    assert not serial and not keys, 'Entrada nao foi totalmente consumida'
    return output.decode('ascii', errors='replace'), uc

path = sys.argv[1] if len(sys.argv) > 1 else 'build/kernel.elf'
commands = (b'help\ninfo\necho teste\necho ac\x08b\ncolor 2\ncolor 99\n'
            b'invalido\n\n' + b'x' * 150 + b'\n' + b'echo scroll\n' * 30 + b'clear\nhalt\n')
text, cpu = run(path, serial_input=commands)
for expected in ('MiniOS didatico', 'Memoria convencional (KiB): 640',
                 '64512', 'teste\r\n', 'Cor alterada.',
                 'Comando desconhecido.', 'CPU parada.'):
    assert expected in text, expected
assert 'x' * 127 in text and 'x' * 128 not in text, 'Limite de linha incorreto'
assert text.count('Comando desconhecido.') == 3
assert 'c\b \bb' in text, 'Backspace nao exercitado'
# PS/2: echo A, echo B e halt. Shift esquerdo e Caps Lock.
keys = [18, 46, 35, 24, 57, 42, 30, 0xAA, 28,
        18, 46, 35, 24, 57, 58, 48, 58, 28,
        35, 30, 38, 20, 28]
text2, _ = run(path, keyboard=keys)
assert 'echo A\r\nA\r\n' in text2
assert 'echo B\r\nB\r\n' in text2
assert 'CPU parada.' in text2
print('OK: entrada Assembly, GDT, pilha, C, VGA e serial em CPU x86 emulada.')
print('OK: comandos, memoria, linha vazia, erros, buffer, Backspace e rolagem.')
print('OK: entrada PS/2 simulada, Shift e Caps Lock.')
print('Pendente: firmware, GRUB, ISO e dispositivos reais em QEMU.')
