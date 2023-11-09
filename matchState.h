// Enumerated matchState_t type for use in ESP-NOW packets
// between electrical system nodes

#ifnddef MATCH_STATE_H
#define MATCH_STATE_H

#include "Arduino.h"

typedef enum {
  EXAMPLE_ONE_ST, // description
  EXAMPLE_TWO_ST  // description
} matchState_t;

#endif // MATCH_STATE_H
