# MatterThreads CLI Reference

## Usage

```
matterthreads [options] [scenario]
```

## Options

| Flag | Description | Default |
|------|-------------|---------|
| `--topology <preset>` | Topology preset: `full`, `linear`, `star` | `full` |
| `--seed <uint32>` | Random seed for reproducibility | random |
| `--duration <seconds>` | Max simulation duration | 120 |
| `--hw` | Enable hardware bridge mode | off |
| `--verbose` | Verbose logging | off |
| `--output <path>` | Write report to file (JSON) | stdout |
| `--scenario <name>` | Run a named scenario and exit | interactive |

## Interactive Commands

### Status & Inspection
```
status                          Show all node states, roles, routes
topology                        Show current link quality matrix
metrics                         Show current metrics summary
timeline [from] [to]            Show event timeline
dashboard                       Live ANSI dashboard (refreshes every 1s)
```

### Link Manipulation
```
link <A> <B> <loss%>            Set packet loss on link A->B
link <A> <B> down               Bring link down (100% loss)
link <A> <B> up                 Restore link
latency <A> <B> <ms>            Set latency on link A->B
```

### Node Control
```
crash <node>                    Kill node process (SIGKILL)
restart <node>                  Restart node process
freeze <node> <seconds>         Pause node for N seconds (SIGSTOP/SIGCONT)
```

### Matter Operations
```
commission <src> <dst>          Start PASE commissioning from src to dst
read <src> <dst> <ep/cluster/attr>     Read an attribute
write <src> <dst> <ep/cluster/attr> <value>  Write an attribute
subscribe <src> <dst> <ep/cluster/attr> <min_s> <max_s>  Start subscription
invoke <src> <dst> <ep/cluster/cmd>    Invoke a command
```

### Fault Injection
```
inject <fault-json>             Inject a fault rule from JSON
chaos on                        Enable random fault injection
chaos off                       Disable random fault injection
```

### Export
```
export <path>                   Export full report to JSON file
quit                            Shut down simulation
```

## Predefined Scenarios

Run with `--scenario <name>`:

| Name | Description |
|------|-------------|
| `mesh-healing` | Linear chain, kill middle node, measure healing time |
| `subscription-recovery` | Subscribe, drop link for 90s, measure recovery |
| `commissioning-loss` | Commission under 10/20/30/50% packet loss |
| `route-failure` | Send 100 commands, degrade direct link to 80% loss |
| `message-ordering` | Rapid-fire commands with jitter + duplication |

## Examples

```bash
# Run mesh healing scenario with seed for reproducibility
matterthreads --scenario mesh-healing --seed 42

# Interactive session with linear topology
matterthreads --topology linear

# Run for 60s with verbose logging and JSON output
matterthreads --duration 60 --verbose --output report.json

# Hardware bridge mode
matterthreads --hw
```
