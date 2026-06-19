# 07 — Reading Order for Your Library

You own eleven strong books. The mistake would be reading them front-to-back in some ranking. They play **three different roles**, and you interleave them with the [roadmap](06-roadmap.md) milestones — build-along books cover-to-cover, references by chapter when a milestone needs them, theory dipped into on demand.

## ⚠️ The one caveat that colors everything

**Almost every book here teaches OOP / class-hierarchy / scene-graph architecture. You are building a Casey-style fat-struct, data-oriented engine.** That is the *opposite* structural philosophy.

So read them for **what a system does and why** — never copy **how they structure the code**. When Madhav makes an `Actor` base class with virtual `UpdateComponent`, or Eberly builds a polymorphic `Spatial`/`Node` scene graph, mentally invert it: *one fat `Entity`, a flat array, a `flags` bitfield, systems as loops*. The algorithms transfer; the object models don't. None of these books cover fat-struct DOD itself — for that, supplement with Mike Acton's "Data-Oriented Design" talk and Casey Muratori's Handmade Hero, which you're already following.

Concretely:
- **Madhav, Gregory, Eberly (all three)** — class/inheritance/scene-graph designs. Mine the *concepts and math*, drop the architecture.
- **Lengyel (FGED)** — lean, struct-friendly, modern. Closest to your style; trust its structure most.
- **Ericson, RTR** — algorithm/reference books, language-agnostic. No architecture to fight.
- **Millington, Beuken** — build-along; follow the logic, restructure into your arena/fat-struct as you port.

## The three roles

| Role | Books | How to read |
|------|-------|-------------|
| **Build-along** (front-to-back, early) | Beuken; Millington; Madhav (selectively) | Read in order, type along, port into DOD |
| **Reference** (by chapter, per milestone) | Gregory; Real-Time Rendering; Ericson; Lengyel V2 | Jump to the chapter the milestone needs |
| **Deep theory** (dip on demand) | Lengyel V1; Eberly ×3 | Reach for when you hit a math/geometry wall |

## Read-first foundation (parallel to M0–M2)

Before deep milestone work, build the mental model and the math you'll write on day one:

1. **Beuken — *Fundamentals of C/C++ Game Programming on SBCs* (whole book, fast).** Uniquely relevant: it's **target-based development on single-board computers** — i.e. your **Raspberry Pi 5** goal. Read it early for the cross-compile / fixed-target / SBC workflow and a gentle ramp (Ch. 6 "A New Third Dimension" onward). It's the only book that speaks directly to your deployment target.
2. **Gregory — *Game Engine Architecture*, Ch. 1–3, 5–8.** The single best "what is an engine and how do the parts fit" map: tooling, software-engineering-for-games, **3D math (5)**, engine support systems (6), **resources & file system (7)**, **the game loop & real-time simulation (8)**. Skip Ch. 4 (parallelism) for now. This is your compass for the whole roadmap.
3. **Lengyel — *FGED Vol. 1: Mathematics*, Ch. 1–2 (Vectors/Matrices, Transforms).** Exactly the `math.h` you write for M1. Concise, correct, DOD-friendly. Ch. 3–4 later.

## Per-milestone mapping

```mermaid
flowchart LR
    subgraph found["Foundation (M0-M2)"]
        b["Beuken (all)"]
        g1["Gregory 1-3,5-8"]
        l1["Lengyel V1 1-2"]
    end
    subgraph render["M1 Render core"]
        l2["Lengyel V1.3 + V2.5-7"]
        rtr["RTR 2-6"]
        m1["Madhav 5-6"]
    end
    subgraph gameplay["M2-M7 Entities→Editor"]
        g2["Gregory 15-16, 10"]
        m2["Madhav 14"]
    end
    subgraph coll["M8 Collision"]
        e["Ericson 1-7,11-12"]
    end
    subgraph phys["M10-11 Combat/Physics"]
        mil["Millington 1-8"]
        m3["Madhav 4,9"]
    end
    subgraph polish["M13-14 Audio/Anim"]
        m4["Madhav 7,12,13"]
        g3["Gregory 12,14"]
    end
    found --> render --> gameplay --> coll --> phys --> polish
```

| Milestone | Primary read | Reference / supplement |
|-----------|--------------|------------------------|
| **M0** Foundation | Beuken (whole); Gregory 8 (game loop) | Gregory 2 (tools) |
| **M1** 3D render core | Lengyel V1.3 (Geometry), **V2.5–7** (graphics pipeline, projections, shading/texturing); RTR **2–6** (pipeline, GPU, transforms, shading, texturing) | Madhav 5–6 (concrete wiring — it's OpenGL, you're on SDL_GPU/Vulkan, read for flow not API); Gregory 11 |
| **M2** Fat-struct entities | Gregory **15–16** (gameplay/object models) — *read critically, invert to DOD* | Acton DOD talk; Handmade Hero (external) |
| **M3** Assets | Gregory **7** (resources & file system); Madhav **14** (level files & binary data) | — |
| **M4** Reflection (serialize+inspector) | Gregory 7 + 15 (object serialization is thin in books) | *Mostly your own design — see [04]/[06]* |
| **M5** Debug draw + fixed step | Gregory **8** (real-time sim), **10** (debugging tools) | — |
| **M6** Camera + controller | Madhav **9** (Cameras); RTR 4 (transforms) | Lengyel V2.6 (projections) |
| **M7** Editor | Gregory **10** (tools for debugging & development) | *Dear ImGui docs — books barely cover editor-building* |
| **M8** Collision | **Ericson — the whole point: 1–7 (BV, primitive tests, BVH, spatial partitioning), 11–12 (robustness), 13 (optimization)** | Eberly *3D Game Engine Design* 8, 13–15 (containment/distance/intersection); Gregory 13 |
| **M9** Rooms/levels | Gregory 7 (streaming/resources) | *Mostly design, little book coverage* |
| **M10** Combat + AI | Madhav **4** (AI — state machines suffice for Zelda enemies); Gregory 16 | — |
| **M11** Items/physics | **Millington 1–8** (particles → mass-aggregate: plenty for knockback/push/projectiles); Gregory 15 | Millington 9–17 only if you want true rigid bodies |
| **M12** Dialogue/quest | Gregory 16 (event/messaging/gameplay foundations) | — |
| **M13** Audio | Madhav **7** (Audio); Gregory **14** (Audio) | — |
| **M14** Animation/polish | Madhav **12** (Skeletal Animation), 13 (Intermediate Graphics); Gregory **12** (Animation Systems); RTR 13 (Beyond Polygons, particles) | Lengyel V1.2 (quaternions for skinning) |

## Deep-theory shelf (reach when you hit a wall, not before)

- **Lengyel V1 Ch. 3–4** (Geometry, Advanced/Grassmann Algebra) and **V2 Ch. 8–10** (Lighting & Shadows, Visibility, Advanced Rendering) — when M1's lighting gets serious.
- **Eberly — *3D Game Engine Design* Ch. 16–17** (Numerical Methods, Rotations) and **Ch. 11–12** (Curves, Surfaces) — geometry/numerics reference. Note: its rendering and scene-graph chapters are dated (older GPU era, deep OOP) — skip for architecture.
- **Eberly — *Game Physics* Ch. 7–14** — a full applied-math reference (linear/affine algebra, calculus, quaternions, ODEs, LCP). Only if you go beyond Millington into real physics.
- **Eberly — *3D Game Engine Architecture* (Wild Magic)** — the implementation companion to *3D Game Engine Design*. It's a textbook OOP scene-graph engine — **most against your grain**. Read Ch. 2 (Core Systems: memory, smart pointers, controllers) and Ch. 6–7 (Collision, Physics) for ideas only; don't adopt the scene-graph architecture.
- **RTR Ch. 7–9** (Shadows, Light & Color, Physically Based Shading), **18–19** (optimization, acceleration) — when you upgrade visuals/perf.

## If you want one linear path

A pragmatic front-to-back-ish sequence that tracks the build:

1. **Beuken** (whole) — SBC/Pi workflow + ramp.
2. **Gregory 1–3, 5–8** — engine model + math + loop + resources.
3. **Lengyel V1 1–2**, then **V1 3** — math you'll code.
4. **RTR 2–6** + **Lengyel V2 5–7** — rendering core (during M1).
5. **Gregory 15–16** — gameplay/object model (invert to DOD, during M2–M7).
6. **Madhav 9, 14, 4, 7** — cameras, level files, AI, audio (cherry-picked as milestones hit).
7. **Ericson 1–7, 11–13** — collision (M8), the most important single read of the back half.
8. **Millington 1–8** — light physics (M11).
9. **Gregory 12 + Madhav 12** — animation (M14).
10. **Lengyel V1 4, V2 8–10, Eberly, RTR 7–9** — deep theory, on demand, forever.

## Bottom line

- **Beuken now** (Pi5 target + gentle on-ramp).
- **Gregory is your spine** — survey early, return per milestone.
- **Lengyel is your math home** — trust its structure most (closest to DOD).
- **Ericson when you hit M8** — the keystone reference; collision is where Zelda-feel lives.
- **Millington for light physics**, not full rigid body, unless you choose to.
- **Eberly = deep math/geometry reference only** — never its scene-graph architecture.
- Through all of it: **read for concepts, build in fat-struct DOD.**
