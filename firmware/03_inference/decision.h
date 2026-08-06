/*
 * decision.h - the verdict, and nothing else.
 *
 * Deliberately free of every Arduino API: no pins, no Serial, no millis(). That
 * is what lets self_test.h exercise it exhaustively on the board, and it is what
 * makes the Phase 3 swap a change to one function body rather than a rewrite.
 */
#ifndef DECISION_H
#define DECISION_H

#include <math.h>

// Channel-neutral on purpose. V_ABOVE means the live reading sits above the
// reference; each channel renders that in its own words, because "FAR" is
// meaningless for brightness and "BRIGHT" is meaningless for distance.
enum Verdict { V_NO_READ = 0, V_CORRECT, V_ABOVE, V_BELOW };

// Distance: fixed bands. Enter CORRECT within +-30 mm, leave it only beyond
// +-40 mm. The 10 mm gap is hysteresis - with a single threshold, a target
// parked on the boundary flips the verdict several times a second, the display
// flickers and the buzzer chatters.
const float TOL_ENTER_MM = 30.0f;
const float TOL_EXIT_MM  = 40.0f;

// Light: proportional bands. A fixed count band would be 3% of a bright
// reference and 15% of a dim one - fussy in one setup and sloppy in another for
// no principled reason. 5% of a typical 2000-count reference is +-100, about
// 17x the measured +-6 noise floor and well inside the ~200-count gap measured
// between adjacent dimmer settings, so one notch on the dimmer registers.
const float LIGHT_TOL_ENTER_PCT = 0.05f;
const float LIGHT_TOL_EXIT_PCT  = 0.07f;

// A reading pinned near either rail means a clipped divider, not a dark or a
// bright room. This is the exact fault that made the LDR look dead on 6 August,
// and reporting 4095 as "very bright" would have hidden it instead of exposing it.
const int LIGHT_MIN_VALID = 50;
const int LIGHT_MAX_VALID = 4045;

inline bool lightReadingValid(int counts) {
  return counts > LIGHT_MIN_VALID && counts < LIGHT_MAX_VALID;
}

// `prev` is last loop's verdict for this channel. Passing it in rather than
// keeping it in a static keeps the function pure, so a test can drive it with
// any history. A negative `live` means the channel could not be read at all.
inline Verdict decide(float live, float ref, Verdict prev,
                      float enterBand, float exitBand) {
  if (live < 0.0f) return V_NO_READ;

  const float diff = live - ref;
  const float mag  = fabsf(diff);
  const float band = (prev == V_CORRECT) ? exitBand : enterBand;

  if (mag <= band) return V_CORRECT;
  return (diff > 0.0f) ? V_ABOVE : V_BELOW;
}

inline Verdict decideDistance(float liveMm, float refMm, Verdict prev) {
  return decide(liveMm, refMm, prev, TOL_ENTER_MM, TOL_EXIT_MM);
}

inline Verdict decideLight(float live, float ref, Verdict prev) {
  return decide(live, ref, prev,
                ref * LIGHT_TOL_ENTER_PCT,
                ref * LIGHT_TOL_EXIT_PCT);
}

inline const char *distanceWord(Verdict v) {
  switch (v) {
    case V_CORRECT: return "CORRECT";
    case V_ABOVE:   return "FAR";
    case V_BELOW:   return "CLOSE";
    default:        return "NO ECHO";
  }
}

inline const char *lightWord(Verdict v) {
  switch (v) {
    case V_CORRECT: return "CORRECT";
    case V_ABOVE:   return "BRIGHT";
    case V_BELOW:   return "DARK";
    default:        return "NO READ";
  }
}

#endif
