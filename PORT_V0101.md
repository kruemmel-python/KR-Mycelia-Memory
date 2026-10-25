# KR Mycelia Native v0.10.1 – Windows DLL unloading workaround

Windows Event ID 1000 reports `CC_OpenCl.dll_unloaded` with exception `0xC0000005` after the real GPU run had already reported completed cycles and successful encrypted snapshot round-trip. This is evidence of execution after unload; the precise callback/destructor responsible is **not identified**.

## Change

In the **real hardware run only**, the exact loaded module is pinned using `GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, ...)` before GPU initialization. `GpuBridge::close()` still invokes `shutdown_gpu()` and `FreeLibrary`, but Windows keeps the pinned image loaded until process termination. No change is made to the original DLL or the KR compiler. `--probe` remains a normal load/unload test. The simulated GPU ABI tests also keep normal load/unload semantics.

This is a **mitigation**, not a confirmed root-cause fix. Process-lifetime pinning is irreversible in Windows and is suitable only for the isolated GPU worker/process, not for repeated dynamic hot swapping of the DLL. It cannot cure prior heap corruption, racing threads, or unsafe teardown in dependent OpenCL DLLs.

## Tests

- Existing Linux test matrix remains unchanged.
- On Windows, new `mycelia_gpu_pin_tests.exe` verifies that the module stays loaded after bridge cleanup.
- Re-run real GPU probe with a **new** `.kms` output filename and check exit code explicitly.
- If it still crashes, collect dump/backtrace and audit original DLL shutdown and callbacks; do not ignore the crash.

## Windows commands

```powershell
$B = 'H:\KR-Mycelia-Memory\KR_BUILD'
& "$B\mycelia_gpu_pin_tests.exe" "$B\libmycelia_test_gpu.dll"
$Dll = 'H:\KR-Mycelia-Memory\CC_OpenCl.dll'
$Snapshot = 'H:\KR-Mycelia-Memory\real_gpu_smoke_v0101.kms'
& "$B\mycelia_real_gpu_probe.exe" --run $Dll 0 64 8 2 $Snapshot
"Exitcode: $LASTEXITCODE"
```

**Do not overwrite an existing .kms file.** Save any user data before testing. The real DLL must be trusted.
