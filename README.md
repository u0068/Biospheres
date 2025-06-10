# Bio-Spheres CPP

A high-performance 3D cellular simulation using GPU compute shaders and instanced rendering in OpenGL. This project demonstrates real-time physics simulation of thousands of spherical cells with efficient GPU-based rendering.

## Features

### 🚀 GPU-Accelerated Physics
- **Compute Shaders**: Cell physics calculations run entirely on the GPU
- **Parallel Processing**: Thousands of cells simulated simultaneously
- **Real-time Updates**: Physics, collision detection, and position updates at 60+ FPS

### 🎨 Instanced Rendering
- **GPU Sphere Meshes**: Efficient instanced rendering of 3D spheres
- **Dynamic Lighting**: Phong shading with specular highlights
- **Depth-based Fog**: Distance-based atmospheric effects
- **Procedural Colors**: Unique colors generated per cell instance

### 🎮 Interactive Controls
- **Free Camera**: WASD movement, mouse look, Q/E roll
- **Real-time UI**: ImGui interface showing FPS, camera position, and controls
- **Demo Window**: Built-in ImGui demo for UI development

## Technical Architecture

### Core Components
- **CellManager**: Manages GPU buffers, compute shaders, and rendering pipeline
- **SphereMesh**: Generates and renders instanced sphere geometry
- **Camera**: 3D camera with perspective projection and smooth controls
- **Shader System**: Unified shader loading for vertex, fragment, and compute shaders

### GPU Pipeline
1. **Initialization**: Cell data uploaded to GPU buffer (SSBO)
2. **Physics Compute**: Parallel force calculations and acceleration updates
3. **Update Compute**: Position and velocity integration
4. **Instance Rendering**: Extract position/radius data for instanced sphere rendering
5. **Fragment Shading**: Per-pixel lighting and color calculation

### File Structure
```
├── main.cpp                 # Application entry point and main loop
├── cell_manager.*          # Cell simulation and GPU buffer management
├── sphere_mesh.*           # Instanced sphere geometry generation
├── camera.*                # 3D camera implementation
├── shader_class.*          # Shader compilation and uniform management
├── shaders/
│   ├── sphere.vert         # Instanced sphere vertex shader
│   ├── sphere.frag         # Sphere fragment shader with lighting
│   ├── cell_physics.comp   # Physics computation shader
│   └── cell_update.comp    # Position/velocity update shader
└── Libraries/              # External dependencies
```

## Dependencies

### Required Libraries
- **OpenGL 4.3+**: Core graphics API with compute shader support
- **GLFW**: Window management and input handling
- **GLAD**: OpenGL function loader
- **GLM**: Mathematics library for 3D transformations
- **ImGui**: Immediate mode GUI for debugging interface

### Build Requirements
- **Visual Studio 2019+** or compatible C++17 compiler
- **Windows 10+** (current configuration)
- **GPU**: OpenGL 4.3 compatible graphics card

## Building

### Visual Studio
1. Open `Biospheres 2.sln`
2. Set configuration to `Debug x64` or `Release x64`
3. Build → Build Solution (`Ctrl+Shift+B`)
4. Run with `F5` or `Ctrl+F5`

### Manual Build
```powershell
# Ensure all dependencies are in Libraries/ folder
# Compile with C++17 standard and link OpenGL libraries
```

## Controls

### Camera Movement
- **WASD**: Move forward/backward, strafe left/right
- **Space/C**: Move up/down
- **Q/E**: Roll left/right
- **Right Mouse + Drag**: Look around
- **Mouse Wheel**: Zoom (if implemented)

### UI
- **F1**: Toggle ImGui demo window
- **ESC**: Exit application

## Configuration

### Performance Tuning
- Adjust `MAX_CELLS` in `cell_manager.h` for cell count limits
- Modify sphere resolution in `SphereMesh::generateSphere()` calls
- Tune compute shader work group sizes for your GPU

### Visual Settings
- Light direction in `sphere.frag`
- Fog parameters in fragment shader
- Cell spawn radius and initial conditions in `CellManager::spawnCells()`

## Shader Details

### Compute Shaders
- **cell_physics.comp**: Calculates inter-cell forces and accelerations
- **cell_update.comp**: Integrates velocity and updates positions

### Rendering Shaders
- **sphere.vert**: Transforms sphere vertices with instance data
- **sphere.frag**: Implements Phong lighting model with fog effects

## Performance Notes

### Optimization Features
- **Instanced Rendering**: Single draw call for all spheres
- **GPU Compute**: Parallel physics calculations
- **Memory Barriers**: Proper synchronization between compute and render
- **Efficient Data Layout**: Aligned structures for GPU access

### Scalability
- Tested with 1000+ cells at 60+ FPS
- Performance scales with GPU compute capability
- Memory usage grows linearly with cell count

## Development Status

### Completed Features
✅ GPU compute pipeline  
✅ Instanced sphere rendering  
✅ Real-time physics simulation  
✅ Interactive camera system  
✅ ImGui debugging interface  

### Future Enhancements
🔄 Cell division and growth  
🔄 Spatial partitioning optimization  
🔄 Advanced lighting models  
🔄 Particle effects  
🔄 Save/load simulation states  

## Contributing

This project serves as a learning platform for GPU programming and real-time simulation. Feel free to experiment with:
- Different physics models
- Rendering techniques
- Compute shader optimizations
- UI improvements

## License

Educational/Personal use project. See individual library licenses for dependencies.

---

*"If you're a dev, please help me I have no idea what I'm doing"* - Original comment from main.cpp 😄

**Note**: You now have a much better idea of what you're doing! This project demonstrates advanced GPU programming concepts including compute shaders, instanced rendering, and real-time simulation.