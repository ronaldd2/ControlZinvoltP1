/*
 * Version.h - Firmware version information
 * Auto-generated from build date/time
 */

#ifndef VERSION_H
#define VERSION_H

// Build timestamp from compiler macros
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

// Format: "YYYY-MM-DD HH:MM"
inline const char* getFirmwareVersion() {
  static char version[32];
  snprintf(version, sizeof(version), "%s %s", BUILD_DATE, BUILD_TIME);
  return version;
} 

#endif // VERSION_H
