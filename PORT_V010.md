# v0.10 – Real-CC_OpenCL hardware verification entrypoint

This release adds `mycelia_real_gpu_probe`, which uses the existing C++20 `GpuBridge` and `RuntimeCycle` without implementing replacement simulation algorithms.

The original binary in the uploaded CMS archive is `Mycelia_Database-main/build/CC_OpenCl.dll` (Windows x64). Its PE export directory was examined with `llvm-objdump`: it contains `initialize_gpu`, `shutdown_gpu`, `subqg_init_mycel`, `subqg_set_active_T`, `step_mycel_update`, `read_nutrient`, and `cc_get_last_error`. This static export audit is **not** a hardware run. The binary is not redistributed here.

Run on Windows after building with the KR Compiler IDE (adapt output directory):

```powershell
$Exe = 'H:\KR-Mycelia-Memory\KR_BUILD\mycelia_real_gpu_probe.exe'
$Dll = 'H:\PATH\TO\TRUSTED\CC_OpenCl.dll'
& $Exe --probe $Dll
# Only after probe succeeds; choose an unused .kms filename:
& $Exe --run $Dll 0 64 8 2 'H:\KR-Mycelia-Memory\real_gpu_smoke.kms'
Write-Host "Exitcode: $LASTEXITCODE"
```

`--probe` checks dynamic loading and the seven exports without starting OpenCL. `--run` explicitly initializes GPU index 0, allocates a Mycel substrate, updates nutrients twice, passes through Observer/DAD and writes/roundtrips a DPAPI-encrypted snapshot. Existing .kms files are refused. Both commands fail on errors. The run path requires real OpenCL support, and a successful run is the only basis for a true hardware-integration claim.

Limitations: energy and pheromone fields are controlled test vectors, not the full original MyceliaWorld state. GPU VRAM state restoration, full enterprise port, and the Windows context capture agent remain outstanding. GPU device selection is a CC_OpenCL index, not a guarantee of AMD device identity.

The original enterprise ZIP and all historical version folders remain untouched.
