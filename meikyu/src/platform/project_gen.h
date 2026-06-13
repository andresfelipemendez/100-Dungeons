#ifndef PROJECT_GEN_H
#define PROJECT_GEN_H

/* project_gen: turns a project folder (marker + src/) into the generated
   build inputs the host feeds to kaji/kansi. The engine owns every build
   recipe; a project carries zero build knowledge.

   A project is any folder holding a project.meikyu marker:
       name dungeon1            (window title, ship exe name; default:
                                 the folder's basename)
       assets ../assets         (dir bundled into ship; default ../assets)

   Generated at every project open, into <build>/gen/ (absolute engine /
   vendor paths inside, so they survive any cwd; regenerated each open so
   engine upgrades propagate):
       game_unity.gen.c   engine prelude + every .c under src/, sorted
       kaji.gen.cfg       snapshot/shaders/vendor_impl/ui/editor/pch/
                          game/ship/host targets, fully concrete
       kansi.gen.cfg      watch project src + engine src */

#include "base/base_types.h"

#define PROJECT_MARKER "project.meikyu"

typedef struct {
    char name[256];
    char assets[1024];
    /* generated file paths, relative to the project root */
    char kaji_cfg[256];
    char kansi_cfg[256];
} Project;

/* cwd must be the project root (PROJECT_MARKER present). Parses the
   marker, discovers sources and shaders, writes the generated files.
   0 = refuse the open (no sources, generation write failure); the
   cause is logged. */
b32 project_open(Project *p);

/* Rescan src/ and rewrite the unity file only if the .c set changed
   (called on the kansi change edge, before the rebuild, so added or
   deleted files hot-reload like edits). 0 = write failure. */
b32 project_regen_unity(Project *p);

#endif /* PROJECT_GEN_H */
