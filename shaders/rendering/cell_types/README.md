# Cell Types Directory Structure

This directory contains all shader files organized by cell type. Each cell type has its own subdirectory containing all relevant shaders.

## Directory Structure

```
cell_types/
├── phagocyte/              # Default cell type
│   ├── sphere_distance_fade.vert
│   └── sphere_distance_fade.frag
│
└── flagellocyte/           # Motile cell with spiral tail
    ├── flagellocyte.vert   # Cell body vertex shader
    ├── flagellocyte.frag   # Cell body fragment shader
    ├── tail_generate.comp  # Compute shader for generating tail geometry
    ├── tail.vert           # Tail vertex shader
    ├── tail.frag           # Tail fragment shader
    └── settings.txt        # Global tail settings (auto-loaded on startup)
```

## Cell Type Descriptions

### Phagocyte
- **Type**: Basic spherical cell
- **Rendering**: Standard sphere mesh with distance-based fading
- **Shaders**: 
  - Vertex/Fragment shaders for LOD rendering
  - Distance-based alpha fading

### Flagellocyte
- **Type**: Motile cell with rotating spiral tail
- **Rendering**: Sphere body + procedurally generated volumetric tail
- **Features**:
  - Smooth spiral motion
  - Adjustable tail length, thickness, taper
  - Rounded end cap
  - Dynamic segment count (8-64)
  - Tail color automatically matches cell body
- **Shaders**:
  - Cell body: Standard vertex/fragment pipeline
  - Tail: Compute shader generates geometry, then rendered with vertex/fragment shaders
- **Settings**: Global settings stored in `settings.txt` (auto-loaded on startup)
  - Edit the file directly to customize tail appearance
  - Settings persist between program restarts

## Adding a New Cell Type

1. Create a new subdirectory under `cell_types/`
2. Add your shaders to the new directory
3. Update `src/simulation/cell/common_structs.h` to add the new cell type to the `CellType` enum
4. Add settings struct if needed
5. Update `src/simulation/cell/culling_system.cpp` to initialize shaders
6. Create UI settings file in `src/ui/cell_types/` if needed

## Shader Naming Conventions

- **Cell body shaders**: `[celltype].vert` and `[celltype].frag`
- **Special feature shaders**: `[feature]_[purpose].ext` (e.g., `tail_generate.comp`)
- Keep names descriptive and consistent
