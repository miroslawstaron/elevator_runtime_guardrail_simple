# Project Structure

## Directory Organization

```
elevator_guardrail/
├── src/                          # Source code
│   ├── elevator.c               # Elevator state machine (297 lines)
│   ├── guardrails.c             # Safety guardrails (85 lines)
│   └── main.c                   # Example application
│
├── include/                      # Header files
│   ├── elevator.h               # Elevator API definition
│   └── guardrails.h             # Guardrails API definition
│
├── tests/                        # Test files
│   ├── test_simple.c            # 49 unit tests (no dependencies)
│   ├── test_elevator.c          # 28 elevator tests (CUnit)
│   ├── test_guardrails.c        # 21 guardrails tests (CUnit)
│   ├── test_runner.c            # CUnit test harness
│   └── test_framework.h         # Custom testing framework
│
├── docs/                         # Documentation
│   ├── README.md                # Project overview
│   ├── STRUCTURE.md             # This file - directory structure
│   ├── DESIGN_DIAGRAMS.md       # 12 mermaid diagrams
│   ├── DESIGN_PRESENTATION_README.md
│   ├── PROJECT_SUMMARY.md       # Complete project overview
│   ├── GUARDRAILS.md            # Safety mechanisms detailed guide
│   ├── STATE_MACHINE.md         # State machine detailed design
│   ├── DESIGN.md                # System design document
│   ├── TESTING.md               # Test coverage details
│   ├── QUICK_START.md           # Getting started guide
│   ├── TEST_FILES.md            # Test file documentation
│   ├── TEST_README.md           # Test installation guide
│   └── FILES.md                 # Original file organization
│
├── presentation/                 # Presentation files
│   └── Elevator_Design_Presentation.pptx (16 slides)
│
├── scripts/                      # Build and utility scripts
│   └── create_presentation.js   # PowerPoint generation script
│
├── build/                        # Build artifacts (generated)
│   └── *.o                      # Object files (created by make)
│
├── Makefile                      # GNU Make build configuration
├── CMakeLists.txt               # CMake build configuration
└── node_modules/                # Node.js dependencies (from pptx generation)

```

---

## File Purposes

### Source Code (src/)

| File | Purpose | Lines |
|------|---------|-------|
| `elevator.c` | State machine implementation with 11 states | 297 |
| `guardrails.c` | Safety violation detection & enforcement | 85 |
| `main.c` | Example application entry point | ~50 |

### Headers (include/)

| File | Purpose |
|------|---------|
| `elevator.h` | Elevator API: states, functions, structures |
| `guardrails.h` | Guardrails API: detection, enforcement |

### Tests (tests/)

| File | Purpose | Tests |
|------|---------|-------|
| `test_simple.c` | Main test suite (no dependencies) | 49 |
| `test_elevator.c` | Elevator-specific tests (CUnit) | 28 |
| `test_guardrails.c` | Guardrails-specific tests (CUnit) | 21 |
| `test_runner.c` | CUnit test harness | - |
| `test_framework.h` | Custom lightweight test framework | - |

### Documentation (docs/)

**Quick Start Documents:**
- `README.md` - Project overview
- `QUICK_START.md` - 5-minute guide
- `PROJECT_SUMMARY.md` - Complete summary

**Technical Deep-Dives:**
- `DESIGN_DIAGRAMS.md` - 12 mermaid diagrams
- `DESIGN.md` - System architecture
- `STATE_MACHINE.md` - State machine details
- `GUARDRAILS.md` - Safety mechanisms

**Implementation Guides:**
- `TESTING.md` - Test coverage
- `STRUCTURE.md` - Directory structure (this file)
- `TEST_FILES.md` - Test organization
- `TEST_README.md` - Installation guide

### Presentation (presentation/)

- `Elevator_Design_Presentation.pptx` - 16-slide professional presentation
  - Overview, architecture, guardrails, testing, conclusions

### Scripts (scripts/)

- `create_presentation.js` - Node.js script to generate PowerPoint
  - Uses pptxgenjs library
  - Configurable colors, fonts, layouts

---

## Build System

### Makefile
- **Location:** Root directory
- **Purpose:** GNU Make build automation
- **Features:**
  - Automatic dependency compilation
  - Separate build directory for object files
  - Include path configuration (-Iinclude)
  - Multiple build targets (test, run, clean)

### CMakeLists.txt
- **Location:** Root directory
- **Purpose:** CMake cross-platform build
- **Features:**
  - Alternative to Makefile
  - Better IDE integration
  - Optional CUnit support

### Build Output

```
build/
├── elevator.o          # Compiled elevator.c
├── guardrails.o        # Compiled guardrails.c
├── main.o              # Compiled main.c
├── test_simple.o       # Test main
├── test_elevator.o     # CUnit test
├── test_guardrails.o   # CUnit test
└── test_runner.o       # CUnit runner
```

Executables in root:
- `./elevator_guardrails` - Main program
- `./test_simple` - Simple test runner (no dependencies)
- `./test_elevator_guardrails` - CUnit test runner

---

## Getting Started

### 1. View Directory Structure
```bash
# List directories
ls -la

# Tree view (if available)
tree -L 2 -d
```

### 2. Build and Test
```bash
# Using make
make test           # Build and run all tests
make run            # Build and run main program
make clean          # Remove build artifacts

# Using CMake
mkdir build && cd build
cmake ..
cmake --build .
ctest
```

### 3. Explore Documentation
```bash
# Start with README
cat docs/README.md

# View project summary
cat docs/PROJECT_SUMMARY.md

# See design diagrams
cat docs/DESIGN_DIAGRAMS.md
```

### 4. Examine Code
```bash
# Look at elevator state machine
cat src/elevator.c

# Review safety mechanisms
cat src/guardrails.c

# Study tests
cat tests/test_simple.c
```

---

## Key Characteristics

### Organization Benefits

✅ **Separation of Concerns**
- Source code isolated in src/
- Headers in include/ for easy imports
- Tests separate from production code
- Documentation centralized

✅ **Build System Clarity**
- All objects in build/ directory
- Easy cleanup (remove build/)
- Include path explicitly set (-Iinclude)
- No object files in source tree

✅ **Easy Maintenance**
- Consistent file locations
- Clear dependencies
- Scalable structure for growth
- Professional project layout

✅ **Documentation Navigation**
- docs/ contains all guides
- Presentation files isolated
- Scripts easily accessible
- Build configuration visible

### Scalability

The structure supports adding:
- Additional source modules in `src/`
- New test suites in `tests/`
- More documentation in `docs/`
- Utilities in `scripts/`
- Binary outputs in `build/`

### Professional Standards

This structure follows:
- **Open Source Standards** - Linux/GNU project layout
- **Corporate Standards** - Enterprise development practices
- **C Best Practices** - Header/source separation
- **Build System Standards** - Makefile + CMake dual support

---

## Dependencies

### Runtime
- **C Standard Library** - No external dependencies (core)
- **Math Library** - `-lm` flag (for floating point)

### Build Tools
- **GCC** - C compiler
- **GNU Make** - Build automation (optional, CMake alternative available)
- **CMake** - Cross-platform build (optional, Makefile alternative available)

### Testing
- **CUnit** - Optional C unit testing framework
- **Custom Framework** - Lightweight built-in alternative

### Presentation
- **Node.js** - For generating PowerPoint
- **pptxgenjs** - PowerPoint generation library

---

## File Modifications

When modifying files:

### Changing Source Code
1. Edit file in `src/` or `include/`
2. Run `make clean && make test`
3. Verify all tests pass

### Adding New Source Files
1. Create in `src/`
2. Create corresponding header in `include/`
3. Update `Makefile` (add to source lists)
4. Update `CMakeLists.txt` (add to executable targets)

### Adding Tests
1. Create test file in `tests/`
2. Include `test_framework.h` or use CUnit
3. Update `Makefile` (add to test object lists)
4. Run `make test` to verify

### Updating Documentation
1. Edit files in `docs/`
2. Keep `README.md` as main entry point
3. Cross-reference from main README

---

## Version History

**v1.0 - Complete Refactor (Current)**
- Organized into src/, include/, tests/, docs/
- Updated Makefile with build/ directory
- Updated CMakeLists.txt for new structure
- All tests passing (49/49, 100%)
- Professional directory layout

**Previous**
- All files in root directory (unorganized)

---

## Related Documents

- [README.md](README.md) - Project overview
- [PROJECT_SUMMARY.md](PROJECT_SUMMARY.md) - Complete summary
- [Makefile](../Makefile) - Build configuration
- [CMakeLists.txt](../CMakeLists.txt) - CMake configuration

---

**Last Updated:** June 15, 2026  
**Status:** Complete & Production Ready ✅
