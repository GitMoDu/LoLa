// Time.h

#ifndef _TIME_h
#define _TIME_h

#include <stdint.h>

static const uint32_t ONE_SECOND_MICROS = 1000000;
static const uint32_t ONE_SECOND_MILLIS = 1000;

static const uint32_t ONE_MILLI_MICROS = 1000;

/// <summary>
/// UINT32_MAX/1000000 us    = 4294.967295 s
/// UINT32_MAX/1000000        = 4294
/// </summary>
static const uint32_t SECONDS_ROLLOVER = UINT32_MAX / ONE_SECOND_MICROS;

/// <summary>
/// UINT32_MAX/1000000 us    = 4294.967295 s
/// UINT32_MAX%1000000        = 967295
/// </summary>
static const uint32_t SUB_SECONDS_ROLLOVER = UINT32_MAX % ONE_SECOND_MICROS;

#endif