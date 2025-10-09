#include "genome_io.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <Windows.h>
#include <commdlg.h>
#include <glm/gtc/quaternion.hpp>

namespace GenomeIO
{
    // Helper function to write a quaternion
    void writeQuaternion(std::ofstream& file, const glm::quat& q)
    {
        file << q.w << " " << q.x << " " << q.y << " " << q.z;
    }
    
    // Helper function to read a quaternion
    bool readQuaternion(std::ifstream& file, glm::quat& q)
    {
        return static_cast<bool>(file >> q.w >> q.x >> q.y >> q.z);
    }
    
    // Helper function to write a vec3
    void writeVec3(std::ofstream& file, const glm::vec3& v)
    {
        file << v.x << " " << v.y << " " << v.z;
    }
    
    // Helper function to read a vec3
    bool readVec3(std::ifstream& file, glm::vec3& v)
    {
        return static_cast<bool>(file >> v.x >> v.y >> v.z);
    }
    
    // Helper function to write a vec2
    void writeVec2(std::ofstream& file, const glm::vec2& v)
    {
        file << v.x << " " << v.y;
    }
    
    // Helper function to read a vec2
    bool readVec2(std::ifstream& file, glm::vec2& v)
    {
        return static_cast<bool>(file >> v.x >> v.y);
    }
    
    // Helper function to write a string (with quotes and escaping)
    void writeString(std::ofstream& file, const std::string& str)
    {
        file << "\"";
        for (char c : str)
        {
            if (c == '\"' || c == '\\')
                file << '\\';
            file << c;
        }
        file << "\"";
    }
    
    // Helper function to read a string (with quotes and escaping)
    bool readString(std::ifstream& file, std::string& str)
    {
        str.clear();
        char c;
        
        // Skip whitespace
        while (file.get(c) && std::isspace(c));
        
        if (c != '\"')
            return false;
        
        bool escaped = false;
        while (file.get(c))
        {
            if (escaped)
            {
                str += c;
                escaped = false;
            }
            else if (c == '\\')
            {
                escaped = true;
            }
            else if (c == '\"')
            {
                return true;
            }
            else
            {
                str += c;
            }
        }
        
        return false;
    }
    
    std::string getGenomesDirectory()
    {
        // Get the executable's directory
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
        
        // Create genomes folder in the executable's directory
        std::filesystem::path genomesPath = exeDir / "genomes";
        
        // Create directory if it doesn't exist
        if (!std::filesystem::exists(genomesPath))
        {
            std::filesystem::create_directories(genomesPath);
        }
        
        return genomesPath.string();
    }
    
    bool saveGenome(const GenomeData& genome, const std::string& filepath)
    {
        std::ofstream file(filepath);
        if (!file.is_open())
        {
            std::cerr << "Failed to open file for writing: " << filepath << std::endl;
            return false;
        }
        
        // Write header
        file << "# Biospheres Genome File v1.0\n";
        file << "# Generated genome configuration\n\n";
        
        // Write genome name
        file << "genome_name ";
        writeString(file, genome.name);
        file << "\n";
        
        // Write initial mode
        file << "initial_mode " << genome.initialMode << "\n";
        
        // Write initial orientation
        file << "initial_orientation ";
        writeQuaternion(file, genome.initialOrientation);
        file << "\n\n";
        
        // Write number of modes
        file << "mode_count " << genome.modes.size() << "\n\n";
        
        // Write each mode
        for (size_t i = 0; i < genome.modes.size(); ++i)
        {
            const ModeSettings& mode = genome.modes[i];
            
            file << "mode " << i << " {\n";
            
            // Mode name
            file << "  name ";
            writeString(file, mode.name);
            file << "\n";
            
            // Cell type
            file << "  cell_type " << static_cast<int>(mode.cellType) << "\n";
            
            // Color
            file << "  color ";
            writeVec3(file, mode.color);
            file << "\n";
            
            // Parent settings
            file << "  parent_make_adhesion " << (mode.parentMakeAdhesion ? 1 : 0) << "\n";
            file << "  split_mass " << mode.splitMass << "\n";
            file << "  split_interval " << mode.splitInterval << "\n";
            file << "  parent_split_direction ";
            writeVec2(file, mode.parentSplitDirection);
            file << "\n";
            file << "  max_adhesions " << mode.maxAdhesions << "\n";
            
            // Child A settings
            file << "  child_a_mode " << mode.childA.modeNumber << "\n";
            file << "  child_a_orientation ";
            writeQuaternion(file, mode.childA.orientation);
            file << "\n";
            file << "  child_a_keep_adhesion " << (mode.childA.keepAdhesion ? 1 : 0) << "\n";
            
            // Child B settings
            file << "  child_b_mode " << mode.childB.modeNumber << "\n";
            file << "  child_b_orientation ";
            writeQuaternion(file, mode.childB.orientation);
            file << "\n";
            file << "  child_b_keep_adhesion " << (mode.childB.keepAdhesion ? 1 : 0) << "\n";
            
            // Adhesion settings
            file << "  adhesion_can_break " << (mode.adhesionSettings.canBreak ? 1 : 0) << "\n";
            file << "  adhesion_break_force " << mode.adhesionSettings.breakForce << "\n";
            file << "  adhesion_rest_length " << mode.adhesionSettings.restLength << "\n";
            file << "  adhesion_linear_spring_stiffness " << mode.adhesionSettings.linearSpringStiffness << "\n";
            file << "  adhesion_linear_spring_damping " << mode.adhesionSettings.linearSpringDamping << "\n";
            file << "  adhesion_orientation_spring_stiffness " << mode.adhesionSettings.orientationSpringStiffness << "\n";
            file << "  adhesion_orientation_spring_damping " << mode.adhesionSettings.orientationSpringDamping << "\n";
            file << "  adhesion_max_angular_deviation " << mode.adhesionSettings.maxAngularDeviation << "\n";
            file << "  adhesion_twist_constraint_stiffness " << mode.adhesionSettings.twistConstraintStiffness << "\n";
            file << "  adhesion_twist_constraint_damping " << mode.adhesionSettings.twistConstraintDamping << "\n";
            file << "  adhesion_enable_twist_constraint " << (mode.adhesionSettings.enableTwistConstraint ? 1 : 0) << "\n";
            
            // Flagellocyte settings
            file << "  flagellocyte_tail_length " << mode.flagellocyteSettings.tailLength << "\n";
            file << "  flagellocyte_tail_thickness " << mode.flagellocyteSettings.tailThickness << "\n";
            file << "  flagellocyte_spiral_tightness " << mode.flagellocyteSettings.spiralTightness << "\n";
            file << "  flagellocyte_spiral_radius " << mode.flagellocyteSettings.spiralRadius << "\n";
            file << "  flagellocyte_rotation_speed " << mode.flagellocyteSettings.rotationSpeed << "\n";
            file << "  flagellocyte_tail_taper " << mode.flagellocyteSettings.tailTaper << "\n";
            file << "  flagellocyte_segments " << mode.flagellocyteSettings.segments << "\n";
            file << "  flagellocyte_tail_color ";
            writeVec3(file, mode.flagellocyteSettings.tailColor);
            file << "\n";
            file << "  flagellocyte_swim_speed " << mode.flagellocyteSettings.swimSpeed << "\n";
            file << "  flagellocyte_nutrient_consumption_rate " << mode.flagellocyteSettings.nutrientConsumptionRate << "\n";
            
            // Nutrient priority
            file << "  nutrient_priority " << mode.nutrientPriority << "\n";
            
            file << "}\n\n";
        }
        
        file.close();
        return true;
    }
    
    bool loadGenome(GenomeData& genome, const std::string& filepath)
    {
        std::ifstream file(filepath);
        if (!file.is_open())
        {
            std::cerr << "Failed to open file for reading: " << filepath << std::endl;
            return false;
        }
        
        genome = GenomeData(); // Reset to default
        genome.modes.clear(); // Clear default mode
        
        std::string line;
        std::string token;
        
        while (file >> token)
        {
            if (token == "#" || token.empty())
            {
                // Skip comments
                std::getline(file, line);
                continue;
            }
            
            if (token == "genome_name")
            {
                if (!readString(file, genome.name))
                {
                    std::cerr << "Failed to read genome name" << std::endl;
                    return false;
                }
            }
            else if (token == "initial_mode")
            {
                file >> genome.initialMode;
            }
            else if (token == "initial_orientation")
            {
                if (!readQuaternion(file, genome.initialOrientation))
                {
                    std::cerr << "Failed to read initial orientation" << std::endl;
                    return false;
                }
            }
            else if (token == "mode_count")
            {
                int modeCount;
                file >> modeCount;
                genome.modes.reserve(modeCount);
            }
            else if (token == "mode")
            {
                int modeIndex;
                file >> modeIndex;
                
                std::string brace;
                file >> brace;
                if (brace != "{")
                {
                    std::cerr << "Expected '{' after mode declaration" << std::endl;
                    return false;
                }
                
                ModeSettings mode;
                
                // Read mode properties
                while (file >> token && token != "}")
                {
                    if (token == "name")
                    {
                        if (!readString(file, mode.name))
                        {
                            std::cerr << "Failed to read mode name" << std::endl;
                            return false;
                        }
                    }
                    else if (token == "cell_type")
                    {
                        int cellType;
                        file >> cellType;
                        mode.cellType = static_cast<CellType>(cellType);
                    }
                    else if (token == "color")
                    {
                        if (!readVec3(file, mode.color))
                        {
                            std::cerr << "Failed to read color" << std::endl;
                            return false;
                        }
                    }
                    else if (token == "parent_make_adhesion")
                    {
                        int value;
                        file >> value;
                        mode.parentMakeAdhesion = (value != 0);
                    }
                    else if (token == "split_mass")
                    {
                        file >> mode.splitMass;
                    }
                    else if (token == "split_interval")
                    {
                        file >> mode.splitInterval;
                    }
                    else if (token == "parent_split_direction")
                    {
                        if (!readVec2(file, mode.parentSplitDirection))
                        {
                            std::cerr << "Failed to read parent split direction" << std::endl;
                            return false;
                        }
                    }
                    else if (token == "max_adhesions")
                    {
                        file >> mode.maxAdhesions;
                    }
                    else if (token == "child_a_mode")
                    {
                        file >> mode.childA.modeNumber;
                    }
                    else if (token == "child_a_orientation")
                    {
                        if (!readQuaternion(file, mode.childA.orientation))
                        {
                            std::cerr << "Failed to read child A orientation" << std::endl;
                            return false;
                        }
                    }
                    else if (token == "child_a_keep_adhesion")
                    {
                        int value;
                        file >> value;
                        mode.childA.keepAdhesion = (value != 0);
                    }
                    else if (token == "child_b_mode")
                    {
                        file >> mode.childB.modeNumber;
                    }
                    else if (token == "child_b_orientation")
                    {
                        if (!readQuaternion(file, mode.childB.orientation))
                        {
                            std::cerr << "Failed to read child B orientation" << std::endl;
                            return false;
                        }
                    }
                    else if (token == "child_b_keep_adhesion")
                    {
                        int value;
                        file >> value;
                        mode.childB.keepAdhesion = (value != 0);
                    }
                    else if (token == "adhesion_can_break")
                    {
                        int value;
                        file >> value;
                        mode.adhesionSettings.canBreak = (value != 0);
                    }
                    else if (token == "adhesion_break_force")
                    {
                        file >> mode.adhesionSettings.breakForce;
                    }
                    else if (token == "adhesion_rest_length")
                    {
                        file >> mode.adhesionSettings.restLength;
                    }
                    else if (token == "adhesion_linear_spring_stiffness")
                    {
                        file >> mode.adhesionSettings.linearSpringStiffness;
                    }
                    else if (token == "adhesion_linear_spring_damping")
                    {
                        file >> mode.adhesionSettings.linearSpringDamping;
                    }
                    else if (token == "adhesion_orientation_spring_stiffness")
                    {
                        file >> mode.adhesionSettings.orientationSpringStiffness;
                    }
                    else if (token == "adhesion_orientation_spring_damping")
                    {
                        file >> mode.adhesionSettings.orientationSpringDamping;
                    }
                    else if (token == "adhesion_max_angular_deviation")
                    {
                        file >> mode.adhesionSettings.maxAngularDeviation;
                    }
                    else if (token == "adhesion_twist_constraint_stiffness")
                    {
                        file >> mode.adhesionSettings.twistConstraintStiffness;
                    }
                    else if (token == "adhesion_twist_constraint_damping")
                    {
                        file >> mode.adhesionSettings.twistConstraintDamping;
                    }
                    else if (token == "adhesion_enable_twist_constraint")
                    {
                        int value;
                        file >> value;
                        mode.adhesionSettings.enableTwistConstraint = (value != 0);
                    }
                    else if (token == "flagellocyte_tail_length")
                    {
                        file >> mode.flagellocyteSettings.tailLength;
                    }
                    else if (token == "flagellocyte_tail_thickness")
                    {
                        file >> mode.flagellocyteSettings.tailThickness;
                    }
                    else if (token == "flagellocyte_spiral_tightness")
                    {
                        file >> mode.flagellocyteSettings.spiralTightness;
                    }
                    else if (token == "flagellocyte_spiral_radius")
                    {
                        file >> mode.flagellocyteSettings.spiralRadius;
                    }
                    else if (token == "flagellocyte_rotation_speed")
                    {
                        file >> mode.flagellocyteSettings.rotationSpeed;
                    }
                    else if (token == "flagellocyte_tail_taper")
                    {
                        file >> mode.flagellocyteSettings.tailTaper;
                    }
                    else if (token == "flagellocyte_segments")
                    {
                        file >> mode.flagellocyteSettings.segments;
                    }
                    else if (token == "flagellocyte_tail_color")
                    {
                        if (!readVec3(file, mode.flagellocyteSettings.tailColor))
                        {
                            std::cerr << "Failed to read flagellocyte tail color" << std::endl;
                            return false;
                        }
                    }
                    else if (token == "flagellocyte_swim_speed")
                    {
                        file >> mode.flagellocyteSettings.swimSpeed;
                    }
                    else if (token == "flagellocyte_nutrient_consumption_rate")
                    {
                        file >> mode.flagellocyteSettings.nutrientConsumptionRate;
                    }
                    else if (token == "nutrient_priority")
                    {
                        file >> mode.nutrientPriority;
                    }
                }
                
                genome.modes.push_back(mode);
            }
        }
        
        file.close();
        
        // Validate that we loaded at least one mode
        if (genome.modes.empty())
        {
            std::cerr << "No modes found in genome file" << std::endl;
            return false;
        }
        
        return true;
    }
    
    std::string openSaveDialog(const std::string& defaultName)
    {
        OPENFILENAMEA ofn;
        char szFile[260] = { 0 };
        char szInitialDir[260] = { 0 };
        
        // Set default filename
        std::string defaultFilename = defaultName + ".genome";
        strcpy_s(szFile, defaultFilename.c_str());
        
        // Set initial directory to genomes folder (must persist for the dialog)
        std::string genomesDir = getGenomesDirectory();
        strcpy_s(szInitialDir, genomesDir.c_str());
        
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "Genome Files\0*.genome\0All Files\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = szInitialDir;
        
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
        ofn.lpstrDefExt = "genome";
        
        if (GetSaveFileNameA(&ofn) == TRUE)
        {
            return std::string(ofn.lpstrFile);
        }
        
        return "";
    }
    
    std::string openLoadDialog()
    {
        OPENFILENAMEA ofn;
        char szFile[260] = { 0 };
        char szInitialDir[260] = { 0 };
        
        // Set initial directory to genomes folder (must persist for the dialog)
        std::string genomesDir = getGenomesDirectory();
        strcpy_s(szInitialDir, genomesDir.c_str());
        
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "Genome Files\0*.genome\0All Files\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = szInitialDir;
        
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
        
        if (GetOpenFileNameA(&ofn) == TRUE)
        {
            return std::string(ofn.lpstrFile);
        }
        
        return "";
    }
    
    std::vector<std::string> listGenomeFiles()
    {
        std::vector<std::string> files;
        std::string genomesDir = getGenomesDirectory();
        
        try
        {
            for (const auto& entry : std::filesystem::directory_iterator(genomesDir))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".genome")
                {
                    files.push_back(entry.path().filename().string());
                }
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error listing genome files: " << e.what() << std::endl;
        }
        
        return files;
    }
}
