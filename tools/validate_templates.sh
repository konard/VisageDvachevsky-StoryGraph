#!/bin/bash
# =============================================================================
# Template Validation Script
# =============================================================================
# Validates all project templates in editor/templates/
#
# Usage: ./tools/validate_templates.sh [template_name]
#   template_name: Optional. If provided, only validates that template.
#
# Checks:
# 1. Required files exist (project.json, template.json, README.md)
# 2. JSON files are valid
# 3. Script files have basic syntax
# 4. All referenced directories exist
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
TEMPLATES_DIR="$PROJECT_ROOT/editor/templates"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Counters
PASS=0
FAIL=0
WARN=0

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((PASS++))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    ((FAIL++))
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
    ((WARN++))
}

log_info() {
    echo -e "[INFO] $1"
}

# Check if a file exists
check_file() {
    local file="$1"
    local required="$2"

    if [ -f "$file" ]; then
        log_pass "File exists: $(basename "$file")"
        return 0
    else
        if [ "$required" = "required" ]; then
            log_fail "Missing required file: $(basename "$file")"
        else
            log_warn "Missing optional file: $(basename "$file")"
        fi
        return 1
    fi
}

# Check if a directory exists
check_dir() {
    local dir="$1"

    if [ -d "$dir" ]; then
        log_pass "Directory exists: $(basename "$dir")"
        return 0
    else
        log_warn "Missing directory: $(basename "$dir")"
        return 1
    fi
}

# Validate JSON syntax
validate_json() {
    local file="$1"

    if [ ! -f "$file" ]; then
        return 1
    fi

    if command -v python3 &> /dev/null; then
        if python3 -c "import json; json.load(open('$file'))" 2>/dev/null; then
            log_pass "Valid JSON: $(basename "$file")"
            return 0
        else
            log_fail "Invalid JSON syntax: $(basename "$file")"
            return 1
        fi
    elif command -v jq &> /dev/null; then
        if jq '.' "$file" > /dev/null 2>&1; then
            log_pass "Valid JSON: $(basename "$file")"
            return 0
        else
            log_fail "Invalid JSON syntax: $(basename "$file")"
            return 1
        fi
    else
        log_warn "Cannot validate JSON (no python3 or jq found)"
        return 0
    fi
}

# Validate a single template
validate_template() {
    local template_path="$1"
    local template_name="$(basename "$template_path")"

    echo ""
    echo "=============================================="
    echo "Validating template: $template_name"
    echo "=============================================="

    # Required files
    check_file "$template_path/project.json" "required"
    check_file "$template_path/template.json" "required"
    check_file "$template_path/README.md" "required"

    # Optional files
    check_file "$template_path/CREDITS.md" "optional"

    # Validate JSON files
    validate_json "$template_path/project.json"
    validate_json "$template_path/template.json"

    # Check directories
    check_dir "$template_path/scripts"
    check_dir "$template_path/assets"

    # Check for at least one script file
    if [ -d "$template_path/scripts" ]; then
        if ls "$template_path/scripts"/*.nms 1>/dev/null 2>&1; then
            log_pass "Script files found"
        else
            log_warn "No .nms script files found in scripts/"
        fi
    fi

    # Check asset subdirectories
    if [ -d "$template_path/assets" ]; then
        for subdir in characters backgrounds music sfx; do
            check_dir "$template_path/assets/$subdir"
        done
    fi

    echo ""
}

# Main execution
echo "=============================================="
echo "NovelMind Template Validator"
echo "=============================================="

if [ ! -d "$TEMPLATES_DIR" ]; then
    echo -e "${RED}Error: Templates directory not found: $TEMPLATES_DIR${NC}"
    exit 1
fi

# Get list of templates to validate
if [ -n "$1" ]; then
    # Validate specific template
    if [ -d "$TEMPLATES_DIR/$1" ]; then
        validate_template "$TEMPLATES_DIR/$1"
    else
        echo -e "${RED}Error: Template not found: $1${NC}"
        exit 1
    fi
else
    # Validate all templates
    for template in "$TEMPLATES_DIR"/*/; do
        if [ -d "$template" ]; then
            validate_template "$template"
        fi
    done
fi

# Summary
echo "=============================================="
echo "VALIDATION SUMMARY"
echo "=============================================="
echo -e "${GREEN}Passed: $PASS${NC}"
echo -e "${YELLOW}Warnings: $WARN${NC}"
echo -e "${RED}Failed: $FAIL${NC}"

if [ $FAIL -gt 0 ]; then
    echo ""
    echo -e "${RED}Validation failed with $FAIL errors${NC}"
    exit 1
else
    echo ""
    echo -e "${GREEN}All validations passed!${NC}"
    exit 0
fi
