# Genome Save/Load System

This directory contains saved genome files for the Biospheres simulation.

## File Format

Genome files use the `.genome` extension and are stored in a human-readable text format.

### Structure

Each genome file contains:
- **Genome name**: A descriptive name for the genome
- **Initial mode**: The starting mode index for new cells
- **Initial orientation**: The initial quaternion orientation
- **Modes**: A list of behavioral modes, each containing:
  - Mode name and cell type
  - Color (RGB)
  - Parent settings (adhesion, split mass, split interval, split direction, max adhesions)
  - Child A and B settings (mode number, orientation, keep adhesion flag)
  - Adhesion settings (spring constants, damping, constraints)
  - Flagellocyte settings (tail properties, swim speed, nutrient consumption)
  - Nutrient priority

## Usage

### Saving a Genome

1. Open the **Genome Editor** window in the application
2. Configure your genome with the desired modes and settings
3. Enter a name for your genome in the "Genome Name" field
4. Click the **Save Genome** button
5. Choose a location and filename in the file dialog (defaults to the `genomes` folder)
6. The genome will be saved with a `.genome` extension

### Loading a Genome

1. Open the **Genome Editor** window in the application
2. Click the **Load Genome** button
3. Select a `.genome` file from the file dialog
4. The genome will be loaded and applied to the simulation
5. The genome editor will update to show the loaded configuration

## File Location

By default, genome files are saved to and loaded from:
```
<project_directory>/genomes/
```

This directory is automatically created when you first save a genome.

## Notes

- Genome files are plain text and can be edited manually if needed
- The file format is version 1.0 (indicated in the file header)
- All quaternions are stored as (w, x, y, z)
- All vectors are stored as space-separated components
- Strings are quoted and escaped
- Comments in genome files start with `#`
