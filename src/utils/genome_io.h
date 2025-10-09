#pragma once
#include <string>
#include <vector>
#include "../simulation/cell/common_structs.h"

namespace GenomeIO
{
    // Save a genome to a .genome file
    // Returns true on success, false on failure
    bool saveGenome(const GenomeData& genome, const std::string& filepath);
    
    // Load a genome from a .genome file
    // Returns true on success, false on failure
    bool loadGenome(GenomeData& genome, const std::string& filepath);
    
    // Open a file dialog to save a genome
    // Returns the selected filepath, or empty string if cancelled
    std::string openSaveDialog(const std::string& defaultName = "genome");
    
    // Open a file dialog to load a genome
    // Returns the selected filepath, or empty string if cancelled
    std::string openLoadDialog();
    
    // Get the genomes directory path (creates it if it doesn't exist)
    std::string getGenomesDirectory();
    
    // List all .genome files in the genomes directory
    std::vector<std::string> listGenomeFiles();
}
