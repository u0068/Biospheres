# Cell Unique ID System

This document describes the unique ID system implemented for cells in the Bio-Spheres simulation.

## Overview

The unique ID system assigns a unique identifier to every cell in the format `X.Y.Z` where:
- `X` = Parent ID (16 bits)
- `Y` = Cell ID (15 bits) 
- `Z` = Child flag (1 bit: 0=A, 1=B)

The entire system operates on the GPU for maximum performance.

## ID Format

The unique ID is stored as a 32-bit unsigned integer with the following bit layout:

```
Bits 31-16: Parent ID (16 bits) - ID of the parent cell when this cell was created
Bits 15-1:  Cell ID (15 bits)   - Unique identifier for this cell (max 32767)
Bit 0:      Child flag (1 bit)  - 0 for 'A' child, 1 for 'B' child after split
```

## ID Assignment

### Initial Spawning
- Parent ID = 0 (no parent)
- Cell ID = sequential counter starting from 1
- Child flag = 0 (A)

### Cell Division
When a cell splits:
1. Two new cell IDs are allocated from the ID pool
2. Parent ID = the splitting cell's Cell ID
3. Child A gets child flag = 0
4. Child B gets child flag = 1

Example: Cell `0.5.A` splits into `5.10.A` and `5.11.B`

## ID Recycling

The system implements ID recycling to reuse IDs when cells die:

1. Dead cells (mass < threshold) are detected by `id_manager.comp`
2. Their Cell IDs are added to a recycling pool
3. New cells preferentially use recycled IDs before generating new ones
4. When all 32767 IDs are used, the system wraps around to ID 1

## GPU Implementation

### Buffers
- `idCounterBuffer`: Stores counters (next available ID, recycled count, etc.)
- `idPoolBuffer`: Pool of available/recycled cell IDs
- `idRecycleBuffer`: Temporary storage for dead cell IDs

### Shaders
- `id_manager.comp`: Handles ID recycling and dead cell detection
- `cell_update_internal.comp`: Assigns IDs during cell division

### Buffer Bindings
The ID management buffers are bound to the internal update shader:
- Binding 4: ID Counter Buffer
- Binding 5: ID Pool Buffer

## Usage

### C++ Side
```cpp
// Get ID components
uint16_t parentID = cell.getParentID();
uint16_t cellID = cell.getCellID();
uint8_t childFlag = cell.getChildFlag();

// Set ID
cell.setUniqueID(parentID, cellID, childFlag);

// Debug output
cellManager.printCellIDs(10); // Print first 10 cell IDs
```

### Shader Side
```glsl
// ID utility functions
uint getParentID(uint uniqueID);
uint getCellID(uint uniqueID);
uint getChildFlag(uint uniqueID);
uint packUniqueID(uint parentID, uint cellID, uint childFlag);

// Allocate new ID
uint newCellID = allocateCellID();
```

## Memory Layout

The ComputeCell struct maintains 16-byte alignment:
```cpp
struct ComputeCell {
    // ... other fields ...
    uint32_t uniqueID;  // Packed ID
    uint32_t padding;   // Maintains alignment
};
```

## Limitations

- Maximum 32767 unique cell IDs (15-bit limitation)
- Parent ID limited to 65535 (16-bit limitation)
- ID recycling may cause the same ID to be reused after all cells with that ID have died

## Integration

The ID system is automatically integrated into the simulation update cycle:
1. Physics update
2. Position update  
3. Internal update (handles cell division and ID assignment)
4. Cell additions (applies new cells from divisions)
5. ID manager (recycles dead cell IDs)

This ensures that IDs are properly managed throughout the simulation lifecycle. 