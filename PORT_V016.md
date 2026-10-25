# v0.16 – Original MYCL restart continuation

## Root cause corrected

`CC_OpenCL.c::load_mycel_state` restores capacity, active count and buffers via
`mycel_initialize` and `mycel_upload_all_state`. The previous C++ GpuBridge
invalidated its cached capacity/active count after a successful native load,
blocking subsequent `update()` and `nutrient()` calls.

The bridge now reads the already-validated 28-byte MYCL header, invokes the
original loader, and retains capacity and active count only on success.
On native-load failure it invalidates both fields. Its existing full-size
activity staging remains unchanged.

## Files

- `src/mycel_state_persistence.cpp`: restored bridge dimensions.
- `src/gpu_bridge.hpp`: read-only access to capacity/active count.
- `src/mycel_resume_probe.cpp`: manual two-process hardware save/resume probe.
- `tests/test_mycel_resume.cpp`: close/reopen/restore/step/readback with fake DLL.
- `tests/test_mycel_state_persistence.cpp`: obsolete test replaced with continuation expectation.
- `CMakeLists.txt`: test and manual probe targets.

## Build & checks

Linux C++20 Release: 16/16 CTest suites passed. No genuine AMD-GPU execution
or Windows build was possible in this environment; test on Windows 11 with
KR Compiler IDE, GCC 16.1.0 and the trusted CC_OpenCl.dll.

## Explicit Windows real-hardware run

Use PowerShell after rebuilding v0.16 (no preexisting test files):

```powershell
$B = 'H:\KR-Mycelia-Memory\KR_BUILD'
$Dll = 'H:\KR-Mycelia-Memory\CC_OpenCl.dll'
& "$B\mycelia_resume_tests.exe" "$B\libmycelia_test_gpu.dll"
$ExitCode = $LASTEXITCODE
Write-Host "Resume unit test exit code: $ExitCode"

$A = 'H:\KR-Mycelia-Memory\mycel_restart_A.mycl'
$C = 'H:\KR-Mycelia-Memory\mycel_restart_B.mycl'
# These files contain unencrypted simulation state. Do not run on private memory.
& "$B\mycelia_mycel_resume_probe.exe" --save $Dll 0 64 8 2 $A
Write-Host "Save process exit code: $LASTEXITCODE"
& "$B\mycelia_mycel_resume_probe.exe" --resume $Dll 0 $A 2 $C
Write-Host "Resume process exit code: $LASTEXITCODE"
```

These two `&` invocations are **separate Windows processes**; the second
initializes OpenCL again, restores the real MYCL state, performs two updates
and saves a second MYCL state. Do not proceed to the second invocation unless
the first exits successfully. The probe never overwrites an existing file.

`MYCL_CYCLE_PASS` reports a **diagnostic FNV-1a hash**, not cryptographic
integrity, and does not establish numerical parity to an uninterrupted run.
A full reproducible continuous-versus-restart comparison requires a controlled
RNG state and coverage of additional SubQG/VRAM buffers. `MYCL` is an original,
unencrypted subset of the global GPU state and must be treated as test data.

## Known security caveats

The native loader in the existing original CC_OpenCL library has unchecked
`fread`s. The bridge checks the MYCL dimensions and expected exact size before
calling it, but this is not a complete security audit. Never load untrusted
MYCL files. The path is converted to the ANSI codepage without lossy fallback.
The original saver creates plaintext `.mycl` files; do not use it with private
Windows memory. The test runner pins the DLL under Windows due to the
observed use-after-unload crash in v0.10; pinning is not a daemon lifecycle fix.

## Original untouched

The CMS-MyceliaDB-Enterprise source ZIP, its CC_OpenCL.c and CC_OpenCl.dll,
and the KR Compiler IDE are not modified.
