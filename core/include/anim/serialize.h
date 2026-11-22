// .celbox document package.
//
// A package is a set of named files (a directory on disk, an NSFileWrapper on
// Apple platforms, a zip for sharing):
//   document.json          model: settings, palette, layers, symbols, cameras, audio
//   drawings/<id>.json     one vector drawing per file
//   drawings/<id>.raster   one raster drawing per file (tiles, uncompressed for now)
// One file per drawing lets autosave rewrite only what changed.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "anim/document.h"

namespace anim::io {

constexpr int kFormatVersion = 1;

using Bytes = std::vector<uint8_t>;
using Package = std::map<std::string, Bytes>;

Package save(const Document& doc);

// Returns false and fills `error` on malformed input. A newer major version
// than this build understands is rejected rather than half-loaded.
bool load(const Package& pkg, Document& out, std::string* error = nullptr);

// Disk helpers (directory form of the package).
bool writeDirectory(const Package& pkg, const std::string& dir, std::string* error = nullptr);
bool readDirectory(const std::string& dir, Package& out, std::string* error = nullptr);

}  // namespace anim::io
