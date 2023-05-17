# obsd-prom-exporter illumos edition

A small, basic Prometheus exporter for illumos hosts with minimal dependencies
and only C code.

As well as standalone operation, it can also be built as a CMON plugin for
Triton.

## Example usage

```bash
$ make
$ pfexec make install
$ pfexec svcadm enable prom-exporter
```

And in `prometheus.yml`:

```yaml
  - job_name: illumos
    metrics_path: /metrics
    scheme: http
    static_configs:
      - targets:
        - some.hostname:27600
```

## Metrics collected

 * ZFS pool capacity and alloc space
 * Thread and process counts by zone
 * vminfo (total swappable memory util)
 * ZFS arcstats
 * Kernel NFS server calls
 * CPU time
 * Network interface errors, bytes, packets, qdrops
 * VFS busy time per zone and busy-qlen product per zone
 * iostat ops/bytes/wait/busy
