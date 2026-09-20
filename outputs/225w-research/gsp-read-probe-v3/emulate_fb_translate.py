"""Run original RISC-V address translator offline with observed PA ranges.

Virtual bases are synthetic, because the log gives PA and length only.
This verifies range selection, NOT real GSP access permissions or a DMA copy.
Requires work/windows-emulation-venv/bin/python (Unicorn).
"""
from pathlib import Path
import hashlib
import json
import struct
from unicorn import Uc, UC_ARCH_RISCV, UC_MODE_RISCV64, UC_HOOK_CODE
from unicorn.riscv_const import UC_RISCV_REG_A0, UC_RISCV_REG_A1, UC_RISCV_REG_A2, UC_RISCV_REG_A3, UC_RISCV_REG_SP, UC_RISCV_REG_RA, UC_RISCV_REG_PC
from heap_evidence import extract

root = Path(__file__).resolve().parents[3]
elf = (root / 'work/linux-probe/gsp-rm.elf').read_bytes()
evidence = extract((root / 'outputs/225w-research/gsp-read-probe-v2/live-20260920/nvlog.bin').read_bytes())
heap, size = [int(x, 16) for x in evidence['regions']['rm_heap']['arguments']]
results = []
for name, pa, length, expected in [
    ('v2_wpr_prefix', 0x3ef020000, 256, 0x5b),
    ('v3_heap_start', heap, 256, 0),
    ('heap_end_valid', heap + size - 256, 256, 0),
    ('heap_end_crossing', heap + size - 128, 256, 0x5b),
]:
    u = Uc(UC_ARCH_RISCV, UC_MODE_RISCV64)
    phoff = struct.unpack_from('<Q', elf, 32)[0]
    for i in range(struct.unpack_from('<H', elf, 56)[0]):
        typ, flags, off, va, _, filesz, memsz, _ = struct.unpack_from('<IIQQQQQQ', elf, phoff + 56 * i)
        if typ == 1:
            u.mem_map(va & ~4095, (memsz + (va & 4095) + 4095) & ~4095)
            u.mem_write(va, elf[off:off + filesz])
    u.mem_map(0x6000000, 0x10000)
    u.mem_map(0x7000000, 0x2000)
    table = 0x4194810
    # Cached triples: VA, PA, size. The source PAs/sizes are real; VAs aren't.
    for offset, vabase, base, count in [
        (0x40, 0x60000000, 0, 0x3eed20000),
        (0xd0, 0x50000000, heap, size),
        (0xa0, 0x70000000, 0x3eed20000, 0x300000),
    ]:
        u.mem_write(table + offset, struct.pack('<QQQ', vabase, base, count))
    for reg, value in [(UC_RISCV_REG_A0, pa), (UC_RISCV_REG_A1, length),
                       (UC_RISCV_REG_A2, 0x7000000), (UC_RISCV_REG_A3, 0),
                       (UC_RISCV_REG_SP, 0x600f000), (UC_RISCV_REG_RA, 0x7001000)]:
        u.reg_write(reg, value)
    trace = []
    def hook(uc, address, insn_size, user):
        trace.append(address)
        if address == 0x1ad724c:  # Logging only; no translation performed here.
            uc.reg_write(UC_RISCV_REG_PC, uc.reg_read(UC_RISCV_REG_RA))
    u.hook_add(UC_HOOK_CODE, hook)
    u.emu_start(0x1b7c694, 0x7001000, count=1000)
    status = u.reg_read(UC_RISCV_REG_A0)
    assert u.reg_read(UC_RISCV_REG_PC) == 0x7001000
    assert status == expected, (name, status)
    target = struct.unpack('<Q', u.mem_read(0x7000000, 8))[0]
    if status == 0:
        assert target == 0x50000000 + pa - heap
    results.append({'case': name, 'source_pa': hex(pa), 'status': hex(status),
                    'synthetic_target_va': hex(target), 'instructions': len(trace)})
print(json.dumps({'scope': __doc__, 'elf_sha256': hashlib.sha256(elf).hexdigest(), 'cases': results}, indent=2))
