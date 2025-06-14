# Structure of Arrays (SoA) Refactoring - Biospheres 2

## Overview
This refactoring converts the cell simulation from Array of Structures (AoS) to Structure of Arrays (SoA) layout for improved GPU performance.

## Benefits of SoA over AoS

### Memory Access Patterns
- **AoS (Before)**: `[pos,vel,acc][pos,vel,acc][pos,vel,acc]...`
- **SoA (After)**: `[pos,pos,pos...][vel,vel,vel...][acc,acc,acc...]`

### GPU Performance Improvements
1. **Better Cache Utilization**: When compute shaders process only positions (common in physics), they access contiguous memory
2. **Reduced Memory Bandwidth**: No need to load unused data (velocity/acceleration) when only processing positions
3. **Vectorization**: GPU can more efficiently process arrays of the same data type
4. **Coalesced Memory Access**: Threads in a warp access consecutive memory locations

### Specific Use Cases
- **Physics Computation**: Only needs positions, masses, and accelerations
- **Rendering**: Only needs positions and radii for instance data
- **Collision Detection**: Primarily uses positions and radii

## Implementation Details

### Data Structure Changes
```cpp
// Old AoS approach
struct ComputeCell {
    glm::vec4 positionAndRadius;  // x, y, z, radius
    glm::vec4 velocityAndMass;    // vx, vy, vz, mass  
    glm::vec4 acceleration;       // ax, ay, az, unused
};
std::vector<ComputeCell> cells;

// New SoA approach
struct CellDataSoA {
    std::vector<glm::vec3> positions;
    std::vector<float> radii;
    std::vector<glm::vec3> velocities;
    std::vector<float> masses;
    std::vector<glm::vec3> accelerations;
};
```

### GPU Buffer Layout
```cpp
// Separate buffers for each property
GLuint positionBuffer;     // vec3 array
GLuint radiusBuffer;       // float array
GLuint velocityBuffer;     // vec3 array
GLuint massBuffer;         // float array
GLuint accelerationBuffer; // vec3 array
```

### Compute Shader Updates
The compute shaders now bind to separate SSBO binding points:
```glsl
layout(std430, binding = 0) buffer PositionBuffer { vec3 positions[]; };
layout(std430, binding = 1) buffer RadiusBuffer { float radii[]; };
layout(std430, binding = 2) buffer VelocityBuffer { vec3 velocities[]; };
// etc...
```

## Performance Expected Improvements

### Memory Bandwidth Reduction
- **Physics Pass**: ~33% reduction (only positions, masses, accelerations)
- **Rendering Pass**: ~66% reduction (only positions, radii)
- **Collision Detection**: ~50% reduction (positions, radii)

### Cache Efficiency
- **L1 Cache**: Better utilization due to accessing same data types consecutively
- **Memory Coalescing**: GPU threads access consecutive memory locations

## Backward Compatibility
Legacy AoS interface maintained through conversion functions:
- `getAoSCell(index)` - converts SoA data to legacy ComputeCell format
- `updateCellData(index, cell)` - accepts legacy format, converts to SoA
- Staging buffer still accepts ComputeCell for incremental additions

## Migration Notes
1. **Compute Shaders**: Updated to use separate SSBO bindings
2. **Memory Management**: Individual buffer allocation/deallocation
3. **Data Access**: Helper functions for converting between formats
4. **UI Components**: Updated to work with SoA accessors

This refactoring should provide significant performance improvements, especially when processing large numbers of cells (10,000+) on GPU compute shaders.
