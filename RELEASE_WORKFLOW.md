# Release Workflow

Complete guide for creating releases of Biospheres.

## Quick Release

For a standard patch release (bug fixes):

```powershell
# 1. Build in Release mode (Ctrl+Shift+B in Visual Studio)
# 2. Run packaging script with version increment
.\package.ps1 -IncrementVersion
```

Output: `Biospheres_v1.0.1_Release.zip`

## Detailed Steps

### 1. Prepare Your Code

- Commit all changes to version control
- Test thoroughly in Debug mode
- Fix any bugs or issues

### 2. Build Release Version

In Visual Studio:
1. Set configuration to **Release**
2. Set platform to **x64**
3. Build → Build Solution (Ctrl+Shift+B)
4. Verify no build errors

### 3. Choose Version Increment Type

**Patch (1.0.0 → 1.0.1)** - Bug fixes only:
```powershell
.\package.ps1 -IncrementVersion
```

**Minor (1.0.5 → 1.1.0)** - New features, backward compatible:
```powershell
.\package.ps1 -IncrementVersion -VersionType minor
```

**Major (1.5.3 → 2.0.0)** - Breaking changes:
```powershell
.\package.ps1 -IncrementVersion -VersionType major
```

### 4. Test the Distribution

1. Navigate to the `Distribution/` folder
2. Run `Biospheres.exe` to verify it works
3. Test all major features
4. Check that version appears in window title

### 5. Share the Release

The ZIP file is ready to share:
- `Biospheres_v1.0.1_Release.zip`

Upload to:
- GitHub Releases
- Your website
- Distribution platform

## Version Increment Without Packaging

If you just want to increment the version without packaging:

```powershell
.\increment_version.ps1                    # Patch
.\increment_version.ps1 -IncrementType minor    # Minor
.\increment_version.ps1 -IncrementType major    # Major
```

## Package Without Version Increment

If you already incremented the version and just want to repackage:

```powershell
.\package.ps1
```

## Files Generated

After running the package script:

```
Biospheres/
├── Distribution/              # Uncompressed distribution
│   ├── Biospheres.exe
│   ├── shaders/
│   └── genomes/
├── Biospheres_v1.0.1_Release.zip  # Compressed release
├── version.txt                # Current version (1.0.1)
└── src/core/version.h         # Auto-generated version header
```

## Troubleshooting

### Build Not Found Error

**Error:** `Build not found at x64\Release`

**Solution:** Build the project in Release mode first
- Visual Studio: Set to Release, then Build → Build Solution

### Version Not Updating in Window Title

**Solution:** Rebuild the project after incrementing version
- The version is compiled into the executable

### Shaders Not Included

**Solution:** Ensure `shaders/` folder exists in project root
- The packaging script copies from the project directory

## Best Practices

1. **Always test before releasing**
   - Run the executable from Distribution folder
   - Test on a clean system if possible

2. **Keep version.txt in version control**
   - Commit after each version increment
   - Helps track release history

3. **Tag releases in Git**
   ```bash
   git tag -a v1.0.1 -m "Release version 1.0.1"
   git push origin v1.0.1
   ```

4. **Write release notes**
   - Document what changed
   - List new features and bug fixes

## Example: Complete Release Process

```powershell
# 1. Build in Release mode (in Visual Studio)

# 2. Package and increment version
.\package.ps1 -IncrementVersion -VersionType minor

# 3. Test the distribution
cd Distribution
.\Biospheres.exe
cd ..

# 4. Commit version change
git add version.txt src/core/version.h
git commit -m "Bump version to 1.1.0"

# 5. Tag the release
git tag -a v1.1.0 -m "Release version 1.1.0"

# 6. Push to repository
git push origin main
git push origin v1.1.0

# 7. Upload Biospheres_v1.1.0_Release.zip to GitHub Releases
```

## Automation (Optional)

For frequent releases, consider creating a batch file:

**`release.bat`:**
```batch
@echo off
echo Building Release...
msbuild Biospheres.sln /p:Configuration=Release /p:Platform=x64 /t:Build /m

echo Packaging...
powershell -ExecutionPolicy Bypass -File package.ps1 -IncrementVersion

echo Done! Check Distribution folder and ZIP file.
pause
```

Run with: `release.bat`
