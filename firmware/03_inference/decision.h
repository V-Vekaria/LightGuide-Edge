/*
 * decision.h - the verdict, and nothing else.
 *
 * Deliberately free of every Arduino API: no pins, no Serial, no millis(). That
 * is what lets self_test.h exercise it exhaustively on the board, and it is what
 * makes the Phase 2 swap a change to one function body rather than a rewrite.
 */
#ifndef DECISION_H
#define DECISION_H

#include <math.h>

enum Verdict { V_NO_ECHO = 0, V_CORRECT, V_FAR, V_CLOSE };

// Enter CORRECT within +-30 mm; leave it only beyond +-40 mm. The 10 mm gap is
// hysteresis. With a single threshold, a target parked on the boundary flips the
// verdict several times a second: the display flickers and the buzzer chatters,
// which is the most visible way for a working device to look broken on camera.
const float TOL_ENTER_MM = 30.0f;
const float TOL_EXIT_MM  = 40.0f;

// `prev` is last loop's verdict. Passing it in rather than keeping it in a
// static keeps the function pure, so a test can drive it with any history.
inline Verdict decide(float live_mm, float ref_mm, Verdict prev) {
  if (live_mm < 0.0f) return V_NO_ECHO;

  const float diff = live_mm - ref_mm;
  const float mag  = fabsf(diff);
  const float band = (prev == V_CORRECT) ? TOL_EXIT_MM : TOL_ENTER_MM;

  if (mag <= band) return V_CORRECT;
  return (diff > 0.0f) ? V_FAR : V_CLOSE;
}

inline const char *verdictName(Verdict v) {
  switch (v) {
    case V_CORRECT: return "CORRECT";
    case V_FAR:     return "FAR";
    case V_CLOSE:   return "CLOSE";
    default:        return "NO_ECHO";
  }
}

#endif
