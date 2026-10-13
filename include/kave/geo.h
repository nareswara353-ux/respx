#ifndef KAVE_GEO_H
#define KAVE_GEO_H

#include <stddef.h>
#include <stdint.h>

#define GEO_LAT_MIN (-85.05112878)
#define GEO_LAT_MAX 85.05112878
#define GEO_LON_MIN (-180.0)
#define GEO_LON_MAX 180.0
#define GEO_STEP 26

typedef struct geo_point {
    double longitude;
    double latitude;
} geo_point;

uint64_t geo_encode(double longitude, double latitude);
int geo_decode(uint64_t bits, geo_point *out);
double geo_distance(const geo_point *a, const geo_point *b);
double geo_distance_haversine(double lon1, double lat1, double lon2, double lat2);

#endif
