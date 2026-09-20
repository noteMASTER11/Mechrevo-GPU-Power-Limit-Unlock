# Historical v1 GSP read probe

v1 booted but metadata GET returned0x25 before any GSP copy. Hopper host WPR metadata was boot input and did not contain ACR-populated runtime offsets. No GPU memory read or write succeeded. The version-specific diagnostic patch and tests are retained as history; use v4/v5 for subsequent results.

The failed assumption was that gspFwWprStart/gspFwWprEnd and computed image/heap addresses were returned to the host metadata. v2 instead obtained actual WPR bounds via NVIDIA's architecture-specific register helper. No EEPROM writes, power changes or automatic boot-time commands were performed.
