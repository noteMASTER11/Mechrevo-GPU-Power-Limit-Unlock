savedcmd_unbound_wpr_probe.mod := printf '%s\n'   unbound_wpr_probe.o | awk '!x[$$0]++ { print("./"$$0) }' > unbound_wpr_probe.mod
