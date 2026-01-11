# Pack Security and Trust Boundary

## Key Generation
- **Asymmetric (verification):** Build pipeline generates an RSA key pair.
  - Private key: kept only in the build/signing environment.
  - Public key: distributed with the runtime (embedded or provided via a trusted
    filesystem path).
- **Symmetric (encryption):** Build pipeline derives a 32-byte AES-256 key per
  project build. The key must be supplied to the runtime explicitly (never hard
  coded in source).

## Runtime Key Provisioning
- Public key (verification):
  - `NOVELMIND_PACK_PUBLIC_KEY` environment variable: path to PEM file.
  - Or inject PEM text via `MultiPackManager::setPackPublicKeyPem(...)`.
- AES key (decryption):
  - `NOVELMIND_PACK_AES_KEY_HEX`: hex-encoded key bytes (64 hex chars for
    AES-256).
  - `NOVELMIND_PACK_AES_KEY_FILE`: path to a binary key file.
  - Or inject bytes via `MultiPackManager::setPackDecryptionKey(...)` /
    `setPackDecryptionKeyFromFile(...)`.

The runtime **refuses** to mount encrypted packs without a decryption key or
signed packs without a public key.

## Trust Boundary
- Trust ends at the pack file boundary. Everything read from `.nmres` is
  untrusted until:
  1. Header/table/footer CRC is verified.
  2. Signature is verified with the trusted public key (when `SIGNED` flag is
     set).
  3. AES-GCM tag is validated during decryption (when `ENCRYPTED` flag is set).
- Key material is never logged. Environment-derived keys are loaded once at
  initialization and held in secure memory only.

## Secure Memory Handling
- All encryption keys are stored using `Core::SecureVector<u8>` which:
  - Automatically zeroes memory on deallocation to prevent exposure in memory
    dumps and core dumps.
  - Attempts to lock memory in physical RAM to prevent swapping to disk (where
    supported).
  - Uses platform-specific secure zeroing functions that compilers respect.
- Key memory is protected against:
  - Memory dumps
  - Debugger inspection
  - Cold boot attacks
  - Core dumps
  - Swap/hibernation file exposure
- This applies to all key storage locations: `BuildConfig`, `PackDecryptor`,
  `MultiPackManager`, and `SecurePackFileSystem`.

## Build Pipeline Requirements
- Sign every pack when the `SIGNED` flag is set; emit `<pack>.sig` alongside the
  pack.
- Encrypt resources with unique IVs and AES-GCM tags; store IV per resource as
  defined in `pack_file_format.md`.
- Provide the runtime with the corresponding public key and AES key through the
  mechanisms above.
