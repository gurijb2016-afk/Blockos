# Patch hook

No speculative source-code rewrite is included here. Pango, HarfBuzz and
FreeType are mature upstream projects; BlockOS-specific changes should be
added only from actual compiler/runtime failures observed with the BlockOS
libc/loader.

Recommended workflow:
1. build the unmodified pinned release;
2. record the first compiler/linker error;
3. add one minimal patch under this directory;
4. rebuild;
5. run the smoke test under BlockOS.
