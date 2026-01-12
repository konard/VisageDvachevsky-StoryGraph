#pragma once

#include "NovelMind/core/types.hpp"
#include <fstream>
#include <string>
#include <vector>

namespace NovelMind::VFS::detail {

inline constexpr u32 kPackMagic = 0x53524D4E;   // "NMRS"
inline constexpr u32 kFooterMagic = 0x46524D4E; // "NMRF"
inline constexpr u16 kPackVersionMajor = 1;
inline constexpr usize kResourceEntrySize = 48;
inline constexpr usize kFooterSize = 64;  // Updated to include SHA-256 (32 bytes) + CRC32 (4 bytes) + metadata
inline constexpr usize kGcmTagSize = 16;
inline constexpr u32 kPackFlagEncrypted = 1u << 0;
inline constexpr u32 kPackFlagCompressed = 1u << 1;
inline constexpr u32 kPackFlagSigned = 1u << 2;

bool readFileToString(std::ifstream &file, std::string &out);
bool readFileToBytes(std::ifstream &file, std::vector<u8> &out);

u32 updateCrc32(u32 crc, const u8 *data, usize size);

#ifndef NOVELMIND_HAS_OPENSSL
struct Sha256Context {
  u8 data[64];
  u32 datalen = 0;
  u64 bitlen = 0;
  u32 state[8] = {0};
};

void sha256Init(Sha256Context &ctx);
void sha256Update(Sha256Context &ctx, const u8 *data, usize len);
void sha256Final(Sha256Context &ctx, u8 hash[32]);
#endif

} // namespace NovelMind::VFS::detail
