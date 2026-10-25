# v0.14 – Native GPU observation persistence

## Verified scope

- Existing v0.13 original bridge + observer/world/DAD checkpoint retained.
- `GpuObservationCodec::save_secure` and `load_secure`: Windows current-user DPAPI, fail closed elsewhere.
- Versioned `KRGPUENC1` encrypted disk envelope.
- No plaintext temporary files; Windows write-through temp-then-rename; refuses existing final path.
- Round-trip includes observed GPU activity/nutrients + CPU world + DAD.
- Deliberate corruption and overwrite regression tests.

## Honest limitations

- This is NOT GPU VRAM restore. The actual original CC_OpenCL ABI does not expose full internal buffer export/import through the existing bridge.
- Original DLL is not modified or repackaged. GPU observation checkpoint is NOT a drop-in replacement for the original simulation's internal state.
- The on-disk SHA-256 is an unkeyed corruption check; DPAPI handles payload protection. Windows same-user malicious writers are outside this format's security guarantee.
- No cross-user portability, concurrent writer coordination or power-loss-atomic directory metadata guarantee.
- Production background service and full Enterprise port are not complete.

## Validation

`ctest --test-dir build --output-on-failure` (Linux); Windows DPAPI test must execute on Windows.

Windows test: `mycelia_gpu_secure_checkpoint_tests.exe`.
