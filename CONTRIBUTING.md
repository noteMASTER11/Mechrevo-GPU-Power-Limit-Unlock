# Contributing

Start with the Wiki and the supported-target table. Keep documentation and public tool output in English.

For a new hardware profile, provide PCI/subsystem IDs, driver and firmware identity, kernel/build information, stock limits and a stock recovery procedure. Do not submit a patch that merely removes version, object-layout, expected-value or retry guards.

Separate observed values from inference. A changed MAX is not a changed CURRENT; a changed CURRENT is not by itself measured load power. Report the test workload, elapsed time, sampled power and temperature, and whether suspend/reset behavior was tested.

Keep changes bounded and test failure paths, especially partial writes, failed readback, once-per-boot state and release ordering. Never run live GPU writes in automated CI. Raw heaps, NVLOG archives, authentication material, machine journals and proprietary firmware must stay out of commits.

Detailed documentation belongs in the GitHub Wiki. Keep the main branch limited to maintained source, build tooling, tests, service definitions, licenses, and the short project README.
