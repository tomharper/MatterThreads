# MatterThreads CLI Reference

## Usage

```
matterthreads [options] [scenario]
```

## Options

| Flag | Description | Default |
|------|-------------|---------|
| `--topology <preset>` | Topology preset: `full`, `linear`, `star`, `van` | `full` |
| `--seed <uint32>` | Random seed for reproducibility | random |
| `--duration <seconds>` | Max simulation duration | 120 |
| `--hw` | Enable hardware bridge mode | off |
| `--verbose` | Verbose logging | off |
| `--output <path>` | Write report to file (JSON) | stdout |
| `--scenario <name>` | Run a named scenario and exit | interactive |

### Topology Presets

| Preset | Description |
|--------|-------------|
| `full` | All mesh nodes connected, phone to BR via backhaul |
| `linear` | 0↔1↔2 chain (0↔2 blocked), phone to BR via backhaul |
| `star` | 0 is hub (1↔2 blocked), phone to BR via backhaul |
| `van` | Linear chain + phone on cellular backhaul (120ms latency, 60ms jitter, 2% loss) |

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

### Phone / Van Commands
```
discover                        Phone scans for devices via DNS-SD
backhaul down                   Simulate cellular backhaul loss (phone ↔ BR)
backhaul up                     Restore cellular backhaul
backhaul latency <ms>           Set backhaul latency
tunnel <sec>                    Simulate tunnel (backhaul down for N seconds)
crank                           Simulate engine cranking power dip (kills/restarts relay)
healing                         Show self-healing event history
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
| `phone-commission-tunnel` | Phone commissions van, subscribes, enters tunnel |
| `backhaul-loss-recovery` | Phone loses backhaul during active subscription |
| `unlock-during-crank` | Door unlock attempt while relay reboots from engine crank |
| `srp-lease-expiry-parking` | SRP lease expires during extended parking, re-registers on startup |
| `proxy-table-overflow` | BR proxy table fills up with multiple controllers |

## Examples

```bash
# Run mesh healing scenario with seed for reproducibility
matterthreads --scenario mesh-healing --seed 42

# Interactive session with linear topology
matterthreads --topology linear

# Van topology with phone controller
matterthreads --topology van

# Run for 60s with verbose logging and JSON output
matterthreads --duration 60 --verbose --output report.json

# Hardware bridge mode
matterthreads --hw
```
