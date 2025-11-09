#include "cpu_soa_data_manager.h"
#include "cpu_soa_validation.h"
#include "cpu_adhesion_connection_manager.h"
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <numeric>

CPUSoADataManager::CPUSoADataManager() {
    // Initialize free index pools
    m_freeCellIndices.reserve(256);
    m_freeConnectionIndices.reserve(1024);
    
    // Initialize connection management system (Requirements 10.1-10.5, 7.4, 7.5)
    m_connectionManager = std::make_unique<CPUAdhesionConnectionManager>();
    m_connectionManager->setCellData(&m_cellData);
    m_connectionManager->setAdhesionData(&m_adhesionData);
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
    
    // Efficiently reset free index pools without expensive push_back operations
    m_freeCellIndices.clear();
    m_freeConnectionIndices.clear();
    
    // Reserve capacity to avoid reallocations
    m_freeCellIndices.reserve(256);
    m_freeConnectionIndices.reserve(1024);
    
    // Use resize and iota for efficient initialization
    m_freeCellIndices.resize(256);
    m_freeConnectionIndices.resize(1024);
    
    // Fill with sequential indices
    std::iota(m_freeCellIndices.begin(), m_freeCellIndices.end(), 0);
    std::iota(m_freeConnectionIndices.begin(), m_freeConnectionIndices.end(), 0);
    
    // Empty scene created silently
}

void CPUSoADataManager::savePreviewScene(const std::string& filename) {
    try {
        serializeSoAData(filename);
        // Scene saved silently
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to save CPU preview scene: " << e.what() << std::endl;
        throw;
    }
}

void CPUSoADataManager::loadPreviewScene(const std::string& filename) {
    try {
        deserializeSoAData(filename);
        // Scene loaded silently
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
    
    // Initialize previous acceleration to zero (for Verlet integration)
    m_cellData.prevAcc_x[index] = 0.0f;
    m_cellData.prevAcc_y[index] = 0.0f;
    m_cellData.prevAcc_z[index] = 0.0f;
    
    // Initialize angular motion to zero
    m_cellData.angularVel_x[index] = 0.0f;
    m_cellData.angularVel_y[index] = 0.0f;
    m_cellData.angularVel_z[index] = 0.0f;
    m_cellData.angularAcc_x[index] = 0.0f;
    m_cellData.angularAcc_y[index] = 0.0f;
    m_cellData.angularAcc_z[index] = 0.0f;
    
    // Initialize previous angular acceleration to zero (for Verlet integration)
    m_cellData.prevAngularAcc_x[index] = 0.0f;
    m_cellData.prevAngularAcc_y[index] = 0.0f;
    m_cellData.prevAngularAcc_z[index] = 0.0f;
    
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
    
    // Apply genome parameters including color
    m_cellData.color_r[index] = params.genome.modeColor.r;
    m_cellData.color_g[index] = params.genome.modeColor.g;
    m_cellData.color_b[index] = params.genome.modeColor.b;
    
    // Initialize adhesion indices to -1 (empty slots)
    for (int i = 0; i < 20; ++i) {
        m_cellData.adhesionIndices[index][i] = -1;
    }
    
    m_cellData.activeCellCount++;
    
    return index;
}

void CPUSoADataManager::removeCell(uint32_t cellIndex) {
    if (cellIndex >= m_cellData.activeCellCount) {
        throw std::out_of_range("Cell index out of range or cell not active");
    }
    
    // Maintain contiguous storage by moving the last cell to fill the gap
    uint32_t lastIndex = m_cellData.activeCellCount - 1;
    
    if (cellIndex != lastIndex) {
        // Move last cell data to the removed cell's position
        m_cellData.pos_x[cellIndex] = m_cellData.pos_x[lastIndex];
        m_cellData.pos_y[cellIndex] = m_cellData.pos_y[lastIndex];
        m_cellData.pos_z[cellIndex] = m_cellData.pos_z[lastIndex];
        m_cellData.vel_x[cellIndex] = m_cellData.vel_x[lastIndex];
        m_cellData.vel_y[cellIndex] = m_cellData.vel_y[lastIndex];
        m_cellData.vel_z[cellIndex] = m_cellData.vel_z[lastIndex];
        m_cellData.acc_x[cellIndex] = m_cellData.acc_x[lastIndex];
        m_cellData.acc_y[cellIndex] = m_cellData.acc_y[lastIndex];
        m_cellData.acc_z[cellIndex] = m_cellData.acc_z[lastIndex];
        m_cellData.quat_x[cellIndex] = m_cellData.quat_x[lastIndex];
        m_cellData.quat_y[cellIndex] = m_cellData.quat_y[lastIndex];
        m_cellData.quat_z[cellIndex] = m_cellData.quat_z[lastIndex];
        m_cellData.quat_w[cellIndex] = m_cellData.quat_w[lastIndex];
        m_cellData.mass[cellIndex] = m_cellData.mass[lastIndex];
        m_cellData.radius[cellIndex] = m_cellData.radius[lastIndex];
        m_cellData.age[cellIndex] = m_cellData.age[lastIndex];
        m_cellData.energy[cellIndex] = m_cellData.energy[lastIndex];
        m_cellData.cellType[cellIndex] = m_cellData.cellType[lastIndex];
        m_cellData.genomeID[cellIndex] = m_cellData.genomeID[lastIndex];
        m_cellData.flags[cellIndex] = m_cellData.flags[lastIndex];
        m_cellData.color_r[cellIndex] = m_cellData.color_r[lastIndex];
        m_cellData.color_g[cellIndex] = m_cellData.color_g[lastIndex];
        m_cellData.color_b[cellIndex] = m_cellData.color_b[lastIndex];
        
        // Move adhesion indices
        for (int i = 0; i < 20; ++i) {
            m_cellData.adhesionIndices[cellIndex][i] = m_cellData.adhesionIndices[lastIndex][i];
        }
    }
    
    // Decrease active cell count (effectively removing the last cell)
    m_cellData.activeCellCount--;
}

void CPUSoADataManager::addAdhesionConnection(uint32_t cellA, uint32_t cellB, const CPUAdhesionParameters& params) {
    if (cellA >= 256 || cellB >= 256) {
        throw std::out_of_range("Cell indices out of range");
    }
    
    // Use the connection manager for proper slot management and validation (Requirements 10.1-10.5)
    if (m_connectionManager) {
        int connectionIndex = m_connectionManager->addAdhesionWithDirections(
            cellA, cellB, params.modeIndex,
            params.anchorDirectionA, params.anchorDirectionB
        );
        
        if (connectionIndex < 0) {
            throw std::runtime_error("Failed to create adhesion connection - no free slots or capacity exceeded");
        }
        
        // Update zone information and twist references (not handled by basic connection manager)
        m_adhesionData.zoneA[connectionIndex] = params.zoneA;
        m_adhesionData.zoneB[connectionIndex] = params.zoneB;
        
        // Set twist reference quaternions
        m_adhesionData.twistReferenceA_x[connectionIndex] = params.twistReferenceA.x;
        m_adhesionData.twistReferenceA_y[connectionIndex] = params.twistReferenceA.y;
        m_adhesionData.twistReferenceA_z[connectionIndex] = params.twistReferenceA.z;
        m_adhesionData.twistReferenceA_w[connectionIndex] = params.twistReferenceA.w;
        m_adhesionData.twistReferenceB_x[connectionIndex] = params.twistReferenceB.x;
        m_adhesionData.twistReferenceB_y[connectionIndex] = params.twistReferenceB.y;
        m_adhesionData.twistReferenceB_z[connectionIndex] = params.twistReferenceB.z;
        m_adhesionData.twistReferenceB_w[connectionIndex] = params.twistReferenceB.w;
    } else {
        // Fallback to old method if connection manager is not available
        uint32_t index = allocateConnectionIndex();
        
        m_adhesionData.cellAIndex[index] = cellA;
        m_adhesionData.cellBIndex[index] = cellB;
        m_adhesionData.modeIndex[index] = params.modeIndex;
        m_adhesionData.isActive[index] = 1;  // Active connection
        m_adhesionData.zoneA[index] = params.zoneA;
        m_adhesionData.zoneB[index] = params.zoneB;
        
        // Set anchor directions for both cells
        m_adhesionData.anchorDirectionA_x[index] = params.anchorDirectionA.x;
        m_adhesionData.anchorDirectionA_y[index] = params.anchorDirectionA.y;
        m_adhesionData.anchorDirectionA_z[index] = params.anchorDirectionA.z;
        m_adhesionData.anchorDirectionB_x[index] = params.anchorDirectionB.x;
        m_adhesionData.anchorDirectionB_y[index] = params.anchorDirectionB.y;
        m_adhesionData.anchorDirectionB_z[index] = params.anchorDirectionB.z;
        
        // Set twist reference quaternions
        m_adhesionData.twistReferenceA_x[index] = params.twistReferenceA.x;
        m_adhesionData.twistReferenceA_y[index] = params.twistReferenceA.y;
        m_adhesionData.twistReferenceA_z[index] = params.twistReferenceA.z;
        m_adhesionData.twistReferenceA_w[index] = params.twistReferenceA.w;
        m_adhesionData.twistReferenceB_x[index] = params.twistReferenceB.x;
        m_adhesionData.twistReferenceB_y[index] = params.twistReferenceB.y;
        m_adhesionData.twistReferenceB_z[index] = params.twistReferenceB.z;
        m_adhesionData.twistReferenceB_w[index] = params.twistReferenceB.w;
        
        m_adhesionData.activeConnectionCount++;
    }
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
        
        // Structure validation passed silently
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
        adhesionParams.anchorDirectionA = glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f));
        adhesionParams.anchorDirectionB = glm::normalize(glm::vec3(-1.0f, 0.0f, 0.0f));
        adhesionParams.modeIndex = 0;  // Default mode
        adhesionParams.zoneA = 0;      // Zone A
        adhesionParams.zoneB = 0;      // Zone A
        adhesionParams.twistReferenceA = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity quaternion
        adhesionParams.twistReferenceB = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity quaternion
        
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
        
        // Test 7: Enhanced adhesion structure validation
        std::cout << "Test 7: Enhanced adhesion structure validation\n";
        testEnhancedAdhesionStructure();
        
        // Test 8: Numerical stability validation
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
    // Use the next available index in the contiguous range
    // This ensures active cells are always stored at indices 0, 1, 2, etc.
    uint32_t index = m_cellData.activeCellCount;
    
    if (index >= 256) {
        throw std::runtime_error("Maximum cell capacity (256) exceeded");
    }
    
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
    // No longer needed with contiguous allocation
    // Cells are automatically "deallocated" by decreasing activeCellCount
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
    
    // Efficiently rebuild free index pools
    m_freeCellIndices.clear();
    m_freeConnectionIndices.clear();
    
    // Reserve capacity to avoid reallocations
    m_freeCellIndices.reserve(256);
    m_freeConnectionIndices.reserve(1024);
    
    // Find free cell indices efficiently
    for (uint32_t i = 0; i < 256; ++i) {
        if (m_cellData.mass[i] == 0.0f) { // Use mass as indicator of active cell
            m_freeCellIndices.push_back(i);
        }
    }
    
    // Find free connection indices efficiently
    for (uint32_t i = 0; i < 1024; ++i) {
        if (i >= m_adhesionData.activeConnectionCount) {
            m_freeConnectionIndices.push_back(i);
        }
    }
}

// Structure validation for enhanced adhesion support (Requirements 7.1, 7.2, 7.3)
void CPUSoADataManager::validateAdhesionStructureAlignment() {
    std::cout << "=== Enhanced Adhesion Structure Validation ===\n";
    
    // Validate structure alignment
    std::cout << "Structure alignment: " << alignof(CPUAdhesionConnections_SoA) << " bytes (required: 32)\n";
    if (alignof(CPUAdhesionConnections_SoA) < 32) {
        throw std::runtime_error("CPUAdhesionConnections_SoA does not meet 32-byte alignment requirement");
    }
    
    // Validate structure size alignment
    std::cout << "Structure size: " << sizeof(CPUAdhesionConnections_SoA) << " bytes\n";
    if (sizeof(CPUAdhesionConnections_SoA) % 32 != 0) {
        throw std::runtime_error("CPUAdhesionConnections_SoA size is not 32-byte aligned");
    }
    
    // Validate capacity
    constexpr size_t expectedCapacity = 5120; // 20 × 256 cells
    constexpr size_t actualCapacity = std::tuple_size_v<decltype(CPUAdhesionConnections_SoA::cellAIndex)>;
    std::cout << "Connection capacity: " << actualCapacity << " (required: " << expectedCapacity << ")\n";
    if (actualCapacity != expectedCapacity) {
        throw std::runtime_error("CPUAdhesionConnections_SoA capacity does not match requirement (5,120 connections)");
    }
    
    // Validate SIMD compatibility
    if (actualCapacity % 8 != 0) {
        throw std::runtime_error("Connection capacity is not SIMD-compatible (must be multiple of 8)");
    }
    
    std::cout << "✓ All adhesion structure alignment requirements met\n\n";
}

void CPUSoADataManager::analyzeAdhesionMemoryLayout() {
    std::cout << "=== Enhanced Adhesion Memory Layout Analysis ===\n";
    
    constexpr size_t capacity = 5120;
    constexpr size_t uint32Arrays = 6;  // cellAIndex, cellBIndex, modeIndex, isActive, zoneA, zoneB
    constexpr size_t floatArrays = 14;   // anchorDirection*2*3 + twistReference*2*4
    
    size_t uint32Memory = uint32Arrays * capacity * sizeof(uint32_t);
    size_t floatMemory = floatArrays * capacity * sizeof(float);
    size_t totalArrayMemory = uint32Memory + floatMemory;
    size_t actualStructSize = sizeof(CPUAdhesionConnections_SoA);
    size_t overhead = actualStructSize - totalArrayMemory;
    
    std::cout << "Array breakdown:\n";
    std::cout << "  Uint32 arrays: " << uint32Arrays << " × " << capacity << " × 4 bytes = " << uint32Memory << " bytes\n";
    std::cout << "  Float arrays: " << floatArrays << " × " << capacity << " × 4 bytes = " << floatMemory << " bytes\n";
    std::cout << "  Total array memory: " << totalArrayMemory << " bytes\n";
    std::cout << "  Actual structure size: " << actualStructSize << " bytes\n";
    std::cout << "  Overhead: " << overhead << " bytes (" << (overhead * 100.0 / actualStructSize) << "%)\n";
    
    // Memory efficiency
    float efficiency = (totalArrayMemory * 100.0f) / actualStructSize;
    std::cout << "  Memory efficiency: " << efficiency << "%\n";
    
    if (efficiency < 95.0f) {
        std::cout << "⚠ Warning: Memory efficiency below 95%, consider optimizing structure layout\n";
    } else {
        std::cout << "✓ Excellent memory efficiency (>95%)\n";
    }
    
    // SIMD analysis
    size_t totalElements = uint32Arrays * capacity + floatArrays * capacity;
    size_t simdBatches = totalElements / 8;
    size_t remainingElements = totalElements % 8;
    
    std::cout << "\nSIMD analysis:\n";
    std::cout << "  Total elements: " << totalElements << "\n";
    std::cout << "  SIMD batches (8 elements): " << simdBatches << "\n";
    std::cout << "  Remaining elements: " << remainingElements << "\n";
    std::cout << "  SIMD efficiency: " << ((simdBatches * 8.0) / totalElements * 100.0) << "%\n";
    
    std::cout << "\n";
}

// Test function to validate the enhanced structure
void CPUSoADataManager::testEnhancedAdhesionStructure() {
    std::cout << "=== Testing Enhanced Adhesion Structure ===\n";
    
    try {
        // Run structure validation
        validateAdhesionStructureAlignment();
        analyzeAdhesionMemoryLayout();
        
        // Test creating a connection with all new fields
        CPUAdhesionParameters testParams;
        testParams.anchorDirectionA = glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f));
        testParams.anchorDirectionB = glm::normalize(glm::vec3(-1.0f, 0.0f, 0.0f));
        testParams.modeIndex = 0;
        testParams.zoneA = 1; // Zone B
        testParams.zoneB = 2; // Zone C
        testParams.twistReferenceA = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        testParams.twistReferenceB = glm::quat(0.707f, 0.0f, 0.707f, 0.0f); // 90-degree rotation
        
        // Create test cells
        CPUCellParameters cellParams;
        cellParams.position = glm::vec3(0.0f, 0.0f, 0.0f);
        cellParams.velocity = glm::vec3(0.0f, 0.0f, 0.0f);
        cellParams.orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        cellParams.mass = 1.0f;
        cellParams.radius = 0.5f;
        cellParams.cellType = 0;
        cellParams.genomeID = 0;
        
        uint32_t cellA = addCell(cellParams);
        cellParams.position = glm::vec3(2.0f, 0.0f, 0.0f);
        uint32_t cellB = addCell(cellParams);
        
        // Add connection with enhanced parameters
        addAdhesionConnection(cellA, cellB, testParams);
        
        // Verify the connection was created correctly
        if (m_adhesionData.activeConnectionCount == 1) {
            uint32_t idx = 0;
            std::cout << "✓ Connection created successfully:\n";
            std::cout << "  Cell A: " << m_adhesionData.cellAIndex[idx] << ", Cell B: " << m_adhesionData.cellBIndex[idx] << "\n";
            std::cout << "  Mode Index: " << m_adhesionData.modeIndex[idx] << "\n";
            std::cout << "  Is Active: " << m_adhesionData.isActive[idx] << "\n";
            std::cout << "  Zone A: " << m_adhesionData.zoneA[idx] << ", Zone B: " << m_adhesionData.zoneB[idx] << "\n";
            std::cout << "  Anchor A: (" << m_adhesionData.anchorDirectionA_x[idx] << ", " 
                      << m_adhesionData.anchorDirectionA_y[idx] << ", " << m_adhesionData.anchorDirectionA_z[idx] << ")\n";
            std::cout << "  Anchor B: (" << m_adhesionData.anchorDirectionB_x[idx] << ", " 
                      << m_adhesionData.anchorDirectionB_y[idx] << ", " << m_adhesionData.anchorDirectionB_z[idx] << ")\n";
            std::cout << "  Twist Ref A: (" << m_adhesionData.twistReferenceA_x[idx] << ", " 
                      << m_adhesionData.twistReferenceA_y[idx] << ", " << m_adhesionData.twistReferenceA_z[idx] 
                      << ", " << m_adhesionData.twistReferenceA_w[idx] << ")\n";
            std::cout << "  Twist Ref B: (" << m_adhesionData.twistReferenceB_x[idx] << ", " 
                      << m_adhesionData.twistReferenceB_y[idx] << ", " << m_adhesionData.twistReferenceB_z[idx] 
                      << ", " << m_adhesionData.twistReferenceB_w[idx] << ")\n";
        } else {
            throw std::runtime_error("Failed to create adhesion connection");
        }
        
        std::cout << "✓ Enhanced adhesion structure test passed!\n\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Enhanced adhesion structure test failed: " << e.what() << "\n\n";
        throw;
    }
}

// Connection management validation (Requirements 10.4, 7.4, 7.5)
void CPUSoADataManager::validateConnectionIntegrity() {
    if (!m_connectionManager) {
        std::cerr << "Connection manager not initialized" << std::endl;
        return;
    }

    std::cout << "=== Validating Connection Integrity ===" << std::endl;
    
    // Run comprehensive connection validation
    auto integrityResult = m_connectionManager->validateConnectionIntegrity();
    m_connectionManager->printValidationReport(integrityResult);
    
    // Validate cell adhesion indices
    auto indicesResult = m_connectionManager->validateCellAdhesionIndices();
    if (!indicesResult.isValid) {
        std::cout << "\n=== Cell Adhesion Indices Validation ===" << std::endl;
        m_connectionManager->printValidationReport(indicesResult);
    }
    
    // Validate connection capacity
    auto capacityResult = m_connectionManager->validateConnectionCapacity();
    if (!capacityResult.isValid) {
        std::cout << "\n=== Connection Capacity Validation ===" << std::endl;
        m_connectionManager->printValidationReport(capacityResult);
    }
    
    // Print connection statistics
    std::cout << "\n";
    m_connectionManager->printConnectionStatistics();
}

void CPUSoADataManager::runConnectionManagerTests() {
    if (!m_connectionManager) {
        std::cerr << "Connection manager not initialized" << std::endl;
        return;
    }

    std::cout << "=== Running Connection Manager Tests ===" << std::endl;
    m_connectionManager->runComprehensiveTests();
}
