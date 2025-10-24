# Metaball Baker Tool

A standalone application for baking metaball geometry variations used by the Biospheres runtime renderer.

## Overview

The Baker Tool generates pre-computed mesh geometry from metaball parameter combinations using raymarching and marching cubes algorithms. This offline baking process enables high-performance runtime rendering of thousands of cellular organisms with organic connections.

## Features

- **Real-time Preview**: Interactive raymarching preview with Space Engineers-style camera controls
- **Parameter Configuration**: Configurable size ratios and distance ratios for mesh variations
- **Batch Baking**: Generate complete parameter grids in a single operation
- **Binary Output**: Efficient binary mesh format optimized for runtime loading
- **ImGui Interface**: Intuitive graphical interface for configuration and control

## Building

The Baker Tool is included in the main Biospheres Visual Studio solution:

1. Open `Biospheres.sln` in Visual Studio 2022
2. Set `BakerTool` as the startup project
3. Build and run (Ctrl+F5)

### Requirements

- Visual Studio 2022 with C++20 support
- OpenGL 4.6 compatible graphics card
- Same dependencies as main Biospheres project (GLFW, GLAD, GLM, ImGui)

## Usage

### Basic Workflow

1. **Configure Parameters**: Set up size ratios and distance ratios in the Configuration panel
2. **Preview**: Use the Parameter Controls to preview different metaball configurations
3. **Adjust Quality**: Set resolution (32³, 64³, or 128³) based on quality vs. speed requirements
4. **Set Output Directory**: Choose where to save the generated mesh files
5. **Bake**: Click "Bake Parameter Grid" to generate all mesh variations

### Camera Controls

- **Right Mouse + Drag**: Rotate camera (cursor hidden during drag)
- **WASD**: Move forward/back/left/right relative to camera
- **Space/C**: Move up/down relative to camera
- **Q/E**: Roll camera around forward axis

### Configuration

#### Size Ratios
- Ratio of larger cell radius to smaller cell radius
- Valid range: 1.0 to 10.0
- Default: [1.0, 1.5, 2.0, 3.0]
- Example: 2.0 means one cell is twice the radius of the other

#### Distance Ratios
- Distance between cell centers divided by combined radius
- Valid range: 0.1 to 3.0
- Default: [0.5, 1.0, 2.0]
- Example: 1.0 means cells are touching, 2.0 means separated

#### Resolution
- Density field resolution for marching cubes
- Options: 32³, 64³, 128³
- Higher resolution = better quality but slower baking

### Output Format

Generated mesh files use the naming convention:
```
bridge_r{sizeRatio}_d{distanceRatio}.mesh
```

Examples:
- `bridge_r1.0_d0.5.mesh` - Equal size cells, close together
- `bridge_r2.0_d1.0.mesh` - 2:1 size ratio, touching
- `bridge_r3.0_d2.0.mesh` - 3:1 size ratio, separated

## File Structure

```
tools/baker/
├── main.cpp                 # Application entry point
├── baker_application.h/cpp  # Main application class
├── baker_ui.h/cpp          # ImGui interface
├── preview_renderer.h/cpp   # Real-time raymarching preview
├── camera_controller.h/cpp  # Space Engineers-style camera
├── mesh_baker.h/cpp        # Core baking engine
├── shader.h/cpp            # Shader management
├── BakerTool.vcxproj       # Visual Studio project
└── README.md               # This file
```

## Integration with Runtime

The baked mesh files are loaded by the runtime `BridgeRenderingSystem`:

1. **Startup**: Runtime scans mesh directory and loads all variations
2. **Selection**: For each cell pair, calculate size ratio and distance ratio
3. **Interpolation**: Find 4 nearest mesh variations and blend in vertex shader
4. **Rendering**: Use instanced rendering for thousands of bridges

## Performance

### Baking Performance
- **32³ resolution**: ~0.1s per mesh
- **64³ resolution**: ~0.5s per mesh  
- **128³ resolution**: ~2.0s per mesh

### Example Baking Times
- 4 size ratios × 3 distance ratios = 12 meshes
- At 64³ resolution: ~6 seconds total
- At 128³ resolution: ~24 seconds total

### Memory Usage
- **32³**: ~128 KB per mesh
- **64³**: ~1 MB per mesh
- **128³**: ~8 MB per mesh

## Troubleshooting

### Common Issues

1. **Shader Compilation Errors**
   - Ensure OpenGL 4.6 support
   - Check shader file paths are correct
   - Verify GLSL version compatibility

2. **Missing Output Directory**
   - Create output directory before baking
   - Ensure write permissions
   - Use absolute paths if relative paths fail

3. **Invalid Parameters**
   - Size ratios must be ≥ 1.0
   - Distance ratios must be > 0.1
   - Remove duplicate values

4. **Performance Issues**
   - Lower resolution for faster baking
   - Reduce parameter count
   - Close other GPU-intensive applications

### Debug Output

The Baker Tool outputs progress information to the console:
- Initialization status
- Baking progress for each mesh
- File write confirmations
- Error messages with details

## Future Enhancements

- **Parallel Baking**: Multi-threaded mesh generation
- **Mesh Optimization**: Automatic vertex welding and decimation
- **Quality Metrics**: Analysis of mesh quality and statistics
- **Batch Export**: Multiple animation states in one operation
- **Preview Comparison**: Side-by-side parameter comparison

## Technical Details

### SDF Evaluation
The metaball geometry is defined using signed distance fields:
- Two spheres positioned based on distance ratio
- Soft-minimum blending for organic connections
- Raymarching for real-time preview

### Marching Cubes
Mesh extraction from density field:
- Regular 3D grid sampling
- Triangle generation at isosurface
- Normal calculation from gradient
- Vertex optimization and welding

### Binary Format
Efficient mesh storage:
- Header with metadata
- Vertex positions (vec3)
- Vertex normals (vec3)  
- Triangle indices (uint32)
- Little-endian format for Windows compatibility