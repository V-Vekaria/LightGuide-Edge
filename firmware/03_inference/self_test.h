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

// Light uses percentage bands, so the same absolute deviation is CORRECT at a
// bright reference and not at a dim one. Cases at 650, 2000 and 3400 counts
// cover the range the LDR actually produced during the 6 August sweep.
const DecisionCase LIGHT_CASES[] = {
  // ref 2000 -> enter +-100, exit +-140
  { 2000, 2000, V_NO_READ, V_CORRECT, "dead on the reference" },
  { 2100, 2000, V_NO_READ, V_CORRECT, "+100 is the last CORRECT at ref 2000" },
  { 2101, 2000, V_NO_READ, V_ABOVE,   "+101 is BRIGHT at ref 2000" },
  { 1900, 2000, V_NO_READ, V_CORRECT, "-100 is the last CORRECT at ref 2000" },
  { 1899, 2000, V_NO_READ, V_BELOW,   "-101 is DARK at ref 2000" },

  // Hysteresis: same reading, different history, different answer.
  { 2130, 2000, V_CORRECT, V_CORRECT, "+130 holds CORRECT once already CORRECT" },
  { 2130, 2000, V_ABOVE,   V_ABOVE,   "+130 cannot enter CORRECT from BRIGHT" },
  { 2141, 2000, V_CORRECT, V_ABOVE,   "+141 finally releases to BRIGHT" },
  { 1859, 2000, V_CORRECT, V_BELOW,   "-141 finally releases to DARK" },

  // The whole point of proportional bands: 150 counts is fine at a bright
  // reference and 40 counts is not at a dim one.
  {  680,  650, V_NO_READ, V_CORRECT, "+30 is CORRECT at ref 650 (band 32.5)" },
  {  690,  650, V_NO_READ, V_ABOVE,   "+40 is BRIGHT at ref 650 (band 32.5)" },
  { 3550, 3400, V_NO_READ, V_CORRECT, "+150 is CORRECT at ref 3400 (band 170)" },
  { 3600, 3400, V_NO_READ, V_ABOVE,   "+200 is BRIGHT at ref 3400 (band 170)" },

  // An unreadable channel beats every history.
  {   -1, 2000, V_CORRECT, V_NO_READ, "clipped reading overrides CORRECT" },
};

const int LIGHT_CASE_COUNT = sizeof(LIGHT_CASES) / sizeof(LIGHT_CASES[0]);

inline int runLightCases(Print &out) {
  int failures = 0;
  for (int i = 0; i < LIGHT_CASE_COUNT; i++) {
    const DecisionCase &c = LIGHT_CASES[i];
    Verdict got = decideLight(c.live, c.ref, c.prev);
    if (got != c.expect) {
      failures++;
      out.print(F("  FAIL light[")); out.print(i);
      out.print(F("] "));            out.print(c.why);
      out.print(F(" - expected "));  out.print(lightWord(c.expect));
      out.print(F(", got "));        out.println(lightWord(got));
    }
  }
  return failures;
}

struct ValidityCase { int counts; bool expect; const char *why; };

// A clipped rail is a wiring fault, not a light level. Reporting 4095 as
// "very bright" would have hidden the 1k-pulldown fault instead of exposing it.
const ValidityCase VALIDITY_CASES[] = {
  {    0, false, "0 counts is a shorted or dead divider" },
  {   50, false, "50 is the exclusive lower bound" },
  {   51, true,  "51 is the first accepted reading" },
  { 2000, true,  "a normal mid-scale reading" },
  { 4044, true,  "4044 is the last accepted reading" },
  { 4045, false, "4045 is the exclusive upper bound" },
  { 4095, false, "4095 counts is a rail, not a room" },
};

const int VALIDITY_CASE_COUNT = sizeof(VALIDITY_CASES) / sizeof(VALIDITY_CASES[0]);

inline int runValidityCases(Print &out) {
  int failures = 0;
  for (int i = 0; i < VALIDITY_CASE_COUNT; i++) {
    const ValidityCase &c = VALIDITY_CASES[i];
    bool got = lightReadingValid(c.counts);
    if (got != c.expect) {
      failures++;
      out.print(F("  FAIL valid[")); out.print(i);
      out.print(F("] "));            out.print(c.why);
      out.print(F(" - expected "));  out.print(c.expect ? F("valid") : F("invalid"));
      out.print(F(", got "));        out.println(got ? F("valid") : F("invalid"));
    }
  }
  return failures;
}

inline int runDecisionSelfTest(Print &out) {
  int failures = runDistanceCases(out)
               + runLightCases(out)
               + runValidityCases(out);
  int total    = DISTANCE_CASE_COUNT + LIGHT_CASE_COUNT + VALIDITY_CASE_COUNT;

  out.print(F("DECISION SELF-TEST: "));
  out.print(total - failures);
  out.print('/');
  out.print(total);
  out.println(failures ? F(" passed - FAILURES PRESENT") : F(" passed - OK"));
  return failures;
}

#endif
