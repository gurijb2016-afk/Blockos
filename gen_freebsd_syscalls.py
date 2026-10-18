#!/usr/bin/env python3
"""Generate a lossless, reviewable syscall inventory from FreeBSD syscalls.master.

This deliberately does NOT assign FreeBSD syscall numbers to BlockOS's native
Linux-like ABI and does not claim to implement kernel handlers. It preserves
source entries and metadata so a later FreeBSD-ABI personality can wire only
verified handlers.
"""
from __future__ import annotations
import argparse, json, re
from pathlib import Path

ENTRY = re.compile(r'^\s*(\d+)\s+(\S+)\s+(.*?)\s*$', re.M)

def parse_master(text: str):
    lines = text.splitlines()
    entries = []
    i = 0
    while i < len(lines):
        m = re.match(r'^\s*(\d+)\s+(\S+)\s+(.*)$', lines[i])
        if not m:
            i += 1
            continue
        number, audit, rest = int(m.group(1)), m.group(2), m.group(3)
        raw = [lines[i]]
        # Prototype entries can span lines and contain braces. Collect until
        # braces balance; comments/UNIMPL/OBSOL entries usually have no block.
        balance = rest.count('{') - rest.count('}')
        if balance > 0:
            i += 1
            while i < len(lines) and balance > 0:
                raw.append(lines[i])
                balance += lines[i].count('{') - lines[i].count('}')
                i += 1
        else:
            i += 1
        raw_text = '\n'.join(raw)
        type_match = re.search(r'\b(STD|OBSOL|RESERVED|UNIMPL|SYSMUX|COMPAT\d*|NODEF|NOARGS|NOPROTO|NOSTD)(?:\|[A-Z0-9_]+)*\b', rest)
        syscall_type = type_match.group(0) if type_match else 'UNKNOWN'
        after_type = rest[type_match.end():].strip() if type_match else rest.strip()
        # For prototypes, function name is the identifier immediately before
        # the first opening parenthesis. For placeholder entries, take first token.
        prototype = re.search(r'([A-Za-z_][A-Za-z0-9_]*)\s*\(', raw_text)
        if prototype and '{' in raw_text:
            name = prototype.group(1)
        else:
            cleaned = re.sub(r'\{.*', '', after_type).strip()
            name = cleaned.split()[0] if cleaned else 'UNKNOWN'
        entries.append({
            'number': number, 'audit': audit, 'type': syscall_type,
            'name': name, 'raw': raw_text
        })
    return entries

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('master', nargs='?', default='kernel/syscall/freebsd_syscalls.master')
    ap.add_argument('-o', '--output', default='kernel/syscall/freebsd_syscalls_catalog.json')
    args = ap.parse_args()
    src = Path(args.master)
    entries = parse_master(src.read_text(encoding='utf-8'))
    nums = [e['number'] for e in entries]
    if not entries:
        raise SystemExit('ERROR: no syscall entries parsed')
    if nums != sorted(nums):
        raise SystemExit('ERROR: syscall numbers are not in ascending source order')
    if len(nums) != len(set(nums)):
        raise SystemExit('ERROR: duplicate syscall number')
    Path(args.output).write_text(json.dumps({
        'source': src.name,
        'entry_count': len(entries),
        'first_number': nums[0],
        'last_number': nums[-1],
        'warning': 'Inventory only; not a claim of implemented BlockOS handlers.',
        'syscalls': entries,
    }, indent=2) + '\n', encoding='utf-8')
    print(f'OK: generated {len(entries)} entries ({nums[0]}..{nums[-1]}) -> {args.output}')

if __name__ == '__main__':
    main()
