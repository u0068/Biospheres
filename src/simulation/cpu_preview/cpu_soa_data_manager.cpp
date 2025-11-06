#include "cpu_soa_data_manager.h"
#include "cpu_soa_validation.h"
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <iostream>

CPUSoADataManager::CPUSoADataManager() {
    // Initialize free index pools
    m_freeCellIndices.reserve(256);
    m_freeConnectionIndices.reserve(1024);
}

CPUSoADataManager::~CPUSoADataManager() {
    // Cleanup handled by RAII
}

void CPUSoADataManager::createEmptyScene(size_t maxCells) {
    if (maxCells > 256) {
        throw std::invalid_argument("Maximum cell count cannot exceed 256 for CPU Preview System");
    }

    // Reset all data to zero
    m_cellData = CPUCellPhysics_SoA{};
    m_adhesionData = CPUAdhesionConnections_SoA{};
    
    // Reset free index pools
    m_freeCellIndices.clear();
    m_freeConnectionIndices.clear();
    
    // Pre-populate free indices
    for (uint32_t i = 0; i < 256; ++i) {
        m_freeCellIndices.push_back(i);
    }
    for (uint32_t i = 0; i < 1024; ++i) {
        m_freeConnectionIndices.push_back(i);
    }
    
    std::cout << "Created empty CPU SoA scene with capacity for " << maxCells << " cells\n";
}

void CPUSoADataManager::savePreviewScene(const std::string& filename) {
    try {
        serializeSoAData(filename);
        std::cout << "Saved CPU preview scene to " << filename << "\n";
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to save CPU preview scene: " << e.what() << std::endl;
        throw;
    }
}

void CPUSoADataManager::loadPreviewScene(const std::string& filename) {
    try {
        deserializeSoAData(filename);
        std::cout << "Loaded CPU preview scene from " << filename << "\n";
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to load CPU preview scene: " << e.what() << std::endl;
        throw;
    }
}

uint32_t CPUSoADataManager::addCell(const CPUCellParameters& params) {
    uint32_t index = allocateCellIndex();
    
    // Set position
    m_cellData.pos_x[index] = params.position.x;
    m_cellData.pos_y[index] = params.position.y;
    m_cellData.pos_z[index] = params.position.z;
    
    // Set velocity
    m_cellData.vel_x[index] = params.velocity.x;
    m_cellData.vel_y[index] = params.velocity.y;
    m_cellData.vel_z[index] = params.velocity.z;
    
    // Initialize acceleration to zero
    m_cellData.acc_x[index] = 0.0f;
    m_cellData.acc_y[index] = 0.0f;
    m_cellData.acc_z[index] = 0.0f;
    
    // Set orientation
    m_cellData.quat_x[index] = params.orientation.x;
    m_cellData.quat_y[index] = params.orientation.y;
    m_cellData.quat_z[index] = params.orientation.z;
    m_cellData.quat_w[index] = params.orientation.w;
    
    // Set physics properties
    m_cellData.mass[index] = params.mass;
    m_cellData.radius[index] = params.radius;
    m_cellData.age[index] = 0.0f;
    m_cellData.energy[index] = 1.0f;
    
    // Set cell state
    m_cellData.cellType[index] = params.cellType;
    m_cellData.genomeID[index] = params.genomeID;
    m_cellData.flags[index] = 0;
    
    m_cellData.activeCellCount++;
    
    return index;
}

void CPUSoADataManager::removeCell(uint32_t cellIndex) {
    if (cellIndex >= 256) {
        throw std::out_of_range("Cell index out of range");
    }
    
    // Zero out the cell data
    m_cellData.pos_x[cellIndex] = 0.0f;
    m_cellData.pos_y[cellIndex] = 0.0f;
    m_cellData.pos_z[cellIndex] = 0.0f;
    m_cellData.vel_x[cellIndex] = 0.0f;
    m_cellData.vel_y[cellIndex] = 0.0f;
    m_cellData.vel_z[cellIndex] = 0.0f;
    m_cellData.acc_x[cellIndex] = 0.0f;
    m_cellData.acc_y[cellIndex] = 0.0f;
    m_cellData.acc_z[cellIndex] = 0.0f;
    m_cellData.quat_x[cellIndex] = 0.0f;
    m_cellData.quat_y[cellIndex] = 0.0f;
    m_cellData.quat_z[cellIndex] = 0.0f;
    m_cellData.quat_w[cellIndex] = 1.0f;
    m_cellData.mass[cellIndex] = 0.0f;
    m_cellData.radius[cellIndex] = 0.0f;
    m_cellData.age[cellIndex] = 0.0f;
    m_cellData.energy[cellIndex] = 0.0f;
    m_cellData.cellType[cellIndex] = 0;
    m_cellData.genomeID[cellIndex] = 0;
    m_cellData.flags[cellIndex] = 0;
    
    deallocateCellIndex(cellIndex);
    
    if (m_cellData.activeCellCount > 0) {
        m_cellData.activeCellCount--;
    }
}

void CPUSoADataManager::addAdhesionConnection(uint32_t cellA, uint32_t cellB, const CPUAdhesionParameters& params) {
    if (cellA >= 256 || cellB >= 256) {
        throw std::out_of_range("Cell indices out of range");
    }
    
    uint32_t index = allocateConnectionIndex();
    
    m_adhesionData.cellA_indices[index] = cellA;
    m_adhesionData.cellB_indices[index] = cellB;
    m_adhesionData.anchor_dir_x[index] = params.anchorDirection.x;
    m_adhesionData.anchor_dir_y[index] = params.anchorDirection.y;
    m_adhesionData.anchor_dir_z[index] = params.anchorDirection.z;
    m_adhesionData.rest_length[index] = params.restLength;
    m_adhesionData.stiffness[index] = params.stiffness;
    m_adhesionData.twist_constraint[index] = params.twistConstraint;
    
    m_adhesionData.activeConnectionCount++;
}

void CPUSoADataManager::updateGenomeParameters(uint32_t cellIndex, const CPUGenomeParameters& params) {
    if (cellIndex >= 256) {
        throw std::out_of_range("Cell index out of range");
    }
    
    // Update cell properties based on genome parameters
    // This is where genome parameters would affect cell behavior
    // For now, we'll store the genome ID and let the physics engine handle the rest
    
    // Example: Update cell type flags based on genome
    m_cellData.flags[cellIndex] = params.cellTypeFlags;
    
    // Note: Full genome parameter application would be implemented here
    // based on the specific genome system requirements
}

void CPUSoADataManager::updateCellPosition(uint32_t cellIndex, const glm::vec3& position) {
    if (cellIndex >= 256) {
        throw std::out_of_range("Cell index out of range");
    }
    
    m_cellData.pos_x[cellIndex] = position.x;
    m_cellData.pos_y[cellIndex] = position.y;
    m_cellData.pos_z[cellIndex] = position.z;
}

void CPUSoADataManager::updateCellVelocity(uint32_t cellIndex, const glm::vec3& velocity) {
    if (cellIndex >= 256) {
        throw std::out_of_range("Cell index out of range");
    }
    
    m_cellData.vel_x[cellIndex] = velocity.x;
    m_cellData.vel_y[cellIndex] = velocity.y;
    m_cellData.vel_z[cellIndex] = velocity.z;
}

void CPUSoADataManager::validateDataIntegrity() {
    try {
        // Use comprehensive SoA validation
        CPUSoAValidation::runComprehensiveValidation(m_cellData, m_adhesionData);
    }
    catch (const std::exception& e) {
        std::cerr << "SoA data integrity validation failed: " << e.what() << std::endl;
        throw;
    }
}

void CPUSoADataManager::validateSoAStructures() {
    try {
        // Validate structure alignment and layout
        CPUSoAValidation::SoAStructureValidator::validateCellPhysicsStructure(m_cellData);
        CPUSoAValidation::SoAStructureValidator::validateAdhesionConnectionsStructure(m_adhesionData);
        
        std::cout << "✓ SoA structure validation passed\n";
    }
    catch (const std::exception& e) {
        std::cerr << "SoA structure validation failed: " << e.what() << std::endl;
        throw;
    }
}

void CPUSoADataManager::analyzePaddingEfficiency() {
    CPUSoAValidation::SoAStructureValidator::analyzePaddingEfficiency();
}

void CPUSoADataManager::runValidationTests() {
    std::cout << "=== Running CPU SoA Validation Tests ===\n\n";
    
    try {
        // Test 1: Structure validation on empty data
        std::cout << "Test 1: Structure validation on empty data\n";
        validateSoAStructures();
        std::cout << "✓ Empty data structure validation passed\n\n";
        
        // Test 2: Padding efficiency analysis
        std::cout << "Test 2: Padding efficiency analysis\n";
        analyzePaddingEfficiency();
        std::cout << "\n";
        
        // Test 3: Add some test data and validate
        std::cout << "Test 3: Data integrity validation with test data\n";
        
        // Create test cell
        CPUCellParameters testCell;
        testCell.position = glm::vec3(1.0f, 2.0f, 3.0f);
        testCell.velocity = glm::vec3(0.1f, 0.2f, 0.3f);
        testCell.orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity quaternion
        testCell.mass = 1.5f;
        testCell.radius = 0.5f;
        testCell.cellType = 1;
        testCell.genomeID = 100;
        
        uint32_t cellIndex1 = addCell(testCell);
        
        // Create second test cell
        testCell.position = glm::vec3(4.0f, 5.0f, 6.0f);
        testCell.velocity = glm::vec3(0.4f, 0.5f, 0.6f);
        uint32_t cellIndex2 = addCell(testCell);
        
        // Add adhesion connection
        CPUAdhesionParameters adhesionParams;
        adhesionParams.anchorDirection = glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f));
        adhesionParams.restLength = 2.0f;
        adhesionParams.stiffness = 10.0f;
        adhesionParams.twistConstraint = 1.0f;
        
        addAdhesionConnection(cellIndex1, cellIndex2, adhesionParams);
        
        // Validate data integrity
        validateDataIntegrity();
        std::cout << "✓ Data integrity validation with test data passed\n\n";
        
        // Test 4: Memory layout validation
        std::cout << "Test 4: Memory layout validation\n";
        CPUSoAValidation::validateMemoryLayout();
        std::cout << "\n";
        
        // Test 5: Performance analysis
        std::cout << "Test 5: Performance analysis\n";
        CPUSoAValidation::performanceAnalysis();
        std::cout << "\n";
        
        // Test 6: Bounds checking validation
        std::cout << "Test 6: Bounds checking validation\n";
        CPUSoAValidation::validateBoundsChecking(m_cellData, m_adhesionData);
        std::cout << "\n";
        
        // Test 7: Numerical stability validation
        std::cout << "Test 7: Numerical stability validation\n";
        CPUSoAValidation::validateNumericalStability(m_cellData);
        std::cout << "\n";
        
        // Test 8: SIMD compatibility validation
        std::cout << "Test 8: SIMD compatibility validation\n";
        CPUSoAValidation::validateSIMDCompatibility();
        std::cout << "\n";
        
        // Test 9: Detailed structure information
        std::cout << "Test 9: Detailed structure information\n";
        CPUSoAValidation::printDetailedStructureInfo();
        std::cout << "\n";
        
        // Clean up test data
        removeCell(cellIndex1);
        removeCell(cellIndex2);
        
        std::cout << "=== All CPU SoA Validation Tests Passed Successfully ===\n";
        
    }
    catch (const std::exception& e) {
        std::cerr << "❌ Validation test failed: " << e.what() << std::endl;
        throw;
    }
}

void CPUSoADataManager::compactArrays() {
    // For now, we use a simple free index system
    // More sophisticated compaction could be implemented if needed
    // This would involve moving active cells to fill gaps left by removed cells
}

uint32_t CPUSoADataManager::allocateCellIndex() {
    if (m_freeCellIndices.empty()) {
        throw std::runtime_error("No free cell indices available");
    }
    
    uint32_t index = m_freeCellIndices.back();
    m_freeCellIndices.pop_back();
    return index;
}

uint32_t CPUSoADataManager::allocateConnectionIndex() {
    if (m_freeConnectionIndices.empty()) {
        throw std::runtime_error("No free connection indices available");
    }
    
    uint32_t index = m_freeConnectionIndices.back();
    m_freeConnectionIndices.pop_back();
    return index;
}

void CPUSoADataManager::deallocateCellIndex(uint32_t index) {
    m_freeCellIndices.push_back(index);
}

void CPUSoADataManager::deallocateConnectionIndex(uint32_t index) {
    m_freeConnectionIndices.push_back(index);
}

void CPUSoADataManager::serializeSoAData(const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file for writing: " + filename);
    }
    
    // Write header
    const char* header = "CPU_SOA_PREVIEW_V1";
    file.write(header, 18);
    
    // Write cell data
    file.write(reinterpret_cast<const char*>(&m_cellData), sizeof(CPUCellPhysics_SoA));
    
    // Write adhesion data
    file.write(reinterpret_cast<const char*>(&m_adhesionData), sizeof(CPUAdhesionConnections_SoA));
    
    if (!file.good()) {
        throw std::runtime_error("Error writing to file: " + filename);
    }
}

void CPUSoADataManager::deserializeSoAData(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file for reading: " + filename);
    }
    
    // Read and validate header
    char header[19] = {0};
    file.read(header, 18);
    if (std::string(header) != "CPU_SOA_PREVIEW_V1") {
        throw std::runtime_error("Invalid file format or version: " + filename);
    }
    
    // Read cell data
    file.read(reinterpret_cast<char*>(&m_cellData), sizeof(CPUCellPhysics_SoA));
    
    // Read adhesion data
    file.read(reinterpret_cast<char*>(&m_adhesionData), sizeof(CPUAdhesionConnections_SoA));
    
    if (!file.good()) {
        throw std::runtime_error("Error reading from file: " + filename);
    }
    
    // Rebuild free index pools
    m_freeCellIndices.clear();
    m_freeConnectionIndices.clear();
    
    // Find free cell indices
    for (uint32_t i = 0; i < 256; ++i) {
        if (m_cellData.mass[i] == 0.0f) { // Use mass as indicator of active cell
            m_freeCellIndices.push_back(i);
        }
    }
    
    // Find free connection indices
    for (uint32_t i = 0; i < 1024; ++i) {
        if (i >= m_adhesionData.activeConnectionCount) {
            m_freeConnectionIndices.push_back(i);
        }
    }
}