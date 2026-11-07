# Contributing

Crossweb is not production-ready. Read `docs/READINESS.md` before adding a platform or a plugin.

From the repository root:

```sh
bash scripts/check.sh
```

That command compiles `tests/test_core.c`, checks the deny-by-default grant file, bootstraps `nob`, and links the Linux native runtime. It does not launch `./build/crossweb`.

Native commands stay denied unless `crossweb.permissions` lists them. Do not add a default grant for filesystem, keystore, or any other privileged command.
