#include "kave/geo.h"
#include <math.h>
#include <string.h>

#define EARTH_RADIUS_M 6372797.560856
#define D_R (M_PI / 180.0)

static uint64_t geo_interleave64(uint32_t xlo, uint32_t ylo)
{
    static const uint64_t B[] = {
        0x5555555555555555ULL,
        0x3333333333333333ULL,
        0x0F0F0F0F0F0F0F0FULL,
        0x00FF00FF00FF00FFULL,
        0x0000FFFF0000FFFFULL
    };
    static const unsigned int S[] = {1, 2, 4, 8, 16};
    uint64_t x = xlo;
    uint64_t y = ylo;
    x = (x | (x << S[4])) & B[4];
    x = (x | (x << S[3])) & B[3];
    x = (x | (x << S[2])) & B[2];
    x = (x | (x << S[1])) & B[1];
    x = (x | (x << S[0])) & B[0];
    y = (y | (y << S[4])) & B[4];
    y = (y | (y << S[3])) & B[3];
    y = (y | (y << S[2])) & B[2];
    y = (y | (y << S[1])) & B[1];
    y = (y | (y << S[0])) & B[0];
    return x | (y << 1);
}

uint64_t geo_encode(double longitude, double latitude)
{
    if (longitude < GEO_LON_MIN) longitude = GEO_LON_MIN;
    if (longitude > GEO_LON_MAX) longitude = GEO_LON_MAX;
    if (latitude < GEO_LAT_MIN) latitude = GEO_LAT_MIN;
    if (latitude > GEO_LAT_MAX) latitude = GEO_LAT_MAX;
    double lat_offset = (latitude - GEO_LAT_MIN) / (GEO_LAT_MAX - GEO_LAT_MIN);
    double lon_offset = (longitude - GEO_LON_MIN) / (GEO_LON_MAX - GEO_LON_MIN);
    uint32_t lat_bits = (uint32_t)(lat_offset * (1ULL << GEO_STEP));
    uint32_t lon_bits = (uint32_t)(lon_offset * (1ULL << GEO_STEP));
    return geo_interleave64(lat_bits, lon_bits);
}

static void geo_deinterleave64(uint64_t interleaved, uint32_t *x, uint32_t *y)
{
    static const uint64_t B[] = {
        0x5555555555555555ULL,
        0x3333333333333333ULL,
        0x0F0F0F0F0F0F0F0FULL,
        0x00FF00FF00FF00FFULL,
        0x0000FFFF0000FFFFULL,
        0x00000000FFFFFFFFULL
    };
    static const unsigned int S[] = {0, 1, 2, 4, 8, 16};
    uint64_t xv = interleaved & B[0];
    uint64_t yv = (interleaved >> 1) & B[0];
    xv = (xv | (xv >> S[0])) & B[1];
    xv = (xv | (xv >> S[1])) & B[2];
    xv = (xv | (xv >> S[2])) & B[3];
    xv = (xv | (xv >> S[3])) & B[4];
    xv = (xv | (xv >> S[4])) & B[5];
    yv = (yv | (yv >> S[0])) & B[1];
    yv = (yv | (yv >> S[1])) & B[2];
    yv = (yv | (yv >> S[2])) & B[3];
    yv = (yv | (yv >> S[3])) & B[4];
    yv = (yv | (yv >> S[4])) & B[5];
    *x = (uint32_t)xv;
    *y = (uint32_t)yv;
}

int geo_decode(uint64_t bits, geo_point *out)
{
    if (!out) return -1;
    uint32_t lat_bits, lon_bits;
    geo_deinterleave64(bits, &lat_bits, &lon_bits);
    double lat_min = GEO_LAT_MIN + (lat_bits * (GEO_LAT_MAX - GEO_LAT_MIN) / (double)(1ULL << GEO_STEP));
    double lat_max = GEO_LAT_MIN + ((lat_bits + 1) * (GEO_LAT_MAX - GEO_LAT_MIN) / (double)(1ULL << GEO_STEP));
    double lon_min = GEO_LON_MIN + (lon_bits * (GEO_LON_MAX - GEO_LON_MIN) / (double)(1ULL << GEO_STEP));
    double lon_max = GEO_LON_MIN + ((lon_bits + 1) * (GEO_LON_MAX - GEO_LON_MIN) / (double)(1ULL << GEO_STEP));
    out->latitude = (lat_min + lat_max) / 2.0;
    out->longitude = (lon_min + lon_max) / 2.0;
    return 0;
}

double geo_distance_haversine(double lon1, double lat1, double lon2, double lat2)
{
    double lat1r = lat1 * D_R;
    double lat2r = lat2 * D_R;
    double u = sin((lat2r - lat1r) / 2.0);
    double v = sin((lon2 - lon1) * D_R / 2.0);
    double a = u * u + cos(lat1r) * cos(lat2r) * v * v;
    return 2.0 * EARTH_RADIUS_M * asin(sqrt(a));
}

double geo_distance(const geo_point *a, const geo_point *b)
{
    if (!a || !b) return -1.0;
    return geo_distance_haversine(a->longitude, a->latitude, b->longitude, b->latitude);
}
