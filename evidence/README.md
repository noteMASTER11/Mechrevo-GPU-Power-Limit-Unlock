# Successful-run evidence

| Record | What it demonstrates | What it does not demonstrate |
|---|---|---|
| [v6 first boot](v6-first-boot.json) | Private control sequence 0→1→2→4 succeeded, three ceilings and CURRENT reached225000 mW, later NVML enforced limit was225 W | Actual load power: the recorded power.draw sample is idle |
| [First FurMark run](furmark-first-run.json) | Owner ran FurMark on the RTX5080, log records 38.559 seconds at7680×4320, maximum59°C and normal shutdown | No power samples or artifact-count result were recorded in this log |
| Owner's confirmation | Owner explicitly reported that the225W experiment worked after testing | Not an independent electrical measurement or long-term qualification |

The [public package verification](public-package-verification.json) records a fresh source build, offline tests and local manifest preparation. It does not record a fresh installation or reboot of the repackaged tools.

These records supersede the project-level preboot status in the old v6 checkpoint. Historical `installed.json` and `final-check.json` remain unchanged because they record what was known before the first v6 boot.

There was a settling interval: the boot service's immediate NVML sample still showed175W while private CURRENT already read225000. Subsequent independent NVML queries showed225W repeatedly. A single immediate sample is therefore not sufficient to evaluate activation. Verify fresh readback after the service completes.

No benchmark is launched automatically by this project. Collect your own telemetry during a manually started test:

```bash
nvidia-smi --query-gpu=timestamp,enforced.power.limit,power.draw,temperature.gpu,utilization.gpu --format=csv -l 1 > gpu-telemetry.csv
```

Stop the logger with Ctrl+C when finished. Preserve timestamps, the loaded module marker and the exact test settings with any result you report.
