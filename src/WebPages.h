/*
 * WebPages.h - HTML page generation for web interface
 */

#ifndef WEBPAGES_H
#define WEBPAGES_H

#include <Arduino.h>

// Common page styles
extern const char PAGE_STYLES[];

// Page generators
String generateActualsPage();
String generateSettingsPage();
String generateUpdatePage();

#endif // WEBPAGES_H
