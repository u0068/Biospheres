# Version Management Guide

This project uses automatic version management for releases.

## Version Format

Versions follow **Semantic Versioning** (SemVer): `MAJOR.MINOR.PATCH`

- **MAJOR**: Breaking changes or major new features
- **MINOR**: New features, backward compatible
- **PATCH**: Bug fixes, backward compatible

### Alpha Status

While the MAJOR version is `0`, the project is in **Early Alpha**:
- Version format: `0.x.x-alpha`
- Example: `0.0.5-alpha`
- Indicates the project is in early development
- Breaking changes may occur between releases
- Once stable, version will move to `1.0.0`

## Files

- **`version.txt`**: Current version number (e.g., `1.0.0`)
- **`src/core/version.h`**: Auto-generated C++ header with version constants
- **`increment_version.ps1`**: Script to increment version
- **`package.ps1`**: Packaging script with optional version increment

## Usage

### Manual Version Increment

Increment the patch version (0.0.0 → 0.0.1):
```powershell
.\increment_version.ps1
```

Increment the minor version (0.0.5 → 0.1.0):
```powershell
.\increment_version.ps1 -IncrementType minor
```

Increment the major version (0.5.3 → 1.0.0 - exits alpha):
```powershell
.\increment_version.ps1 -IncrementType major
```

### Package with Version Increment

Package and increment patch version:
```powershell
.\package.ps1 -IncrementVersion
```

Package and increment minor version:
```powershell
.\package.ps1 -IncrementVersion -VersionType minor
```

Package and increment major version:
```powershell
.\package.ps1 -IncrementVersion -VersionType major
```

Package without incrementing:
```powershell
.\package.ps1
```

## Using Version in Code

Include the version header in your C++ code:

```cpp
#include "core/version.h"

// Access version information
std::cout << "Biospheres v" << Version::STRING << std::endl;
std::cout << "Built on " << Version::BUILD_DATE << " at " << Version::BUILD_TIME << std::endl;

// Individual components
int major = Version::MAJOR;
int minor = Version::MINOR;
int patch = Version::PATCH;
```

## Example: Display Version in UI

Add to your UI manager or main window:

```cpp
#include "core/version.h"

void UIManager::renderAboutWindow()
{
    ImGui::Begin("About Biospheres");
    ImGui::Text("Version: %s", Version::STRING);
    ImGui::Text("Build Date: %s", Version::BUILD_DATE);
    ImGui::Text("Build Time: %s", Version::BUILD_TIME);
    ImGui::End();
}
```

## Workflow

### For Bug Fixes (Patch Release - Alpha)
1. Fix the bug
2. Build in Release mode
3. Run: `.\package.ps1 -IncrementVersion`
4. Output: `Biospheres_v0.0.1-alpha_Release.zip`

### For New Features (Minor Release - Alpha)
1. Implement the feature
2. Build in Release mode
3. Run: `.\package.ps1 -IncrementVersion -VersionType minor`
4. Output: `Biospheres_v0.1.0-alpha_Release.zip`

### For First Stable Release (Exit Alpha)
1. Ensure project is stable and ready
2. Build in Release mode
3. Run: `.\package.ps1 -IncrementVersion -VersionType major`
4. Output: `Biospheres_v1.0.0_Release.zip` (no longer alpha)

## Output Files

After packaging with version 1.2.3:
- `Distribution/` - Uncompressed distribution folder
- `Biospheres_v1.2.3_Release.zip` - Compressed release archive

## Notes

- **`version.txt`** should be committed to version control
- **`src/core/version.h`** is auto-generated and can be in `.gitignore`
- Version increments are permanent - they update `version.txt`
- The ZIP filename includes the version for easy identification
