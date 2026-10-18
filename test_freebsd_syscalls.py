#!/usr/bin/env python3
import json, subprocess, sys, tempfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
MASTER = ROOT / 'kernel/syscall/freebsd_syscalls.master'
GEN = ROOT / 'tools/gen_freebsd_syscalls.py'
with tempfile.TemporaryDirectory() as td:
    out = Path(td) / 'catalog.json'
    subprocess.run([sys.executable, str(GEN), str(MASTER), '-o', str(out)], check=True)
    data = json.loads(out.read_text())
    entries = data['syscalls']
    assert data['entry_count'] == len(entries) > 500, data['entry_count']
    nums = [x['number'] for x in entries]
    assert nums == sorted(nums)
    assert len(nums) == len(set(nums))
    assert nums[0] == 0 and nums[-1] == 605
    by_num = {x['number']: x for x in entries}
    assert by_num[3]['name'] == 'read', by_num[3]
    assert by_num[4]['name'] == 'write', by_num[4]
    assert by_num[59]['name'] == 'execve', by_num[59]
    assert 'COMPAT' in by_num[318]['type'], by_num[318]
    assert by_num[313]['type'] == 'OBSOL', by_num[313]
    print(f'PASS: {len(entries)} syscall records; order, uniqueness, prototype names, markers')
