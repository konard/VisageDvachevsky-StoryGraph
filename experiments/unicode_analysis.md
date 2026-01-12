# Unicode Identifier Range Analysis

## Current Implementation (lexer.cpp:99-151)

### ID_Start ranges currently implemented:
1. ASCII letters: `A-Z`, `a-z` (0x0041-0x005A, 0x0061-0x007A)
2. Latin Extended-A, Extended-B, Extended Additional: 0x00C0-0x024F
3. Cyrillic: 0x0400-0x04FF
4. Cyrillic Supplement: 0x0500-0x052F
5. Greek: 0x0370-0x03FF
6. CJK Unified Ideographs: 0x4E00-0x9FFF
7. Hiragana: 0x3040-0x309F
8. Katakana: 0x30A0-0x30FF
9. Korean Hangul: 0xAC00-0xD7AF
10. Arabic: 0x0600-0x06FF
11. Hebrew: 0x0590-0x05FF

### ID_Continue additions currently implemented:
1. ASCII digits: `0-9` (0x0030-0x0039)
2. Unicode combining marks: 0x0300-0x036F

## Missing Unicode Ranges

According to UAX #31 and common programming language implementations, the following important ranges are missing:

### Missing ID_Start ranges:

1. **Thai** (0x0E00-0x0E7F) - Thai alphabet
2. **Lao** (0x0E80-0x0EFF) - Lao alphabet
3. **Tibetan** (0x0F00-0x0FFF) - Tibetan script
4. **Georgian** (0x10A0-0x10FF) - Georgian alphabet
5. **Devanagari** (0x0900-0x097F) - Hindi, Sanskrit, etc.
6. **Bengali** (0x0980-0x09FF) - Bengali script
7. **Gurmukhi** (0x0A00-0x0A7F) - Punjabi script
8. **Gujarati** (0x0A80-0x0AFF) - Gujarati script
9. **Oriya** (0x0B00-0x0B7F) - Oriya script
10. **Tamil** (0x0B80-0x0BFF) - Tamil script
11. **Telugu** (0x0C00-0x0C7F) - Telugu script
12. **Kannada** (0x0C80-0x0CFF) - Kannada script
13. **Malayalam** (0x0D00-0x0D7F) - Malayalam script
14. **Sinhala** (0x0D80-0x0DFF) - Sinhala script
15. **Armenian** (0x0530-0x058F) - Armenian alphabet
16. **Ethiopic** (0x1200-0x137F) - Ethiopic script
17. **Cherokee** (0x13A0-0x13FF) - Cherokee syllabary
18. **Canadian Aboriginal** (0x1400-0x167F) - Unified Canadian Aboriginal Syllabics
19. **Ogham** (0x1680-0x169F) - Ogham script
20. **Runic** (0x16A0-0x16FF) - Runic alphabet
21. **Tagalog** (0x1700-0x171F) - Tagalog/Baybayin script
22. **Hanunoo** (0x1720-0x173F) - Hanunoo script
23. **Buhid** (0x1740-0x175F) - Buhid script
24. **Tagbanwa** (0x1760-0x177F) - Tagbanwa script
25. **Khmer** (0x1780-0x17FF) - Khmer/Cambodian script
26. **Mongolian** (0x1800-0x18AF) - Mongolian script
27. **Latin Extended Additional** (0x1E00-0x1EFF) - Additional Latin characters (partially covered)
28. **Greek Extended** (0x1F00-0x1FFF) - Extended Greek characters
29. **Cyrillic Extended-A** (0x2DE0-0x2DFF)
30. **Cyrillic Extended-B** (0xA640-0xA69F)
31. **Arabic Supplement** (0x0750-0x077F)
32. **Arabic Extended-A** (0x08A0-0x08FF)
33. **Arabic Presentation Forms-A** (0xFB50-0xFDFF)
34. **Arabic Presentation Forms-B** (0xFE70-0xFEFF)
35. **Hebrew Presentation Forms** (0xFB1D-0xFB4F)
36. **CJK Compatibility Ideographs** (0xF900-0xFAFF)
37. **CJK Extension A** (0x3400-0x4DBF)
38. **Yi Syllables** (0xA000-0xA48F)
39. **Yi Radicals** (0xA490-0xA4CF)
40. **Bopomofo** (0x3100-0x312F) - Chinese phonetic symbols
41. **Bopomofo Extended** (0x31A0-0x31BF)
42. **Hangul Jamo** (0x1100-0x11FF) - Korean alphabet components
43. **Hangul Compatibility Jamo** (0x3130-0x318F)
44. **Hangul Jamo Extended-A** (0xA960-0xA97F)
45. **Hangul Jamo Extended-B** (0xD7B0-0xD7FF)

### Missing ID_Continue ranges:

1. **Connector Punctuation** (Pc category):
   - Underscore `_` is already ASCII (0x005F) ✓
   - Low Line: 0x005F (already covered)
   - Katakana-Hiragana Prolonged Sound Mark: 0x30FC
   - Other connector punctuation: 0x203F-0x2040, 0x2054, 0xFE33-0xFE34, 0xFE4D-0xFE4F, 0xFF3F

2. **Decimal Numbers** (Nd category):
   - ASCII digits already covered ✓
   - Arabic-Indic digits: 0x0660-0x0669
   - Extended Arabic-Indic: 0x06F0-0x06F9
   - Devanagari digits: 0x0966-0x096F
   - Bengali digits: 0x09E6-0x09EF
   - And many more script-specific digit ranges

3. **Combining Marks** (Mn, Mc categories):
   - Currently only 0x0300-0x036F is covered
   - Need to add script-specific combining marks for all supported scripts

## Recommendation

The implementation should be updated to include at minimum:
1. Common South Asian scripts (Devanagari, Tamil, Telugu, etc.)
2. Thai and other Southeast Asian scripts
3. Armenian and Georgian
4. Additional Arabic and Hebrew ranges
5. Extended Cyrillic ranges
6. CJK extensions
7. Additional combining marks for supported scripts
8. Connector punctuation beyond underscore
9. Script-specific digit ranges

This will ensure proper Unicode identifier support as per UAX #31 specification.
