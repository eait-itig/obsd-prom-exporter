# obsd-prom-exporter

A small, basic Prometheus exporter for OpenBSD hosts with minimal dependencies
and only C code.

## Example usage

```bash
$ make
$ doas make installids
$ doas make install
$ doas rcctl enable promexporter
```

And in `prometheus.yml`:

```yaml
  - job_name: obsd
    metrics_path: /metrics
    scheme: http
    static_configs:
      - targets:
        - some.hostname:27600
```

## Metrics collected

 * CPU time usage (per-core, user/nice/sys/spin/intr/idle)
 * Disk I/O operations (read/write), bytes (read/write), busy time (read/write)
 * Network interface packets, bytes, errors, qdrops (in/out)
 * UVM system-wide memory usage (free/active/inactive/total)
 * PF states, state ops, src nodes, limit hits, overload hits, drops (reason)
 * System total files open (current/max), processes running (current/max), thread running (current/max)
 * Kernel memory pool item sizes, allocations, gets/puts/fails, pages, idle
