# Bio-Spheres Project Structure

This document outlines the reorganized file structure of the Bio-Spheres project, designed for better maintainability, clarity, and scalability.

## 📁 Root Directory Structure

```
Bio-Spheres/
├── main.cpp                    # Application entry point
├── README.md                   # Project documentation
├── PROJECT_STRUCTURE.md        # This file - project organization guide
├── CLEANUP_SUMMARY.md          # Previous cleanup documentation
├── CELL_ID_SYSTEM.md          # Cell ID system documentation
├── CODE_STYLE.md              # Code style guidelines
├── .gitignore                  # Git ignore rules
├── .gitattributes             # Git attributes
├── imgui.ini                  # ImGui configuration
│
├── src/                       # 🚀 SOURCE CODE (organized by functionality)
├── shaders/                   # 🎨 SHADER FILES (organized by usage)
├── third_party/               # 📦 EXTERNAL DEPENDENCIES
├── docs/                      # 📚 DOCUMENTATION
├── project/                   # 📋 PROJECT ASSETS
├── build/                     # 🏗️ BUILD OUTPUT (generated)
├── Biospheres.sln             # 🔧 Visual Studio solution file
├── Biospheres.vcxproj*        # 🔧 Visual Studio project files
└── Biospheres.code-workspace  # 🔧 VS Code workspace configuration
```

## 🚀 Source Code Organization (`src/`)

The source code is organized by functional domains:

### Core System Files (`src/core/`)
- **`config.h`** - Application configuration constants
- **`common.h`** - Common utilities and definitions
- **`forward_declarations.h`** - Forward declarations for compile optimization
- **`resource.h`** - Resource management definitions

### Audio System (`src/audio/`)
- **`audio_engine.cpp/.h`** - Audio engine implementation
- **`synthesizer.cpp/.h`** - Audio synthesis system

### Input System (`src/input/`)
- **`input.cpp/.h`** - Input handling and processing

### Rendering System (`src/rendering/`)
```
src/rendering/
├── camera/
│   ├── camera.cpp              # Camera implementation
│   └── camera.h                # Camera interface
├── core/
│   ├── shader_class.cpp/.h     # Shader compilation and management
│   ├── glad_helpers.cpp/.h     # OpenGL loader helpers
│   ├── glfw_helpers.cpp/.h     # GLFW window management helpers
│   └── mesh/
│       ├── sphere_mesh.cpp     # Sphere mesh generation
│       └── sphere_mesh.h       # Sphere mesh interface
└── systems/
    ├── frustum_culling.cpp     # Frustum culling implementation
    └── frustum_culling.h       # Frustum culling interface
```

### Simulation System (`src/simulation/`)
```
src/simulation/
├── cell/
│   ├── cell_manager.cpp        # Cell simulation management
│   └── cell_manager.h          # Cell manager interface
├── genome/
│   ├── genome.cpp              # Genetic system implementation
│   └── genome.h                # Genetic system interface
└── spatial/
    └── (spatial partitioning systems - future)
```

### User Interface (`src/ui/`)
- **`ui_manager.cpp/.h`** - Main UI management
- **`imgui_helpers.cpp/.h`** - ImGui utility functions
- **`panels/`** - Individual UI panels (future organization)

### Scene Management (`src/scene/`)
- **`scene_manager.h`** - Scene state management

### Utilities (`src/utils/`)
- **`timer.cpp/.h`** - Performance timing utilities

## 🎨 Shader Organization (`shaders/`)

Shaders are organized by their functional purpose:

```
shaders/
├── cell/
│   ├── physics/
│   │   ├── cell_physics_spatial.comp    # Spatial physics computation
│   │   ├── cell_update.comp             # Cell state updates
│   │   └── cell_update_internal.comp    # Internal cell processing
│   ├── lifecycle/
│   │   └── (cell division, death shaders - future)
│   └── management/
│       ├── apply_additions.comp         # Apply cell additions
│       ├── cell_counter.comp            # Cell counting
│       ├── extract_instances.comp       # Instance data extraction
│       └── id_manager.comp              # Cell ID management
├── spatial/
│   ├── grid_assign.comp                 # Spatial grid assignment
│   ├── grid_clear.comp                  # Grid clearing
│   ├── grid_insert.comp                 # Grid insertion
│   └── grid_prefix_sum.comp             # Grid prefix sum operations
├── rendering/
│   ├── sphere/
│   │   ├── sphere.vert                  # Sphere vertex shader
│   │   ├── sphere.frag                  # Sphere fragment shader
│   │   ├── sphere_lod.vert              # LOD sphere vertex shader
│   │   ├── sphere_lod.frag              # LOD sphere fragment shader
│   │   └── sphere_lod.comp              # LOD computation
│   ├── culling/
│   │   ├── frustum_cull.comp            # Frustum culling
│   │   └── frustum_cull_lod.comp        # LOD frustum culling
│   └── debug/
│       ├── gizmo.vert/.frag             # Debug gizmo rendering
│       ├── gizmo_extract.comp           # Gizmo data extraction
│       ├── ring_gizmo.vert/.frag        # Ring gizmo rendering
│       ├── ring_gizmo_extract.comp      # Ring gizmo extraction
│       ├── adhesion_line.vert/.frag     # Adhesion line rendering
│       └── adhesion_line_extract.comp   # Adhesion line extraction
└── common/
    └── (shared shader utilities - future)
```

## 📦 External Dependencies (`third_party/`)

All external libraries and dependencies are consolidated here:

```
third_party/
├── imgui/                      # Dear ImGui library
│   ├── imgui.h
│   ├── imgui.cpp
│   ├── imgui_impl_glfw.cpp/.h
│   ├── imgui_impl_opengl3.cpp/.h
│   └── (other ImGui files)
├── include/                    # Header-only libraries
│   ├── glad/
│   ├── GLFW/
│   ├── glm/
│   ├── KHR/
│   └── miniaudio.h
├── lib/                        # Static libraries
│   └── glfw3.lib
└── glad.c                      # GLAD implementation
```

## 🔧 Build & Project Files (Root Directory)

Build system files remain in the root directory for tool compatibility:

- **`Biospheres.sln`** - Visual Studio solution file (must be in root)
- **`Biospheres.vcxproj*`** - Visual Studio project files (must be in root)
- **`Biospheres.code-workspace`** - VS Code workspace configuration

## 📚 Additional Directories

- **`docs/`** - Additional documentation files
- **`project/`** - Project-specific assets and configurations
- **`build/`** - Build output directory (generated by build system)

## 🔄 Migration Benefits

### Before Reorganization:
- ❌ 20+ source files scattered in root directory
- ❌ Multiple dependency directories (`external/`, `Libraries/`, `imgui/`)
- ❌ Build files mixed with source code
- ❌ Shaders not well organized by function

### After Reorganization:
- ✅ Clean root directory with only essential files
- ✅ Logical grouping by functionality
- ✅ Consistent dependency management
- ✅ Clear separation of concerns
- ✅ Scalable structure for future development

## 🛠️ Development Guidelines

### Adding New Files:
1. **Source Code**: Place in appropriate `src/` subdirectory
2. **Shaders**: Organize by functional purpose in `shaders/`
3. **Dependencies**: Add to `third_party/` with proper attribution
4. **Documentation**: Use `docs/` for extensive documentation

### Include Path Conventions:
- Use relative paths from project root
- Group includes by category (core, rendering, simulation, etc.)
- Always use forward slashes in include paths
- Example: `#include "src/rendering/camera/camera.h"`

### Namespace Guidelines:
- Consider using namespaces to match directory structure
- Example: `Bio::Rendering::Camera`, `Bio::Simulation::Cell`

## 🎯 Future Enhancements

The new structure supports these planned improvements:

1. **Modular Compilation**: Each directory can become a separate library
2. **Plugin Architecture**: Easy to add new simulation or rendering modules
3. **Testing Integration**: Clear structure for unit tests per module
4. **Documentation Generation**: Automatic docs from organized source
5. **CMake Migration**: Structure ready for cross-platform build system

This organization transforms Bio-Spheres from a monolithic structure into a clean, maintainable, and scalable codebase that supports future development and collaboration. 