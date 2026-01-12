#include "pack_security_detail.hpp"

#include <algorithm>

namespace NovelMind::VFS::detail {

namespace {

constexpr u32 kCrc32Table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD706B3, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D};

#ifndef NOVELMIND_HAS_OPENSSL
constexpr u32 kSha256K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline u32 sha256_rotr(u32 x, u32 n) {
  return (x >> n) | (x << (32 - n));
}

void sha256Transform(Sha256Context& ctx, const u8 data[]) {
  u32 m[64];
  for (u32 i = 0; i < 16; ++i) {
    m[i] = (static_cast<u32>(data[i * 4]) << 24) | (static_cast<u32>(data[i * 4 + 1]) << 16) |
           (static_cast<u32>(data[i * 4 + 2]) << 8) | (static_cast<u32>(data[i * 4 + 3]));
  }
  for (u32 i = 16; i < 64; ++i) {
    u32 s0 = sha256_rotr(m[i - 15], 7) ^ sha256_rotr(m[i - 15], 18) ^ (m[i - 15] >> 3);
    u32 s1 = sha256_rotr(m[i - 2], 17) ^ sha256_rotr(m[i - 2], 19) ^ (m[i - 2] >> 10);
    m[i] = m[i - 16] + s0 + m[i - 7] + s1;
  }

  u32 a = ctx.state[0];
  u32 b = ctx.state[1];
  u32 c = ctx.state[2];
  u32 d = ctx.state[3];
  u32 e = ctx.state[4];
  u32 f = ctx.state[5];
  u32 g = ctx.state[6];
  u32 h = ctx.state[7];

  for (u32 i = 0; i < 64; ++i) {
    u32 s1 = sha256_rotr(e, 6) ^ sha256_rotr(e, 11) ^ sha256_rotr(e, 25);
    u32 ch = (e & f) ^ (~e & g);
    u32 temp1 = h + s1 + ch + kSha256K[i] + m[i];
    u32 s0 = sha256_rotr(a, 2) ^ sha256_rotr(a, 13) ^ sha256_rotr(a, 22);
    u32 maj = (a & b) ^ (a & c) ^ (b & c);
    u32 temp2 = s0 + maj;

    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;
  }

  ctx.state[0] += a;
  ctx.state[1] += b;
  ctx.state[2] += c;
  ctx.state[3] += d;
  ctx.state[4] += e;
  ctx.state[5] += f;
  ctx.state[6] += g;
  ctx.state[7] += h;
}
#endif

} // namespace

bool readFileToString(std::ifstream& file, std::string& out) {
  file.seekg(0, std::ios::end);
  const std::streampos size = file.tellg();
  if (size < 0) {
    return false;
  }

  out.resize(static_cast<size_t>(size));
  file.seekg(0, std::ios::beg);
  if (!out.empty()) {
    file.read(out.data(), static_cast<std::streamsize>(out.size()));
    if (!file) {
      return false;
    }
  }

  return true;
}

bool readFileToBytes(std::ifstream& file, std::vector<u8>& out) {
  file.seekg(0, std::ios::end);
  const std::streampos size = file.tellg();
  if (size < 0) {
    return false;
  }

  out.resize(static_cast<size_t>(size));
  file.seekg(0, std::ios::beg);
  if (!out.empty()) {
    file.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
    if (!file) {
      return false;
    }
  }

  return true;
}

u32 updateCrc32(u32 crc, const u8* data, usize size) {
  for (usize i = 0; i < size; ++i) {
    crc = kCrc32Table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
  }
  return crc;
}

#ifndef NOVELMIND_HAS_OPENSSL
void sha256Init(Sha256Context& ctx) {
  ctx.datalen = 0;
  ctx.bitlen = 0;
  ctx.state[0] = 0x6a09e667;
  ctx.state[1] = 0xbb67ae85;
  ctx.state[2] = 0x3c6ef372;
  ctx.state[3] = 0xa54ff53a;
  ctx.state[4] = 0x510e527f;
  ctx.state[5] = 0x9b05688c;
  ctx.state[6] = 0x1f83d9ab;
  ctx.state[7] = 0x5be0cd19;
}

void sha256Update(Sha256Context& ctx, const u8* data, usize len) {
  for (usize i = 0; i < len; ++i) {
    ctx.data[ctx.datalen] = data[i];
    ctx.datalen++;
    if (ctx.datalen == 64) {
      sha256Transform(ctx, ctx.data);
      ctx.bitlen += 512;
      ctx.datalen = 0;
    }
  }
}

void sha256Final(Sha256Context& ctx, u8 hash[32]) {
  u32 i = ctx.datalen;

  if (ctx.datalen < 56) {
    ctx.data[i++] = 0x80;
    while (i < 56) {
      ctx.data[i++] = 0x00;
    }
  } else {
    ctx.data[i++] = 0x80;
    while (i < 64) {
      ctx.data[i++] = 0x00;
    }
    sha256Transform(ctx, ctx.data);
    std::fill(std::begin(ctx.data), std::end(ctx.data), static_cast<u8>(0));
  }

  ctx.bitlen += ctx.datalen * 8ULL;
  ctx.data[63] = static_cast<u8>(ctx.bitlen);
  ctx.data[62] = static_cast<u8>(ctx.bitlen >> 8);
  ctx.data[61] = static_cast<u8>(ctx.bitlen >> 16);
  ctx.data[60] = static_cast<u8>(ctx.bitlen >> 24);
  ctx.data[59] = static_cast<u8>(ctx.bitlen >> 32);
  ctx.data[58] = static_cast<u8>(ctx.bitlen >> 40);
  ctx.data[57] = static_cast<u8>(ctx.bitlen >> 48);
  ctx.data[56] = static_cast<u8>(ctx.bitlen >> 56);
  sha256Transform(ctx, ctx.data);

  for (i = 0; i < 4; ++i) {
    hash[i] = static_cast<u8>((ctx.state[0] >> (24 - i * 8)) & 0xFF);
    hash[i + 4] = static_cast<u8>((ctx.state[1] >> (24 - i * 8)) & 0xFF);
    hash[i + 8] = static_cast<u8>((ctx.state[2] >> (24 - i * 8)) & 0xFF);
    hash[i + 12] = static_cast<u8>((ctx.state[3] >> (24 - i * 8)) & 0xFF);
    hash[i + 16] = static_cast<u8>((ctx.state[4] >> (24 - i * 8)) & 0xFF);
    hash[i + 20] = static_cast<u8>((ctx.state[5] >> (24 - i * 8)) & 0xFF);
    hash[i + 24] = static_cast<u8>((ctx.state[6] >> (24 - i * 8)) & 0xFF);
    hash[i + 28] = static_cast<u8>((ctx.state[7] >> (24 - i * 8)) & 0xFF);
  }
}
#endif

} // namespace NovelMind::VFS::detail
