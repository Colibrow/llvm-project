# REQUIRES: aarch64

# RUN: llvm-mc -filetype=obj -triple=aarch64 %s -o %t
# RUN: echo "A B 5" > %t.call_graph
# RUN: echo "B C 50" >> %t.call_graph
# RUN: echo "C D 40" >> %t.call_graph
# RUN: echo "D B 10" >> %t.call_graph
# RUN: not ld.lld -o /dev/null --irpgo-profile-sort %s --call-graph-ordering-file=%t.call_graph 2>&1 | FileCheck %s --check-prefix=IRPGO-ERR
# RUN: not ld.lld -o /dev/null --irpgo-profile-sort=%s --call-graph-ordering-file=%t.call_graph 2>&1 | FileCheck %s --check-prefix=IRPGO-ERR
# IRPGO-ERR: --irpgo-profile-sort is incompatible with --call-graph-ordering-file

# RUN: not ld.lld -o /dev/null --compression-sort=function --call-graph-ordering-file %t.call_graph 2>&1 | FileCheck %s --check-prefix=COMPRESSION-ERR
# COMPRESSION-ERR: --compression-sort= is incompatible with --call-graph-ordering-file

# RUN: not ld.lld -o /dev/null --compression-sort=malformed 2>&1 | FileCheck %s --check-prefix=COMPRESSION-MALFORM
# COMPRESSION-MALFORM: unknown value `malformed` for --compression-sort=

# RUN: not ld.lld -o /dev/null --compression-sort-startup-functions 2>&1 | FileCheck %s --check-prefix=STARTUP
# STARTUP: --compression-sort-startup-functions must be used with --irpgo-profile-sort
