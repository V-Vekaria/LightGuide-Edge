/*
 * self_test.h - exercises the decision logic against tables of known-good answers.
 *
 * There is no host C++ compiler on this machine, so the tests run on the board
 * at every boot and report over serial. That is not a compromise: it proves the
 * logic on the same toolchain and the same float behaviour that ships.
 */
#ifndef SELF_TEST_H
#define SELF_TEST_H

#include "decision.h"

struct DecisionCase {
  float       live;
  float       ref;
  Verdict     prev;
  Verdict     expect;
  const char *why;
};

const DecisionCase DISTANCE_CASES[] = {
  // Plain classification, with no history worth speaking of.
  { 1000, 1000, V_NO_READ, V_CORRECT, "dead on the reference" },
  { 1500, 1000, V_NO_READ, V_ABOVE,   "150 cm against a 100 cm reference" },
  {  700, 1000, V_NO_READ, V_BELOW,   "70 cm against a 100 cm reference" },

  // Edges of the enter band. 30 mm is inside it, 31 mm is not.
  { 1030, 1000, V_ABOVE, V_CORRECT, "+30 mm is the last CORRECT" },
  { 1031, 1000, V_ABOVE, V_ABOVE,   "+31 mm is past the enter band" },
  {  970, 1000, V_BELOW, V_CORRECT, "-30 mm is the last CORRECT" },
  {  969, 1000, V_BELOW, V_BELOW,   "-31 mm is past the enter band" },

  // Hysteresis. Same reading, different history, different answer - this pair
  // is the entire point of TOL_EXIT_MM and the reason the buzzer stays steady
  // when the operator parks on the boundary.
  { 1035, 1000, V_CORRECT, V_CORRECT, "+35 mm holds CORRECT once already CORRECT" },
  { 1035, 1000, V_ABOVE,   V_ABOVE,   "+35 mm cannot enter CORRECT from FAR" },
  {  965, 1000, V_CORRECT, V_CORRECT, "-35 mm holds CORRECT once already CORRECT" },
  {  965, 1000, V_BELOW,   V_BELOW,   "-35 mm cannot enter CORRECT from CLOSE" },

  // Edges of the exit band.
  { 1040, 1000, V_CORRECT, V_CORRECT, "+40 mm is the last hold" },
  { 1041, 1000, V_CORRECT, V_ABOVE,   "+41 mm finally releases to FAR" },
  {  959, 1000, V_CORRECT, V_BELOW,   "-41 mm finally releases to CLOSE" },

  // No reading beats every history. A wrong CORRECT is worse than an honest
  // "I cannot tell you".
  {   -1, 1000, V_CORRECT, V_NO_READ, "no echo overrides CORRECT" },
  {   -1, 1000, V_ABOVE,   V_NO_READ, "no echo overrides FAR" },

  // A reference other than the default, so nothing is quietly hardcoded to 1000.
  {  800,  800, V_NO_READ, V_CORRECT, "a reference of 80 cm behaves the same" },
  {  900,  800, V_NO_READ, V_ABOVE,   "90 cm is FAR of an 80 cm reference" },
};

const int DISTANCE_CASE_COUNT = sizeof(DISTANCE_CASES) / sizeof(DISTANCE_CASES[0]);

inline int runDistanceCases(Print &out) {
  int failures = 0;
  for (int i = 0; i < DISTANCE_CASE_COUNT; i++) {
    const DecisionCase &c = DISTANCE_CASES[i];
    Verdict got = decideDistance(c.live, c.ref, c.prev);
    if (got != c.expect) {
      failures++;
      out.print(F("  FAIL dist["));   out.print(i);
      out.print(F("] "));             out.print(c.why);
      out.print(F(" - expected "));   out.print(distanceWord(c.expect));
      out.print(F(", got "));         out.println(distanceWord(got));
    }
  }
  return failures;
}

inline int runDecisionSelfTest(Print &out) {
  int failures = runDistanceCases(out);
  int total    = DISTANCE_CASE_COUNT;

  out.print(F("DECISION SELF-TEST: "));
  out.print(total - failures);
  out.print('/');
  out.print(total);
  out.println(failures ? F(" passed - FAILURES PRESENT") : F(" passed - OK"));
  return failures;
}

#endif
