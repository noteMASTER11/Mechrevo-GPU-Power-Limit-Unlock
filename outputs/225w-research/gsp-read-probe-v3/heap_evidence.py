"""Decode exact GSP 615.71.09 startup records. No GPU/device access.

The firmware logging call encodes its metadata id in the final qword.
For these records, argument qwords immediately precede that marker;
one additional preceding qword contains the timestamp. Duplicate NVLOG
buffers are expected, but all copies must agree on the recorded values.
"""
from pathlib import Path
import hashlib
import json
import struct
import sys

SPECS = [
    # name, metadata AUIPC PC, ADDI, arg count, descriptor offset
    ('ordinary_fb', 0x1afaccc, 0x644, 3, 0x48),
    ('rm_heap', 0x1afacf8, 0x5e8, 2, 0xd8),
    ('non_wpr', 0x1afad0e, 0x5ba, 2, 0xa8),
    ('allocator_heap_size', 0x1afad5e, 0x53a, 1, 0xf8),
]

def extract(data):
    result = {'nvlog_sha256': hashlib.sha256(data).hexdigest(), 'regions': {}}
    for name, pc, delta, argc, field in SPECS:
        metadata = pc + 0x1e8bd000 + delta
        marker = 0x0300000000000000 | ((argc + 2) << 48) | (metadata & 0xffffff)
        key = struct.pack('<Q', marker)
        records = []
        cursor = 0
        while True:
            cursor = data.find(key, cursor)
            if cursor < 0:
                break
            if cursor < 8 * (argc + 1):
                raise ValueError('Truncated record: ' + name)
            args = struct.unpack_from('<' + 'Q' * argc, data, cursor - 8 * argc)
            records.append({'offset': hex(cursor), 'arguments': [hex(x) for x in args]})
            cursor += 8
        if not records:
            raise ValueError('Missing startup record: ' + name)
        if any(r['arguments'] != records[0]['arguments'] for r in records):
            raise ValueError('Conflicting startup records: ' + name)
        result['regions'][name] = {
            'firmware_pc': hex(pc), 'descriptor_offset': hex(field),
            'marker': hex(marker), 'arguments': records[0]['arguments'], 'copies': records,
        }
    heap = [int(x, 16) for x in result['regions']['rm_heap']['arguments']]
    if heap != [0x3ef024000, 0x73dc000]:
        raise ValueError('RM heap differs from fixed v3 target: ' + repr(heap))
    size = result['regions']['allocator_heap_size']['arguments']
    if size != ['0x73dc000']:
        raise ValueError('Allocator heap size differs')
    result['fixed_probe_target'] = hex(heap[0])
    result['heap_end_exclusive'] = hex(sum(heap))
    result['wpr_prefix_excluded_bytes'] = 0x4000
    result['hardware_read_success_proven'] = False
    return result

if __name__ == '__main__':
    print(json.dumps(extract(Path(sys.argv[1]).read_bytes()), indent=2))
