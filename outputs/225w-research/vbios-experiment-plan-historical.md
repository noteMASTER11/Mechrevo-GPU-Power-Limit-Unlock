# Abandoned VBIOS identity experiment

Historical plan: pass an unmodified copy of the native VBIOS through the open kernel module into GSP, verify identity behavior, then consider changed source tables. Initial target225W. This was an in-memory override, not EEPROM flashing.

The control experiment caused severe display artifacts/initialization trouble. The user returned to stock boot; the managed identity entry was removed. This path was abandoned. Do not deploy historical identity patches. The later successful route changes validated live GSP heap fields and leaves VBIOS unchanged.
