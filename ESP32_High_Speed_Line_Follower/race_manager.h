/**
 * race_manager.h
 * -----------------------------------------------------------------------------
 * Race finite-state machine model, timing statistics and race counters.
 *
 * The RaceManager holds the current state, race timing and loop-timing
 * statistics. The actual per-state behavior (what the motors do) is driven by
 * the main .ino, which queries/sets the state here. Keeping the FSM data in one
 * place makes transitions explicit and testable.
 * -----------------------------------------------------------------------------
 */
#pragma once
#include <Arduino.h>
#include "config.h"

enum class RaceState : uint8_t {
  BOOT,         // hardware init
  CALIBRATING,  // sampling sensor calibration (motors inhibited)
  READY,        // calibrated, waiting for start command (motors inhibited)
  RUNNING,      // active line following
  RECOVERING,   // line lost, searching
  STOPPED,      // user/safe stop; requires explicit restart
  ERROR,        // fault; requires explicit user action
  FINISHED      // validated finish detected (optional feature)
};

const char* raceStateName(RaceState s);

// Rolling loop-timing statistics (updated every control cycle).
struct LoopStats {
  uint32_t lastPeriodUs   = 0;   // measured time between cycle starts
  uint32_t lastComputeUs  = 0;   // time spent inside the control computation
  uint32_t maxPeriodUs    = 0;
  uint32_t maxComputeUs   = 0;
  uint32_t missedDeadlines = 0;  // cycles exceeding DEADLINE_WARN_US
  uint32_t cycles          = 0;

  void reset() {
    lastPeriodUs = lastComputeUs = 0;
    maxPeriodUs = maxComputeUs = 0;
    missedDeadlines = 0;
    cycles = 0;
  }
  void note(uint32_t periodUs, uint32_t computeUs) {
    lastPeriodUs  = periodUs;
    lastComputeUs = computeUs;
    if (periodUs  > maxPeriodUs)  maxPeriodUs  = periodUs;
    if (computeUs > maxComputeUs) maxComputeUs = computeUs;
    if (periodUs  > DEADLINE_WARN_US) missedDeadlines++;
    cycles++;
  }
};

class RaceManager {
 public:
  RaceManager();

  RaceState state() const { return state_; }
  void setState(RaceState s, uint32_t nowMs);

  bool is(RaceState s) const { return state_ == s; }

  // Race timing.
  void     markRaceStart(uint32_t nowMs) { raceStartMs_ = nowMs; }
  uint32_t raceElapsedMs(uint32_t nowMs) const { return nowMs - raceStartMs_; }

  // Event counters.
  void     noteLineLoss()  { lineLossCount_++; }
  void     noteRecovery()  { recoveryCount_++; }
  uint32_t lineLossCount() const { return lineLossCount_; }
  uint32_t recoveryCount() const { return recoveryCount_; }

  // How long the FSM has been in the current state.
  uint32_t stateElapsedMs(uint32_t nowMs) const { return nowMs - stateEnterMs_; }

  LoopStats stats;

  void resetCounters() { lineLossCount_ = 0; recoveryCount_ = 0; stats.reset(); }

 private:
  RaceState state_        = RaceState::BOOT;
  uint32_t  stateEnterMs_ = 0;
  uint32_t  raceStartMs_  = 0;
  uint32_t  lineLossCount_ = 0;
  uint32_t  recoveryCount_ = 0;
};
