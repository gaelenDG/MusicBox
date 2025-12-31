#ifndef TAG_PARSER_H
#define TAG_PARSER_H

#include <Arduino.h>
#include <string>

struct TagCommand {
  uint16_t folder;      // e.g., 07
  uint16_t track;       // always 1 (first track in folder)
  int volume;           // -1 if not specified, otherwise 0–30
  bool shuffle;         // true if "shuffle" keyword found
  bool valid;           // true if parsing succeeded
};

/**
 * Parse a tag payload string like "07 | volume 5 | shuffle"
 * Folder is required; track always defaults to 1.
 * Returns TagCommand with folder, volume, and shuffle flags.
 */
TagCommand parseTagPayload(const String& payload);

#endif