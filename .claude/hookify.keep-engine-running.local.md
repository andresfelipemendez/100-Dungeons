---
name: keep-engine-running
enabled: true
event: bash
pattern: meikyu[\s\S]*\b(kill|pkill|killall)\b|\b(pkill|killall)\b[\s\S]*meikyu
action: block
---

🛑 **Don't kill the meikyu engine — keep it running.**

Hot-reloading is the whole point: the engine is the stable binary, and a code
change is applied by rebuilding the reloadable target, NOT by restarting.

- **Game code / shaders changed?** Run `meikyu --build game` (the running
  engine's watcher reloads the new dll; hot state survives via seni).
- **Want to see it?** Screenshot the already-running window instead of the
  launch → screenshot → `kill` cycle.
- **Only relaunch** when no engine is running.

If you genuinely must restart (a `--build host` engine-exe rebuild, or a hard
crash that left a dead process), the user can do it — or disable this rule.
