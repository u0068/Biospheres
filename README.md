# Bio-Spheres

A high-performance 3D cellular simulation using GPU compute shaders and instanced rendering in OpenGL. This project demonstrates real-time physics simulation of thousands of spherical cells with efficient GPU-based rendering.

## Recent Updates (June 2025)

This project has undergone significant organizational improvements and code cleanup to improve maintainability, readability, and scalability:

### 📁 Project Reorganization (Latest)
- **Complete directory restructure**: Moved all source files from root into organized `src/` directories
- **Functional organization**: Grouped files by purpose (rendering, simulation, audio, UI, etc.)
- **Dependency consolidation**: Unified all external libraries under `third_party/`
- **Shader organization**: Categorized shaders by usage (cell physics, rendering, spatial operations)
- **Build separation**: Moved project files to dedicated `build_files/` directory
- **Documentation**: Created comprehensive `PROJECT_STRUCTURE.md` guide

### 🧹 Code Organization Improvements
- **Modularized main.cpp**: Extracted window state management, performance monitoring, input processing, and rendering into separate functions
- **Structured CellManager**: Organized member variables with clear comments and removed ambiguous TODOs
- **Enhanced config.h**: Added clear section headers and improved documentation for configuration constants
- **Cleaned shader system**: Improved error messages and code formatting

### 🔧 Specific Changes Made

#### Main Application Structure
- **Window State Management**: Extracted into `handleWindowStateTransitions()` function with proper state tracking
- **Performance Monitoring**: Centralized in `updatePerformanceMonitoring()` function 
- **Input Processing**: Organized into `processInput()` function for better separation of concerns
- **Rendering Pipeline**: Split into `renderFrame()` and `renderImGui()` functions
- **Error Handling**: Consistent error handling patterns throughout the main loop

#### CellManager Improvements
- **Documentation**: Added clear comments explaining GPU buffer purposes and usage
- **Variable Naming**: Improved consistency in member variable naming
- **Memory Management**: Better documentation of pointer usage and buffer relationships
- **Deprecated Code**: Clearly marked deprecated CPU-side vectors with migration notes

#### Configuration System
- **Organized Sections**: Grouped related constants with section headers
- **Improved Comments**: Added explanatory comments for spatial partitioning parameters
- **Consistent Formatting**: Standardized naming conventions and spacing

#### Shader System Enhancements
- **Include Guards**: Fixed spacing in header includes for consistency
- **Error Messages**: Improved shader compilation error reporting
- **Code Comments**: Removed unhelpful comments and added clear documentation

### 🎯 Benefits of Cleanup
- **Maintainability**: Code is now easier to understand and modify
- **Debugging**: Better error messages and organized structure aids troubleshooting
- **Collaboration**: Clear documentation makes it easier for others to contribute
- **Performance**: No performance impact - purely organizational improvements
- **Future Development**: Solid foundation for adding new features

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

### File Structure (Reorganized)
```
Bio-Spheres/
├── main.cpp                 # Application entry point with modularized main loop
├── PROJECT_STRUCTURE.md     # Detailed project organization documentation
├── src/                     # 🚀 SOURCE CODE (organized by functionality)
│   ├── core/               # Core system configuration and utilities
│   ├── rendering/          # Camera, shaders, OpenGL helpers, mesh generation
│   ├── simulation/         # Cell management, genome system, physics
│   ├── ui/                 # User interface and ImGui integration
│   ├── audio/              # Audio engine and synthesizer
│   ├── input/              # Input handling system
│   ├── scene/              # Scene management
│   └── utils/              # Utility functions and timing
├── shaders/                # 🎨 SHADER FILES (organized by usage)
│   ├── cell/               # Cell physics, lifecycle, and management
│   ├── rendering/          # Sphere rendering, culling, debug visualizations
│   ├── spatial/            # Spatial partitioning and grid operations
│   └── common/             # Shared shader utilities
├── third_party/            # 📦 EXTERNAL DEPENDENCIES (ImGui, OpenGL, etc.)
├── Biospheres.sln          # 🔧 Visual Studio solution file
├── Biospheres.vcxproj*     # 🔧 Visual Studio project files
└── docs/                   # 📚 DOCUMENTATION
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
✅ **Code cleanup and organization** (June 2025)
✅ **Modular architecture** with separated concerns
✅ **Improved documentation** and code comments
✅ **Enhanced error handling** and debugging support

### Future Enhancements
🔄 Cell division and growth  
🔄 Spatial partitioning optimization  
🔄 Advanced lighting models  
🔄 Particle effects  
🔄 Save/load simulation states  
🔄 **Complete CPU→GPU migration** (remove deprecated CPU vectors)
🔄 **Performance profiling tools** integration

## Contributing

This project serves as a learning platform for GPU programming and real-time simulation. The recent code cleanup makes it much easier to understand and contribute to the project.

### How to Contribute
Feel free to experiment with:
- Different physics models
- Rendering techniques  
- Compute shader optimizations
- UI improvements
- **Code cleanup**: Continue the refactoring efforts by addressing remaining TODOs
- **Performance optimization**: GPU profiling and bottleneck identification
- **Documentation**: Add inline documentation to complex algorithms

### Code Quality Guidelines
Following the recent cleanup, please maintain:
- **Clear function separation**: Keep functions focused on single responsibilities
- **Consistent naming**: Follow the established naming conventions
- **Proper documentation**: Add clear comments explaining complex logic
- **Error handling**: Include appropriate try-catch blocks and error messages
- **Header organization**: Keep includes organized and properly spaced

---

*~~"If you're a dev, please help me I have no idea what I'm doing"~~ - Original comment from main.cpp 😄*

**Update**: You now have a much better idea of what you're doing! This project demonstrates advanced GPU programming concepts including compute shaders, instanced rendering, and real-time simulation. The recent code cleanup has transformed this from a learning project into a well-structured, maintainable codebase that serves as an excellent foundation for further development.

## License

Educational/Personal use project. See individual library licenses for dependencies.
