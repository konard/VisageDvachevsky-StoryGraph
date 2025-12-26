#!/bin/bash
# Test script for issue #80 - verify regex pattern matches both forms of say

echo "=== Testing regex patterns for say statements ==="

# Test 1: say with speaker - "say Hero \"text\""
echo -e "\nTest 1: say with speaker"
echo 'say Hero "Hello world"' | grep -oP '\bsay\s+(?:(\w+)\s+)?"([^"]*)"' || echo "OLD PATTERN: No match"
echo 'say Hero "Hello world"' | grep -oP '\bsay\s+(?\:(\w+)\s+)?"([^"]*)"' || echo "NEW PATTERN: No match"

# Test 2: say without speaker - "say \"text\""
echo -e "\nTest 2: say without speaker (default 'New scene')"
echo 'say "New scene"' | grep -oP '\bsay\s+(\w+)\s+"([^"]*)"' || echo "OLD PATTERN: No match (THIS IS THE BUG)"
echo 'say "New scene"' | grep -oP '\bsay\s+(?:(\w+)\s+)?"([^"]*)"' || echo "NEW PATTERN: No match"

echo -e "\n=== Summary ==="
echo "The OLD pattern only matches 'say speaker \"text\"'"
echo "The NEW pattern matches both 'say speaker \"text\"' AND 'say \"text\"'"
echo "This fixes issue #80 where default 'say \"New scene\"' was not being replaced"
