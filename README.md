# Bio-Spheres

# This prototype is no longer being developed!
This version has become an unmanagable mess of spaghetti code, so we have decided to start over with rust and webgpu
See the first rust prototype [here](https://github.com/Quadraxis77/BioSpheres-Q)

A real-time 3D cellular simulation and visualization engine built with OpenGL and GPU compute shaders. Bio-Spheres simulates the behavior of biological cells in a 3D environment with physics, genetics, and interactive visualization.

![Bio-Spheres](https://img.shields.io/badge/OpenGL-4.6-blue) ![Bio-Spheres](https://img.shields.io/badge/C++-20-orange) ![Bio-Spheres](https://img.shields.io/badge/Windows-10+-green)

## 🎯 Features

### Core Simulation
- **GPU-Accelerated Physics**: Real-time simulation of up to 100,000 cells using OpenGL compute shaders
- **Spatial Partitioning**: Efficient 64³ grid system for collision detection and neighbor queries
- **Cell Lifecycle**: Complete cell simulation including growth, division, death, and genetic inheritance
- **Genetic System**: Genome-based behavior with multiple cellular modes and inheritance patterns
- **Unique ID System**: Hierarchical cell identification with parent-child relationships

### Visualization & Rendering
- **Level-of-Detail (LOD)**: Dynamic mesh complexity based on camera distance
- **Frustum Culling**: GPU-accelerated visibility culling for performance
- **Interactive Gizmos**: Visual indicators for cell orientation, adhesion, and ring structures
- **Wireframe Mode**: Toggle between solid and wireframe rendering
- **Real-time Camera Control**: Orbit, pan, and zoom with mouse and keyboard

### User Interface
- **ImGui Integration**: Real-time parameter adjustment and simulation control
- **Performance Monitoring**: Live FPS, frame time, and GPU utilization metrics
- **Scene Management**: Switch between preview and main simulation modes
- **Cell Selection**: Click and drag individual cells for manual manipulation

### Audio System
- **Real-time Synthesis**: Procedural audio generation based on simulation state
- **Miniaudio Integration**: Low-latency audio playback with 44.1kHz sample rate

## 🚀 Getting Started

### Prerequisites

- **Windows 10 or later**
- **Visual Studio 2022** with C++20 support
- **OpenGL 4.6** compatible graphics card
- **DirectX 11** or later

### Installation

1. **Clone the repository**
   ```bash
   git clone https://github.com/yourusername/bio-spheres.git
   cd bio-spheres
   ```

2. **Open the project**
   - Open `Biospheres.sln` in Visual Studio 2022
   - Select your preferred configuration (Debug/Release) and platform (x64 recommended)

3. **Build the project**
   - Press `Ctrl+Shift+B` or go to `Build → Build Solution`
   - The executable will be generated in `x64/Debug/` or `x64/Release/`

4. **Run the application**
   - Press `F5` to run with debugging or `Ctrl+F5` to run without debugging
   - The application will launch with a default simulation of 100,000 cells

### Quick Start

1. **Launch the application** - You'll see a 3D view with spherical cells
2. **Camera Controls**:
   - **Left Mouse**: Orbit camera around the simulation
   - **Right Mouse**: Pan camera
   - **Mouse Wheel**: Zoom in/out
   - **WASD**: Move camera position
3. **UI Controls**:
   - **Performance Panel**: Monitor FPS and system metrics
   - **Simulation Panel**: Adjust physics parameters and cell behavior
   - **Rendering Panel**: Toggle visualization options
4. **Cell Interaction**:
   - **Click**: Select individual cells
   - **Drag**: Move selected cells manually

## 🎮 Controls

### Camera Controls
| Input | Action |
|-------|--------|
| Left Mouse + Drag | Orbit camera around center |
| Right Mouse + Drag | Pan camera |
| Mouse Wheel | Zoom in/out |
| W/A/S/D | Move camera forward/left/backward/right |
| Q/E | Move camera up/down |
| R | Reset camera to default position |

### UI Controls
| Panel | Function |
|-------|----------|
| Performance | Monitor FPS, frame time, cell count |
| Simulation | Adjust physics timestep, cell limits |
| Rendering | Toggle wireframe, gizmos, LOD |
| Audio | Control synthesis parameters |

### Cell Interaction
| Action | Description |
|--------|-------------|
| Left Click | Select cell at cursor position |
| Left Click + Drag | Move selected cell |
| Scroll Wheel | Adjust cell properties (when selected) |

## 🏗️ Architecture

### Core Systems

#### **Cell Manager** (`src/simulation/cell/cell_manager.h`)
- GPU-accelerated cell simulation using compute shaders
- Spatial partitioning with 64³ grid system
- Unique ID management for cell tracking
- LOD and frustum culling for performance

#### **Rendering Pipeline** (`src/rendering/`)
- OpenGL 4.6 with compute shader support
- Instanced rendering for efficient cell display
- Dynamic LOD system with distance-based mesh complexity
- Real-time gizmo generation and rendering

#### **Audio Engine** (`src/audio/`)
- Real-time procedural synthesis
- Miniaudio integration for low-latency playback
- Simulation-driven audio generation

#### **UI System** (`src/ui/`)
- ImGui-based interface
- Real-time parameter adjustment
- Performance monitoring and debugging tools

### GPU Compute Pipeline

```
Physics Compute → Update Compute → Internal Update → Spatial Grid → Rendering
```

1. **Physics Compute**: Position, velocity, and collision calculations
2. **Update Compute**: Cell lifecycle and genetic behavior
3. **Internal Update**: Signaling substances and internal state
4. **Spatial Grid**: Neighbor queries and spatial organization
5. **Rendering**: LOD calculation, frustum culling, and draw calls

## 📊 Performance

### Target Specifications
- **Minimum**: GTX 1060 / RX 580 (OpenGL 4.6)
- **Recommended**: RTX 3060 / RX 6600 or better
- **Optimal**: RTX 4070 / RX 7700 XT or better

### Performance Metrics
- **100,000 cells**: 60+ FPS on recommended hardware
- **GPU Memory**: ~500MB for maximum cell count
- **CPU Usage**: <5% (GPU-accelerated simulation)
- **Memory Usage**: ~2GB total application memory

### Optimization Features
- **GPU Compute Shaders**: Parallel physics simulation
- **Spatial Partitioning**: O(1) neighbor queries
- **Level-of-Detail**: Distance-based mesh complexity
- **Frustum Culling**: Only render visible cells
- **Instanced Rendering**: Efficient batch drawing

## 🔧 Configuration

### Key Parameters (`src/core/config.h`)

```cpp
// Simulation Limits
constexpr int MAX_CELLS{100000};
constexpr int DEFAULT_CELL_COUNT{100000};
constexpr float DEFAULT_SPAWN_RADIUS{50.0f};

// World Configuration
constexpr float WORLD_SIZE{100.0f};
constexpr int GRID_RESOLUTION{64};
constexpr int MAX_CELLS_PER_GRID{32};

// Rendering
constexpr int INITIAL_WINDOW_WIDTH{800};
constexpr int INITIAL_WINDOW_HEIGHT{600};
constexpr int OPENGL_VERSION_MAJOR{4};
constexpr int OPENGL_VERSION_MINOR{6};
```

### Runtime Configuration
- **Physics Timestep**: Adjust simulation accuracy vs performance
- **Cell Limits**: Control maximum cell count
- **LOD Distances**: Fine-tune rendering performance
- **Audio Parameters**: Modify synthesis behavior

## 🧬 Simulation Details

### Cell Structure
Each cell contains:
- **Physics**: Position, velocity, mass, orientation
- **Internal State**: Age, toxins, nitrates, signaling substances
- **Genetics**: Mode index, unique ID, parent-child relationships
- **Behavior**: Growth, division, death, and interaction patterns

### Genetic System
- **Genome Modes**: Multiple behavioral states per cell
- **Inheritance**: Parent-to-child genetic transfer
- **Mutation**: Random genetic changes during division
- **Selection**: Environmental pressure on survival

### Spatial System
- **64³ Grid**: Efficient spatial partitioning
- **Neighbor Queries**: Fast proximity detection
- **Collision Detection**: GPU-accelerated physics
- **Adhesion**: Cell-to-cell interaction simulation

## 🛠️ Development

### Project Structure
```
Bio-Spheres/
├── src/
│   ├── audio/          # Audio engine and synthesis
│   ├── core/           # Configuration and common utilities
│   ├── input/          # Input handling and camera controls
│   ├── rendering/      # OpenGL rendering pipeline
│   ├── simulation/     # Cell simulation and genetics
│   ├── ui/            # ImGui interface
│   └── utils/         # Utility functions and timing
├── shaders/           # GLSL compute and rendering shaders
├── third_party/       # External libraries (GLFW, GLM, ImGui)
└── docs/             # Documentation and guides
```

### Building from Source
1. Ensure Visual Studio 2022 is installed with C++20 support
2. Open `Biospheres.sln`
3. Select x64 platform and Release configuration
4. Build solution (`Ctrl+Shift+B`)
5. Run the application (`F5`)

### Debugging
- **Debug Configuration**: Includes debug symbols and validation
- **Performance Monitoring**: Built-in FPS and timing metrics
- **GPU Debugging**: OpenGL error checking and validation
- **Cell Inspection**: Real-time cell data viewing

## 🤝 Contributing

### Development Guidelines
1. **Code Style**: Follow existing C++20 patterns
2. **Performance**: Maintain 60+ FPS target
3. **Documentation**: Update relevant documentation
4. **Testing**: Test on multiple GPU configurations

### Areas for Contribution
- **New Cell Behaviors**: Additional genetic modes and interactions
- **Visual Effects**: Enhanced rendering and particle systems
- **Audio Features**: More sophisticated synthesis algorithms
- **UI Improvements**: Additional controls and visualization options
- **Performance**: Optimization and profiling improvements

## 📝 License

This project is licensed under the MIT License - see the LICENSE file for details.
