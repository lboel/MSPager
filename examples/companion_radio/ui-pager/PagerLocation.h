#pragma once

// MSPager position handling: wire suffix format/parse (FRD-010, FRD-012) and
// distance/bearing (FRD-005). Coordinates are kept in 1e-6 degrees, like LocationProvider.

#include <Arduino.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PAGER_FIX_FRESH_SECS  120   // older fixes are sent with an age

struct PagerPos {
  bool known;        // coordinates present
  bool no_gps;       // sender reported "[no GPS]"
  long lat_e6, lon_e6;
  unsigned long age_secs;   // fix age when the message was sent
};

// 1e-6 deg -> "52.5201" (rounded to 4 decimals, ~10 m)
static void formatCoord(char* dest, size_t dest_size, long v_e6) {
  long v4 = v_e6 >= 0 ? (v_e6 + 50) / 100 : (v_e6 - 50) / 100;
  long a = labs(v4);
  snprintf(dest, dest_size, "%s%ld.%04ld", v4 < 0 ? "-" : "", a / 10000, a % 10000);
}

static void formatFixAge(char* dest, size_t dest_size, unsigned long secs) {
  unsigned long mins = secs / 60;
  if (mins < 1000)       snprintf(dest, dest_size, "%lumin", mins);
  else if (mins < 2880)  snprintf(dest, dest_size, "%luh", mins / 60);
  else                   snprintf(dest, dest_size, "%lud", mins / 1440);
}

// " [52.5201,13.4050]", " [52.5201,13.4050 ~12min]" or " [no GPS]"
static void formatPosSuffix(char* dest, size_t dest_size, const PagerPos& pos) {
  if (!pos.known) {
    snprintf(dest, dest_size, " [no GPS]");
    return;
  }
  char lat[16], lon[16];
  formatCoord(lat, sizeof(lat), pos.lat_e6);
  formatCoord(lon, sizeof(lon), pos.lon_e6);
  if (pos.age_secs <= PAGER_FIX_FRESH_SECS) {
    snprintf(dest, dest_size, " [%s,%s]", lat, lon);
  } else {
    char age[12];
    formatFixAge(age, sizeof(age), pos.age_secs);
    snprintf(dest, dest_size, " [%s,%s ~%s]", lat, lon, age);
  }
}

// Strips a trailing position suffix from body (in place) into pos.
// Tolerant of 1-6 decimals. Text without a valid suffix is left untouched.
static void parsePosSuffix(char* body, PagerPos& pos) {
  memset(&pos, 0, sizeof(pos));
  int len = strlen(body);
  if (len < 3 || body[len - 1] != ']') return;
  char* open = NULL;
  for (char* p = body + len - 2; p > body; p--) {
    if (*p == '[' && p[-1] == ' ') { open = p; break; }
  }
  if (open == NULL) return;

  char inner[48];
  int ilen = (body + len - 1) - (open + 1);
  if (ilen <= 0 || ilen >= (int)sizeof(inner)) return;
  memcpy(inner, open + 1, ilen);
  inner[ilen] = 0;

  if (strcmp(inner, "no GPS") == 0) {
    pos.no_gps = true;
  } else {
    char* end;
    double lat = strtod(inner, &end);
    if (end == inner || *end != ',') return;
    char* lon_start = end + 1;
    double lon = strtod(lon_start, &end);
    if (end == lon_start || lat < -90 || lat > 90 || lon < -180 || lon > 180) return;
    unsigned long age = 0;
    if (*end == ' ' && end[1] == '~') {
      char* unit;
      unsigned long n = strtoul(end + 2, &unit, 10);
      if (unit == end + 2) return;
      if (strcmp(unit, "min") == 0)    age = n * 60;
      else if (strcmp(unit, "h") == 0) age = n * 3600;
      else if (strcmp(unit, "d") == 0) age = n * 86400;
      else return;
    } else if (*end != 0) {
      return;
    }
    pos.known = true;
    pos.lat_e6 = lround(lat * 1e6);
    pos.lon_e6 = lround(lon * 1e6);
    pos.age_secs = age;
  }
  open[-1] = 0;   // cut " [...]" off the body
}

// great-circle distance (m) and initial bearing (deg, 0 = N)
static void distanceBearing(long lat1_e6, long lon1_e6, long lat2_e6, long lon2_e6, double& dist_m, double& bearing) {
  const double R = 6371000.0;
  double la1 = lat1_e6 * 1e-6 * DEG_TO_RAD, lo1 = lon1_e6 * 1e-6 * DEG_TO_RAD;
  double la2 = lat2_e6 * 1e-6 * DEG_TO_RAD, lo2 = lon2_e6 * 1e-6 * DEG_TO_RAD;
  double dla = la2 - la1, dlo = lo2 - lo1;
  double a = sin(dla / 2) * sin(dla / 2) + cos(la1) * cos(la2) * sin(dlo / 2) * sin(dlo / 2);
  dist_m = 2 * R * atan2(sqrt(a), sqrt(1 - a));
  double y = sin(dlo) * cos(la2);
  double x = cos(la1) * sin(la2) - sin(la1) * cos(la2) * cos(dlo);
  bearing = fmod(atan2(y, x) * RAD_TO_DEG + 360.0, 360.0);
}

static const char* compass8(double bearing) {
  static const char* const dirs[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
  return dirs[((int)((bearing + 22.5) / 45.0)) % 8];
}

// "340m NE", "1.2km N", "12km SW"
static void formatDistance(char* dest, size_t dest_size, double dist_m, double bearing) {
  if (dist_m < 1000)       snprintf(dest, dest_size, "%dm %s", ((int)(dist_m + 5) / 10) * 10, compass8(bearing));
  else if (dist_m < 10000) snprintf(dest, dest_size, "%.1fkm %s", dist_m / 1000.0, compass8(bearing));
  else                     snprintf(dest, dest_size, "%dkm %s", (int)(dist_m / 1000.0 + 0.5), compass8(bearing));
}
