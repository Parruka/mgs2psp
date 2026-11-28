/* bound.h (gl38, the bound turns; attic/gl38/gl38_plan.md): a measurement build leaves one kind of drawing out in each 100-vblank
   turn.  MGS2_BOUND_CYCLE=<digits> (one digit a turn, turns of MGS2_OPT_CYCLE's vblanks from its first vblank; past the list: 0) names
   each turn's kind; the kind's mask goes with every picture that begins in the turn (boot/nat_stub.h rt_bound_at: the frame that
   begins at hook V, made by the pipeline call of vblank V - 1, has turn (V - from) / n, so a turn's window holds exactly its own
   pictures' walks).  The game's state is the same in every kind: only what the host draws changes.
     kind 0 (K0)  everything drawn
     kind 1 (K1)  the ship: the builder 0x1214a0 stubbed and its groups not drawn (the native path's arm 4), BD_SHIP
     kind 2 (K2)  the people: the transfers of chain 2 left out before they are queued, BD_PEOPLE
     kind 3 (K3)  the sorted prims and meshes of chain 6 but the rain: left out call by call (ge/gelive.c), BD_SORT
     kind 4 (K4)  the rain's effect calls (ge/fxthin.c's owners) but the drop sheets' near calls (MGS2_GL_RAIN_NEAR), BD_RAIN
     kind 5 (K5)  the stage's program (blur / post, chain 10): its transfers left out, BD_STAGE
     kind 6 (K6)  the ship, the people and chain 6 whole (the rain with it): BD_SHIP | BD_PEOPLE | BD_CH6
     kind 7 (K7)  nothing drawn: every transfer left out, and on the PSP the turn is the main CPU's with the ME resting, BD_ALL
                  (the builder's stub stays arm 3: the main CPU's game code does K0's work)
     kind 8 (K8)  gl39: nothing drawn as K7, but the turn stays the ME's (BD_ALL | BD_ME): empty pictures go through the hand-over, the
                  ME and the GE; with MGS2_LOAD_CYCLE the ME also runs a load loop on its own memory (ge/xme.c xme_load)
     kind 9 (K-N2R, native step 2)  only the chains that carry the native ship and people (BD_CHI_NAT0..BD_CHI_NAT1: the people's chain 2,
                  the ship's 3 and 4) translated: every other transfer left out before it is queued (chains 0-1, 5-10 built by the game, not
                  drawn; the screen's set-up stays the host's), BD_NATIVE.  The turn stays the ME's.  With MGS2_NGE_CYCLE's digit 2 in the same
                  turn the ship's and the people's groups there are the native ones; BD_NATIVE is the main CPU's (not in the ME's 8 bits) */
#ifndef MGS2_BOUND_H
#define MGS2_BOUND_H
enum { BD_SHIP = 1, BD_PEOPLE = 2, BD_SORT = 4, BD_RAIN = 8, BD_STAGE = 16, BD_CH6 = 32, BD_ALL = 64, BD_ME = 128, BD_NATIVE = 256 };
enum { BD_CHI_PEOPLE = 2, BD_CHI_SORT = 6, BD_CHI_STAGE = 10, BD_CHI_NAT0 = 2, BD_CHI_NAT1 = 4 };   /* the chains' places in the frame's chain
                                                    list (ge/natsplit.h); step 2: the native groups' chains [C twin: boot/xlive_chk.c "nat lw chains"] */
#define BD_KINDS 10
#define BD_MASKS { 0, BD_SHIP, BD_PEOPLE, BD_SORT, BD_RAIN, BD_STAGE, BD_SHIP | BD_PEOPLE | BD_CH6, BD_ALL, BD_ALL | BD_ME, BD_NATIVE }
/* 1: the transfer of chain chi is left out under mask bd */
static inline int bd_drop_chain(int bd, int chi)
{
  if (bd & BD_NATIVE) return chi < BD_CHI_NAT0 || chi > BD_CHI_NAT1;   /* step 2, K-N2R (chi -1: not in the list) */
  return (bd & BD_ALL) || ((bd & BD_PEOPLE) && chi == BD_CHI_PEOPLE) || ((bd & BD_STAGE) && chi == BD_CHI_STAGE) || ((bd & BD_CH6) && chi == BD_CHI_SORT);
}
#endif
