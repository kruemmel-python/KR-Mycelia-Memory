# v0.17 – MYCL restart-parity experiment

## Scope

This release adds an exact byte comparison of the original CC_OpenCL MYCL v1 state after either (a) four uninterrupted GPU steps or (b) two steps, a process restart, and two further steps. The original CC_OpenCL.c/DLL remains unmodified. The comparison is not a full-device-VRAM equivalence test: the original MYCL format captures the subset documented in v0.15 and may omit global random-number generators and other OpenCL buffers.

## Files

- `src/mycel_resume_probe.cpp`: adds `--continuous DLL GPU CAPACITY ACTIVE STEPS NEW_MYCL` mode, in addition to `--save` and `--resume`.
- `src/mycl_compare.hpp/.cpp`: validates the two input MYCL files and performs a strict bytewise comparison, including length and first mismatched offset, and diagnostic FNV-1a 64-bit checksums. Hashes are not cryptographic authenticity proofs.
- `src/mycl_compare_main.cpp`: native comparison CLI.
- `tests/test_mycl_compare.cpp`: equality, one-byte mutation detection, and truncated-input rejection using the isolated fake GPU DLL.

## Results and limits

All 17 Linux/Release CTest cases passed. No real AMD GPU was available in the test environment. Under Windows, the new test and real reference-versus-restart comparison remain to be run. A mismatch is a research finding, not proof of a faulty snapshot: non-deterministic kernels, omitted state, or uninitialized memory may cause divergence. Conversely, identical MYCL bytes do not prove global GPU state equivalence.

## Manual hardware test (Windows PowerShell)

Build the full source tree with KR Compiler IDE; do not run against files left by previous builds. Use fresh paths; MYCL outputs are plaintext test data and are never overwritten.

```powershell
$B = 'H:\KR-Mycelia-Memory\KR_BUILD'
$Dll = 'H:\KR-Mycelia-Memory\CC_OpenCl.dll'
$Prefix = 'H:\KR-Mycelia-Memory\parity_' + (Get-Date -Format 'yyyyMMdd_HHmmss')
$A = "${Prefix}_after2.mycl"
$Resumed = "${Prefix}_resumed4.mycl"
$Continuous = "${Prefix}_continuous4.mycl"

& "$B\mycelia_mycl_compare_tests.exe" "$B\libmycelia_test_gpu.dll"
if ($LASTEXITCODE -ne 0) { throw 'Comparator regression failed' }

& "$B\mycelia_mycel_resume_probe.exe" --save $Dll 0 64 8 2 $A
if ($LASTEXITCODE -ne 0) { throw 'Save run failed' }

& "$B\mycelia_mycel_resume_probe.exe" --resume $Dll 0 $A 2 $Resumed
if ($LASTEXITCODE -ne 0) { throw 'Resume run failed' }

& "$B\mycelia_mycel_resume_probe.exe" --continuous $Dll 0 64 8 4 $Continuous
if ($LASTEXITCODE -ne 0) { throw 'Continuous run failed' }

& "$B\mycelia_mycl_compare.exe" $Continuous $Resumed
$CompareExit = $LASTEXITCODE
Write-Host "Comparison exit code: $CompareExit"
# 0 = byte-identical MYCL, 3 = valid but different, 1 = malformed/unreadable, 2 = usage error
```

The comparison is intentionally *not* part of automated CTest: hardware access is explicit, and the MYCL files contain plaintext simulation states.
