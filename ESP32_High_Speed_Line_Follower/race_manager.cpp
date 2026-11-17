/**
 * race_manager.cpp  -- see race_manager.h
 */
#include "race_manager.h"

const char* raceStateName(RaceState s) {
  switch (s) {
    case RaceState::BOOT:        return "BOOT";
    case RaceState::CALIBRATING: return "CALIBRATING";
    case RaceState::READY:       return "READY";
    case RaceState::RUNNING:     return "RUNNING";
    case RaceState::RECOVERING:  return "RECOVERING";
    case RaceState::STOPPED:     return "STOPPED";
    case RaceState::ERROR:       return "ERROR";
    case RaceState::FINISHED:    return "FINISHED";
    default:                     return "?";
  }
}

RaceManager::RaceManager() {}

void RaceManager::setState(RaceState s, uint32_t nowMs) {
  state_        = s;
  stateEnterMs_ = nowMs;
}
