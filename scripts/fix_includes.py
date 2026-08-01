#!/usr/bin/env python3
# scripts/fix_includes.py
# Usage: python3 scripts/fix_includes.py [--apply]
import os, re, argparse

parser = argparse.ArgumentParser()
parser.add_argument('--apply', action='store_true', help='Apply changes (otherwise dry-run)')
args = parser.parse_args()

ROOT = '.'
include_dirs = ['.', 'kernel', 'arch/86_64x', 'drivers', 'fs', 'examples', 'include']

inc_re = re.compile(r'^\s*#\s*include\s*"([^"]+)"')
exts = ('.c','.cpp','.cc','.h','.hpp','.S')

def find_header(name):
    for d in include_dirs:
        p = os.path.join(ROOT, d, name)
        if os.path.exists(p):
            return os.path.join(d, name)
    # try recursive search for exact filename
    for d in include_dirs:
        for root,_,files in os.walk(os.path.join(ROOT,d)):
            if name in files:
                rel = os.path.relpath(os.path.join(root,name), ROOT)
                return rel.replace('\\','/')
    return None

changes = []
for dirpath,_,files in os.walk(ROOT):
    # skip .git, build, node_modules
    if any(x in dirpath for x in ['.git', 'build', 'node_modules']):
        continue
    for fn in files:
        if fn.endswith(exts):
            path = os.path.join(dirpath, fn)
            with open(path, 'r', encoding='utf-8', errors='ignore') as f:
                lines = f.readlines()
            new_lines = []
            modified = False
            for ln in lines:
                m = inc_re.match(ln)
                if m:
                    name = m.group(1)
                    # if header exists as given relative to file, keep
                    local_path = os.path.normpath(os.path.join(dirpath, name))
                    if os.path.exists(local_path):
                        new_lines.append(ln)
                        continue
                    found = find_header(name)
                    if found and found != name:
                        new_ln = re.sub(r'"[^"]+"', f'"{found}"', ln, count=1)
                        new_lines.append(new_ln)
                        modified = True
                        changes.append((path, ln.strip(), new_ln.strip()))
                        continue
                new_lines.append(ln)
            if modified and args.apply:
                with open(path, 'w', encoding='utf-8') as f:
                    f.writelines(new_lines)

print('Dry-run results (use --apply to write files):' if not args.apply else 'Applied changes:')
for p,old,new in changes:
    print(p, ' : ', old, ' -> ', new)
print('Total changes:', len(changes))
