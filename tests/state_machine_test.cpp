#include <cassert>
#include <cstring>
#include <iostream>

#include "../state_machine.h"

static int gAssertions = 0;

#define ASSERT_TRUE(expr)                                                                 \
  do {                                                                                    \
    ++gAssertions;                                                                        \
    if (!(expr)) {                                                                        \
      std::cerr << "Assertion failed at line " << __LINE__ << ": " #expr << std::endl;   \
      return 1;                                                                           \
    }                                                                                     \
  } while (0)

#define ASSERT_MODE(mode, expected) ASSERT_TRUE((mode) == (expected))

int main() {
  ModeArbiter arbiter;

  // Basic startup behavior.
  ASSERT_MODE(arbiter.currentMode(), AudioMode::IDLE);

  // Connected but no media/call -> IDLE.
  ModeTransition t = arbiter.applyInputs({true, false, false});
  ASSERT_MODE(t.to, AudioMode::IDLE);
  ASSERT_TRUE(!t.changed);

  // EC-001: incoming call while music active => CALL priority.
  t = arbiter.applyInputs({true, true, false});
  ASSERT_MODE(t.to, AudioMode::MUSIC);
  ASSERT_TRUE(t.changed);
  t = arbiter.applyInputs({true, true, true});
  ASSERT_MODE(t.to, AudioMode::CALL);
  ASSERT_TRUE(t.changed);
  ASSERT_TRUE(std::strcmp(transitionDirection(t.from, t.to), "MUSIC->CALL") == 0);

  // EC-002: call ends while media still active => MUSIC resumes.
  t = arbiter.applyInputs({true, true, false});
  ASSERT_MODE(t.to, AudioMode::MUSIC);
  ASSERT_TRUE(std::strcmp(transitionDirection(t.from, t.to), "CALL->MUSIC") == 0);

  // EC-003: call ends and media inactive => IDLE.
  t = arbiter.applyInputs({true, false, true});
  ASSERT_MODE(t.to, AudioMode::CALL);
  t = arbiter.applyInputs({true, false, false});
  ASSERT_MODE(t.to, AudioMode::IDLE);
  ASSERT_TRUE(std::strcmp(transitionDirection(t.from, t.to), "CALL->IDLE") == 0);

  // EC-004: source disconnect during call => forced IDLE.
  t = arbiter.applyInputs({true, false, true});
  ASSERT_MODE(t.to, AudioMode::CALL);
  t = arbiter.applyInputs({false, false, true});
  ASSERT_MODE(t.to, AudioMode::IDLE);
  ASSERT_TRUE(!arbiter.inputs().call_active);

  // EC-005: source disconnect during music => forced IDLE.
  t = arbiter.applyInputs({true, true, false});
  ASSERT_MODE(t.to, AudioMode::MUSIC);
  t = arbiter.applyInputs({false, true, false});
  ASSERT_MODE(t.to, AudioMode::IDLE);
  ASSERT_TRUE(!arbiter.inputs().music_active);

  // EC-006: rapid alternating events remain valid and deterministic.
  t = arbiter.applyInputs({true, true, false});   // play
  ASSERT_MODE(t.to, AudioMode::MUSIC);
  t = arbiter.applyInputs({true, false, false});  // pause
  ASSERT_MODE(t.to, AudioMode::IDLE);
  t = arbiter.applyInputs({true, true, false});   // play
  ASSERT_MODE(t.to, AudioMode::MUSIC);
  t = arbiter.applyInputs({true, true, true});    // call start
  ASSERT_MODE(t.to, AudioMode::CALL);
  t = arbiter.applyInputs({true, false, true});   // music stops during call
  ASSERT_MODE(t.to, AudioMode::CALL);
  t = arbiter.applyInputs({true, false, false});  // call end
  ASSERT_MODE(t.to, AudioMode::IDLE);
  ASSERT_TRUE(std::strcmp(modeToString(arbiter.currentMode()), "IDLE") == 0);

  // Sanitization check: disconnected input cannot stay in CALL.
  t = arbiter.applyInputs({false, false, true});
  ASSERT_MODE(t.to, AudioMode::IDLE);
  ASSERT_TRUE(!arbiter.inputs().call_active);

  std::cout << "state_machine_test: PASS (" << gAssertions << " assertions)" << std::endl;
  return 0;
}
