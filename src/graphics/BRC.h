#pragma once

#include "GPSStatus.h"
#include "gps/GeoCoord.h"
#include "graphics/Screen.h"

using namespace meshtastic;

const int32_t BRC_LATI= (40.786958 * 1e7);
const int32_t BRC_LONI = (-119.202994 * 1e7);
const double BRC_LATF = 40.786958;
const double BRC_LONF = -119.202994;
const double BRC_NOON = 1.5;
const double RAD_TO_HOUR = (6.0/3.14159);
const double METER_TO_FEET = 3.28084;

// Pre-calculated street data for performance
struct StreetInfo {
    float center;
    float width;
    const char* name;
};

static const StreetInfo streets[] = {
    {2500, 50, "Esp"},
    {2940, 220, "A"},
    {3230, 145, "B"},        // 2940+290
    {3520, 145, "C"},        // 2940+290*2
    {3810, 145, "D"},        // 2940+290*3
    {4100, 145, "E"},        // 2940+290*4
    {4590, 245, "F"},        // 2940+290*4+490
    {4880, 145, "G"},        // 2940+290*5+490
    {5170, 145, "H"},        // 2940+290*6+490
    {5460, 145, "I"},        // 2940+290*7+490
    {5650, 95, "J"},         // 2940+290*7+490+190
    {5840, 95, "K"},         // 2940+290*7+490+190*2
    {5915, 0, nullptr}       // 2940+290*7+490+190*2+75
};

static char* BRCAddress(int32_t lat, int32_t lon)
{
    thread_local static char addrStr[20];
    
    // Cache previous calculations to avoid expensive trigonometric operations
    // when position hasn't changed significantly
    thread_local static int32_t cachedLat = 0;
    thread_local static int32_t cachedLon = 0;
    thread_local static float cachedBearing = 0.0f;
    thread_local static float cachedDistance = 0.0f;
    thread_local static bool cacheValid = false;
    
    // Check if we can use cached values
    // GPS int32_t format: 1e-7 degrees, so 1 unit ≈ 1.11 cm at equator
    // 25 units ≈ 28 cm, 90 units ≈ 1 meter, 900 units ≈ 10 meters
    const int32_t CACHE_THRESHOLD = 90;  // ~1 meter - good balance of precision vs performance
    bool positionChanged = !cacheValid || 
                          abs(lat - cachedLat) > CACHE_THRESHOLD || 
                          abs(lon - cachedLon) > CACHE_THRESHOLD;
    
    if (positionChanged) {
        // Update cache with new calculations
        double latD = DegD(lat);
        double lonD = DegD(lon);
        
        cachedBearing = GeoCoord::bearing(BRC_LATF, BRC_LONF, latD, lonD) * RAD_TO_HOUR;
        cachedDistance = GeoCoord::latLongToMeter(BRC_LATF, BRC_LONF, latD, lonD) * METER_TO_FEET;
        
        cachedLat = lat;
        cachedLon = lon;
        cacheValid = true;
    }
    
    // Use cached values for calculations
    float bearingToMan = cachedBearing;
    bearingToMan += 12.0 - BRC_NOON;
    while (bearingToMan > 12.0) {bearingToMan -= 12.0;}
    uint8_t hour = (uint8_t)(bearingToMan);
    uint8_t minute = (uint8_t)((bearingToMan - hour) * 60.0);
    hour %= 12;
    if (hour == 0) {hour = 12;}

    float d = cachedDistance;

    if (bearingToMan > 1.75  && bearingToMan < 10.25) {
        const char* street = nullptr;
        float dist = 0;
        // Find the appropriate street based on distance
        for (const auto& s : streets) {
            if (d > s.center - s.width) {
                street = s.name;
                dist = d - s.center;
            } else {
                break;
            }
        }
        if (street) {
            snprintf(addrStr, sizeof(addrStr), "%d:%02d & %s %dft", hour, minute, street, int(dist));
            return addrStr;
        }

    }

    snprintf(addrStr, sizeof(addrStr), "%d:%02d & %dft", hour, minute, (uint32_t)d);
    return addrStr;
}


static void drawBRCAddress(OLEDDisplay *display, int16_t x, int16_t y, const GPSStatus *gps)
{
    // Only draw if we have a valid GPS position
    if ((!gps->getIsConnected() || !gps->getHasLock()) && !config.position.fixed_position) {
        return; // Skip drawing - no valid position available
    }
    
    auto displayLine = BRCAddress(int32_t(gps->getLatitude()), int32_t(gps->getLongitude()));
    display->drawString(x + (display->getWidth() - (display->getStringWidth(displayLine))) / 2, y, displayLine);
}
