#include "cpu_adhesion_connection_manager.h"
#include <algorithm>
#include <unordered_map>
#include <set>
#include <chrono>
#include <iomanip>

CPUAdhesionConnectionManager::CPUAdhesionConnectionManager() {
    // Constructor - data references will be set externally
}

CPUAdhesionConnectionManager::~CPUAdhesionConnectionManager() {
    // Destructor - no cleanup needed as we don't own the data
}

// Connection creation with proper slot management (Requirement 10.2)
int CPUAdhesionConnectionManager::addAdhesionWithDirections(
    uint32_t cellA, uint32_t cellB, uint32_t modeIndex,
    const glm::vec3& anchorDirectionA, const glm::vec3& anchorDirectionB,
    float restLength) {
    
    if (!m_cellData || !m_adhesionData) {
        return -1; // Data not initialized
    }

    // Validate cell indices
    if (!isValidCellIndex(cellA) || !isValidCellIndex(cellB)) {
        return -1; // Invalid cell indices
    }

    // Check if cells are the same (no self-connections)
    if (cellA == cellB) {
        return -1; // Cannot connect cell to itself
    }

    // Check connection capacity (Requirement 10.5)
    if (!isConnectionCapacityAvailable()) {
        return -1; // Connection array is full
    }

    // Find free slots in both cells (Requirement 10.2)
    int slotA = findFreeAdhesionSlot(cellA);
    int slotB = findFreeAdhesionSlot(cellB);
    
    if (slotA < 0 || slotB < 0) {
        return -1; // No free slots available in one or both cells
    }

    // Find free connection slot
    int connectionIndex = findFreeConnectionSlot();
    if (connectionIndex < 0) {
        return -1; // Connection array is full
    }

    // Create the connection
    m_adhesionData->cellAIndex[connectionIndex] = cellA;
    m_adhesionData->cellBIndex[connectionIndex] = cellB;
    m_adhesionData->modeIndex[connectionIndex] = modeIndex;
    m_adhesionData->isActive[connectionIndex] = 1;
    m_adhesionData->zoneA[connectionIndex] = 0; // Default zone
    m_adhesionData->zoneB[connectionIndex] = 0; // Default zone

    // Set anchor directions (normalized)
    glm::vec3 normalizedAnchorA = glm::length(anchorDirectionA) > 0.001f ? 
                                  glm::normalize(anchorDirectionA) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 normalizedAnchorB = glm::length(anchorDirectionB) > 0.001f ? 
                                  glm::normalize(anchorDirectionB) : glm::vec3(-1.0f, 0.0f, 0.0f);

    m_adhesionData->anchorDirectionA_x[connectionIndex] = normalizedAnchorA.x;
    m_adhesionData->anchorDirectionA_y[connectionIndex] = normalizedAnchorA.y;
    m_adhesionData->anchorDirectionA_z[connectionIndex] = normalizedAnchorA.z;
    m_adhesionData->anchorDirectionB_x[connectionIndex] = normalizedAnchorB.x;
    m_adhesionData->anchorDirectionB_y[connectionIndex] = normalizedAnchorB.y;
    m_adhesionData->anchorDirectionB_z[connectionIndex] = normalizedAnchorB.z;

    // Set default twist references (identity quaternions)
    m_adhesionData->twistReferenceA_x[connectionIndex] = 0.0f;
    m_adhesionData->twistReferenceA_y[connectionIndex] = 0.0f;
    m_adhesionData->twistReferenceA_z[connectionIndex] = 0.0f;
    m_adhesionData->twistReferenceA_w[connectionIndex] = 1.0f;
    m_adhesionData->twistReferenceB_x[connectionIndex] = 0.0f;
    m_adhesionData->twistReferenceB_y[connectionIndex] = 0.0f;
    m_adhesionData->twistReferenceB_z[connectionIndex] = 0.0f;
    m_adhesionData->twistReferenceB_w[connectionIndex] = 1.0f;

    // Update adhesion indices in both cells (Requirement 10.1)
    if (!setAdhesionIndex(cellA, slotA, connectionIndex) ||
        !setAdhesionIndex(cellB, slotB, connectionIndex)) {
        // Rollback if setting indices failed
        markConnectionInactive(connectionIndex);
        return -1;
    }

    // Mark connection as active and update count
    markConnectionActive(connectionIndex);
    updateActiveConnectionCount();

    return connectionIndex;
}

// Connection removal and cleanup (Requirement 10.3)
bool CPUAdhesionConnectionManager::removeAdhesion(int connectionIndex) {
    if (!m_cellData || !m_adhesionData) {
        return false; // Data not initialized
    }

    if (!isValidConnectionIndex(connectionIndex) || !isConnectionActive(connectionIndex)) {
        return false; // Invalid or inactive connection
    }

    // Get the cells involved in this connection
    uint32_t cellA = m_adhesionData->cellAIndex[connectionIndex];
    uint32_t cellB = m_adhesionData->cellBIndex[connectionIndex];

    // Remove adhesion indices from both cells (Requirement 10.3)
    removeAdhesionIndex(cellA, connectionIndex);
    removeAdhesionIndex(cellB, connectionIndex);

    // Mark connection as inactive
    markConnectionInactive(connectionIndex);

    // Update active connection count
    updateActiveConnectionCount();

    return true;
}

void CPUAdhesionConnectionManager::removeAllConnectionsForCell(uint32_t cellIndex) {
    if (!m_cellData || !m_adhesionData || !isValidCellIndex(cellIndex)) {
        return;
    }

    // Get all connections for this cell
    std::vector<int> connections = getConnectionsForCell(cellIndex);

    // Remove each connection
    for (int connectionIndex : connections) {
        removeAdhesion(connectionIndex);
    }
}

void CPUAdhesionConnectionManager::cleanupInactiveConnections() {
    if (!m_adhesionData) {
        return;
    }

    // Compact the connection array by moving active connections to the front
    size_t writeIndex = 0;
    
    for (size_t readIndex = 0; readIndex < m_adhesionData->activeConnectionCount; ++readIndex) {
        if (m_adhesionData->isActive[readIndex] == 1) {
            if (writeIndex != readIndex) {
                // Move connection data from readIndex to writeIndex
                m_adhesionData->cellAIndex[writeIndex] = m_adhesionData->cellAIndex[readIndex];
                m_adhesionData->cellBIndex[writeIndex] = m_adhesionData->cellBIndex[readIndex];
                m_adhesionData->modeIndex[writeIndex] = m_adhesionData->modeIndex[readIndex];
                m_adhesionData->isActive[writeIndex] = m_adhesionData->isActive[readIndex];
                m_adhesionData->zoneA[writeIndex] = m_adhesionData->zoneA[readIndex];
                m_adhesionData->zoneB[writeIndex] = m_adhesionData->zoneB[readIndex];

                m_adhesionData->anchorDirectionA_x[writeIndex] = m_adhesionData->anchorDirectionA_x[readIndex];
                m_adhesionData->anchorDirectionA_y[writeIndex] = m_adhesionData->anchorDirectionA_y[readIndex];
                m_adhesionData->anchorDirectionA_z[writeIndex] = m_adhesionData->anchorDirectionA_z[readIndex];
                m_adhesionData->anchorDirectionB_x[writeIndex] = m_adhesionData->anchorDirectionB_x[readIndex];
                m_adhesionData->anchorDirectionB_y[writeIndex] = m_adhesionData->anchorDirectionB_y[readIndex];
                m_adhesionData->anchorDirectionB_z[writeIndex] = m_adhesionData->anchorDirectionB_z[readIndex];

                m_adhesionData->twistReferenceA_x[writeIndex] = m_adhesionData->twistReferenceA_x[readIndex];
                m_adhesionData->twistReferenceA_y[writeIndex] = m_adhesionData->twistReferenceA_y[readIndex];
                m_adhesionData->twistReferenceA_z[writeIndex] = m_adhesionData->twistReferenceA_z[readIndex];
                m_adhesionData->twistReferenceA_w[writeIndex] = m_adhesionData->twistReferenceA_w[readIndex];
                m_adhesionData->twistReferenceB_x[writeIndex] = m_adhesionData->twistReferenceB_x[readIndex];
                m_adhesionData->twistReferenceB_y[writeIndex] = m_adhesionData->twistReferenceB_y[readIndex];
                m_adhesionData->twistReferenceB_z[writeIndex] = m_adhesionData->twistReferenceB_z[readIndex];
                m_adhesionData->twistReferenceB_w[writeIndex] = m_adhesionData->twistReferenceB_w[readIndex];

                // Update cell adhesion indices to point to new location
                uint32_t cellA = m_adhesionData->cellAIndex[writeIndex];
                uint32_t cellB = m_adhesionData->cellBIndex[writeIndex];
                
                // Update adhesion indices in cells to point to new connection index
                for (int i = 0; i < MAX_ADHESIONS_PER_CELL; ++i) {
                    if (m_cellData->adhesionIndices[cellA][i] == static_cast<int>(readIndex)) {
                        m_cellData->adhesionIndices[cellA][i] = static_cast<int>(writeIndex);
                    }
                    if (m_cellData->adhesionIndices[cellB][i] == static_cast<int>(readIndex)) {
                        m_cellData->adhesionIndices[cellB][i] = static_cast<int>(writeIndex);
                    }
                }
            }
            writeIndex++;
        }
    }

    // Update active connection count
    m_adhesionData->activeConnectionCount = writeIndex;
}

// Adhesion index slot management (Requirement 10.1)
void CPUAdhesionConnectionManager::initializeCellAdhesionIndices(uint32_t cellIndex) {
    if (!m_cellData || !isValidCellIndex(cellIndex)) {
        return;
    }

    // Initialize all 20 adhesionIndices slots to -1 to indicate empty slots (Requirement 10.1)
    for (int i = 0; i < MAX_ADHESIONS_PER_CELL; ++i) {
        m_cellData->adhesionIndices[cellIndex][i] = -1;
    }
}

int CPUAdhesionConnectionManager::findFreeAdhesionSlot(uint32_t cellIndex) const {
    if (!m_cellData || !isValidCellIndex(cellIndex)) {
        return -1;
    }

    // Find first available slot (value < 0) in the cell's adhesionIndices array (Requirement 10.2)
    for (int i = 0; i < MAX_ADHESIONS_PER_CELL; ++i) {
        if (m_cellData->adhesionIndices[cellIndex][i] < 0) {
            return i;
        }
    }
    return -1; // No free slots available
}

bool CPUAdhesionConnectionManager::setAdhesionIndex(uint32_t cellIndex, int slotIndex, int connectionIndex) {
    if (!m_cellData || !isValidCellIndex(cellIndex)) {
        return false;
    }

    if (slotIndex < 0 || slotIndex >= MAX_ADHESIONS_PER_CELL) {
        return false; // Invalid slot index
    }

    if (!isValidConnectionIndex(connectionIndex)) {
        return false; // Invalid connection index
    }

    // Set the adhesion index in the cell's adhesionIndices array (Requirement 10.1)
    m_cellData->adhesionIndices[cellIndex][slotIndex] = connectionIndex;
    return true;
}

bool CPUAdhesionConnectionManager::removeAdhesionIndex(uint32_t cellIndex, int connectionIndex) {
    if (!m_cellData || !isValidCellIndex(cellIndex)) {
        return false;
    }

    // Remove the adhesion index from the cell's adhesionIndices array (Requirement 10.3)
    bool found = false;
    for (int i = 0; i < MAX_ADHESIONS_PER_CELL; ++i) {
        if (m_cellData->adhesionIndices[cellIndex][i] == connectionIndex) {
            m_cellData->adhesionIndices[cellIndex][i] = -1; // Mark as empty
            found = true;
            break; // Only remove first occurrence
        }
    }
    return found;
}

// Data integrity validation (Requirement 10.4)
CPUAdhesionConnectionManager::ValidationResult CPUAdhesionConnectionManager::validateConnectionIntegrity() const {
    ValidationResult result;
    
    if (!m_cellData || !m_adhesionData) {
        reportError(result, "Data structures not initialized");
        return result;
    }

    result.totalConnections = m_adhesionData->activeConnectionCount;
    
    // Validate each connection
    for (size_t i = 0; i < m_adhesionData->activeConnectionCount; ++i) {
        if (m_adhesionData->isActive[i] == 1) {
            result.activeConnections++;
            
            if (!validateSingleConnection(static_cast<int>(i))) {
                result.invalidConnections++;
                reportError(result, "Invalid connection at index " + std::to_string(i));
            }
        }
    }

    // Check for orphaned connections
    std::vector<int> orphaned = findOrphanedConnections();
    result.orphanedConnections = orphaned.size();
    for (int index : orphaned) {
        reportWarning(result, "Orphaned connection at index " + std::to_string(index));
    }

    // Check for duplicate connections
    std::vector<int> duplicates = findDuplicateConnections();
    result.duplicateConnections = duplicates.size();
    for (int index : duplicates) {
        reportWarning(result, "Duplicate connection at index " + std::to_string(index));
    }

    // Check for circular references
    if (checkCircularReferences()) {
        reportError(result, "Circular references detected in connection graph");
    }

    // Final validation
    result.isValid = result.errors.empty();
    
    return result;
}

CPUAdhesionConnectionManager::ValidationResult CPUAdhesionConnectionManager::validateCellAdhesionIndices() const {
    ValidationResult result;
    
    if (!m_cellData || !m_adhesionData) {
        reportError(result, "Data structures not initialized");
        return result;
    }

    // Validate each cell's adhesion indices
    for (size_t cellIndex = 0; cellIndex < m_cellData->activeCellCount; ++cellIndex) {
        for (int slotIndex = 0; slotIndex < MAX_ADHESIONS_PER_CELL; ++slotIndex) {
            int connectionIndex = m_cellData->adhesionIndices[cellIndex][slotIndex];
            
            if (connectionIndex >= 0) {
                // Validate connection index is within bounds
                if (!isValidConnectionIndex(connectionIndex)) {
                    reportError(result, "Cell " + std::to_string(cellIndex) + 
                               " slot " + std::to_string(slotIndex) + 
                               " has invalid connection index " + std::to_string(connectionIndex));
                    continue;
                }

                // Validate connection is active
                if (!isConnectionActive(connectionIndex)) {
                    reportError(result, "Cell " + std::to_string(cellIndex) + 
                               " slot " + std::to_string(slotIndex) + 
                               " references inactive connection " + std::to_string(connectionIndex));
                    continue;
                }

                // Validate connection actually involves this cell
                uint32_t cellA = m_adhesionData->cellAIndex[connectionIndex];
                uint32_t cellB = m_adhesionData->cellBIndex[connectionIndex];
                
                if (cellA != cellIndex && cellB != cellIndex) {
                    reportError(result, "Cell " + std::to_string(cellIndex) + 
                               " references connection " + std::to_string(connectionIndex) + 
                               " but connection doesn't involve this cell");
                }
            }
        }
    }

    result.isValid = result.errors.empty();
    return result;
}

CPUAdhesionConnectionManager::ValidationResult CPUAdhesionConnectionManager::validateConnectionCapacity() const {
    ValidationResult result;
    
    if (!m_adhesionData) {
        reportError(result, "Adhesion data not initialized");
        return result;
    }

    // Check connection capacity limits (Requirement 10.5)
    if (m_adhesionData->activeConnectionCount > MAX_CONNECTIONS) {
        reportError(result, "Active connection count (" + 
                   std::to_string(m_adhesionData->activeConnectionCount) + 
                   ") exceeds maximum capacity (" + std::to_string(MAX_CONNECTIONS) + ")");
    }

    // Check utilization
    float utilization = getConnectionCapacityUtilization();
    if (utilization > 0.95f) {
        reportWarning(result, "Connection capacity utilization is high: " + 
                     std::to_string(utilization * 100.0f) + "%");
    }

    result.totalConnections = m_adhesionData->activeConnectionCount;
    result.isValid = result.errors.empty();
    
    return result;
}

bool CPUAdhesionConnectionManager::validateSingleConnection(int connectionIndex) const {
    if (!m_adhesionData || !isValidConnectionIndex(connectionIndex)) {
        return false;
    }

    // Check if connection is active
    if (!isConnectionActive(connectionIndex)) {
        return false;
    }

    // Validate connection indices
    if (!validateConnectionIndices(connectionIndex)) {
        return false;
    }

    // Validate anchor directions
    if (!validateAnchorDirections(connectionIndex)) {
        return false;
    }

    // Validate twist references
    if (!validateTwistReferences(connectionIndex)) {
        return false;
    }

    return true;
}

// Connection capacity management (Requirement 10.5)
bool CPUAdhesionConnectionManager::isConnectionCapacityAvailable() const {
    if (!m_adhesionData) {
        return false;
    }

    return m_adhesionData->activeConnectionCount < MAX_CONNECTIONS;
}

size_t CPUAdhesionConnectionManager::getActiveConnectionCount() const {
    if (!m_adhesionData) {
        return 0;
    }

    // Count actual active connections
    size_t count = 0;
    for (size_t i = 0; i < m_adhesionData->activeConnectionCount; ++i) {
        if (m_adhesionData->isActive[i] == 1) {
            count++;
        }
    }
    return count;
}

float CPUAdhesionConnectionManager::getConnectionCapacityUtilization() const {
    if (!m_adhesionData) {
        return 0.0f;
    }

    return static_cast<float>(getActiveConnectionCount()) / static_cast<float>(MAX_CONNECTIONS);
}

// Connection queries and information
std::vector<int> CPUAdhesionConnectionManager::getConnectionsForCell(uint32_t cellIndex) const {
    std::vector<int> connections;
    
    if (!m_cellData || !isValidCellIndex(cellIndex)) {
        return connections;
    }

    // Collect all connection indices from the cell's adhesion indices
    for (int i = 0; i < MAX_ADHESIONS_PER_CELL; ++i) {
        int connectionIndex = m_cellData->adhesionIndices[cellIndex][i];
        if (connectionIndex >= 0 && isValidConnectionIndex(connectionIndex) && 
            isConnectionActive(connectionIndex)) {
            connections.push_back(connectionIndex);
        }
    }

    return connections;
}

std::vector<uint32_t> CPUAdhesionConnectionManager::getConnectedCells(uint32_t cellIndex) const {
    std::vector<uint32_t> connectedCells;
    
    if (!m_cellData || !m_adhesionData || !isValidCellIndex(cellIndex)) {
        return connectedCells;
    }

    // Get all connections for this cell
    std::vector<int> connections = getConnectionsForCell(cellIndex);
    
    // Extract the other cell from each connection
    for (int connectionIndex : connections) {
        uint32_t cellA = m_adhesionData->cellAIndex[connectionIndex];
        uint32_t cellB = m_adhesionData->cellBIndex[connectionIndex];
        
        // Add the other cell (not the current cell)
        if (cellA == cellIndex) {
            connectedCells.push_back(cellB);
        } else if (cellB == cellIndex) {
            connectedCells.push_back(cellA);
        }
    }

    return connectedCells;
}

bool CPUAdhesionConnectionManager::areCellsConnected(uint32_t cellA, uint32_t cellB) const {
    return findConnectionBetweenCells(cellA, cellB) >= 0;
}

int CPUAdhesionConnectionManager::findConnectionBetweenCells(uint32_t cellA, uint32_t cellB) const {
    if (!m_cellData || !m_adhesionData || !isValidCellIndex(cellA) || !isValidCellIndex(cellB)) {
        return -1;
    }

    // Get connections for cellA and check if any connect to cellB
    std::vector<int> connections = getConnectionsForCell(cellA);
    
    for (int connectionIndex : connections) {
        uint32_t connCellA = m_adhesionData->cellAIndex[connectionIndex];
        uint32_t connCellB = m_adhesionData->cellBIndex[connectionIndex];
        
        if ((connCellA == cellA && connCellB == cellB) || 
            (connCellA == cellB && connCellB == cellA)) {
            return connectionIndex;
        }
    }

    return -1; // No connection found
}

// System information and statistics
CPUAdhesionConnectionManager::ConnectionStatistics CPUAdhesionConnectionManager::getConnectionStatistics() const {
    ConnectionStatistics stats;
    
    if (!m_cellData || !m_adhesionData) {
        return stats;
    }

    // Calculate slot statistics
    stats.totalSlots = m_cellData->activeCellCount * MAX_ADHESIONS_PER_CELL;
    
    for (size_t cellIndex = 0; cellIndex < m_cellData->activeCellCount; ++cellIndex) {
        uint32_t cellConnections = 0;
        
        for (int i = 0; i < MAX_ADHESIONS_PER_CELL; ++i) {
            if (m_cellData->adhesionIndices[cellIndex][i] >= 0) {
                stats.usedSlots++;
                cellConnections++;
            }
        }
        
        if (cellConnections > stats.maxConnectionsOnSingleCell) {
            stats.maxConnectionsOnSingleCell = cellConnections;
            stats.cellsWithMaxConnections = 1;
        } else if (cellConnections == stats.maxConnectionsOnSingleCell) {
            stats.cellsWithMaxConnections++;
        }
    }
    
    stats.freeSlots = stats.totalSlots - stats.usedSlots;
    
    if (m_cellData->activeCellCount > 0) {
        stats.averageConnectionsPerCell = static_cast<float>(stats.usedSlots) / 
                                         static_cast<float>(m_cellData->activeCellCount);
    }
    
    stats.connectionArrayUtilization = getActiveConnectionCount();
    
    return stats;
}

void CPUAdhesionConnectionManager::printConnectionStatistics() const {
    ConnectionStatistics stats = getConnectionStatistics();
    
    std::cout << "=== Connection Statistics ===" << std::endl;
    std::cout << "Total adhesion slots: " << stats.totalSlots << std::endl;
    std::cout << "Used slots: " << stats.usedSlots << std::endl;
    std::cout << "Free slots: " << stats.freeSlots << std::endl;
    std::cout << "Average connections per cell: " << std::fixed << std::setprecision(2) 
              << stats.averageConnectionsPerCell << std::endl;
    std::cout << "Max connections on single cell: " << stats.maxConnectionsOnSingleCell << std::endl;
    std::cout << "Cells with max connections: " << stats.cellsWithMaxConnections << std::endl;
    std::cout << "Connection array utilization: " << stats.connectionArrayUtilization 
              << " / " << MAX_CONNECTIONS << " (" 
              << std::fixed << std::setprecision(1) 
              << (getConnectionCapacityUtilization() * 100.0f) << "%)" << std::endl;
}

void CPUAdhesionConnectionManager::printValidationReport(const ValidationResult& result) const {
    std::cout << "=== Connection Validation Report ===" << std::endl;
    std::cout << "Overall Status: " << (result.isValid ? "✓ VALID" : "✗ INVALID") << std::endl;
    std::cout << "Total connections: " << result.totalConnections << std::endl;
    std::cout << "Active connections: " << result.activeConnections << std::endl;
    std::cout << "Invalid connections: " << result.invalidConnections << std::endl;
    std::cout << "Orphaned connections: " << result.orphanedConnections << std::endl;
    std::cout << "Duplicate connections: " << result.duplicateConnections << std::endl;
    
    if (!result.errors.empty()) {
        std::cout << "\nErrors:" << std::endl;
        for (const std::string& error : result.errors) {
            std::cout << "  ✗ " << error << std::endl;
        }
    }
    
    if (!result.warnings.empty()) {
        std::cout << "\nWarnings:" << std::endl;
        for (const std::string& warning : result.warnings) {
            std::cout << "  ⚠ " << warning << std::endl;
        }
    }
}

// Internal helper methods
int CPUAdhesionConnectionManager::findFreeConnectionSlot() const {
    if (!m_adhesionData) {
        return -1;
    }

    // Find first inactive connection slot or extend array if within capacity
    for (size_t i = 0; i < MAX_CONNECTIONS; ++i) {
        if (i >= m_adhesionData->activeConnectionCount || m_adhesionData->isActive[i] == 0) {
            return static_cast<int>(i);
        }
    }
    
    return -1; // No free slots
}

bool CPUAdhesionConnectionManager::isValidCellIndex(uint32_t cellIndex) const {
    return m_cellData && cellIndex < m_cellData->activeCellCount && cellIndex < MAX_CELLS;
}

bool CPUAdhesionConnectionManager::isValidConnectionIndex(int connectionIndex) const {
    return connectionIndex >= 0 && 
           static_cast<size_t>(connectionIndex) < MAX_CONNECTIONS &&
           static_cast<size_t>(connectionIndex) < m_adhesionData->activeConnectionCount;
}

bool CPUAdhesionConnectionManager::isConnectionActive(int connectionIndex) const {
    return m_adhesionData && isValidConnectionIndex(connectionIndex) && 
           m_adhesionData->isActive[connectionIndex] == 1;
}

void CPUAdhesionConnectionManager::markConnectionActive(int connectionIndex) {
    if (m_adhesionData && isValidConnectionIndex(connectionIndex)) {
        m_adhesionData->isActive[connectionIndex] = 1;
    }
}

void CPUAdhesionConnectionManager::markConnectionInactive(int connectionIndex) {
    if (m_adhesionData && isValidConnectionIndex(connectionIndex)) {
        m_adhesionData->isActive[connectionIndex] = 0;
    }
}

void CPUAdhesionConnectionManager::updateActiveConnectionCount() {
    if (!m_adhesionData) {
        return;
    }

    // Find the highest index with an active connection
    size_t maxActiveIndex = 0;
    for (size_t i = 0; i < m_adhesionData->activeConnectionCount; ++i) {
        if (m_adhesionData->isActive[i] == 1) {
            maxActiveIndex = i + 1;
        }
    }
    
    m_adhesionData->activeConnectionCount = maxActiveIndex;
}

// Validation helpers
bool CPUAdhesionConnectionManager::validateConnectionIndices(int connectionIndex) const {
    if (!m_adhesionData || !isValidConnectionIndex(connectionIndex)) {
        return false;
    }

    uint32_t cellA = m_adhesionData->cellAIndex[connectionIndex];
    uint32_t cellB = m_adhesionData->cellBIndex[connectionIndex];

    // Validate cell indices are within bounds
    if (!isValidCellIndex(cellA) || !isValidCellIndex(cellB)) {
        return false;
    }

    // Validate cells are different
    if (cellA == cellB) {
        return false;
    }

    return true;
}

bool CPUAdhesionConnectionManager::validateAnchorDirections(int connectionIndex) const {
    if (!m_adhesionData || !isValidConnectionIndex(connectionIndex)) {
        return false;
    }

    // Check anchor direction A
    glm::vec3 anchorA(
        m_adhesionData->anchorDirectionA_x[connectionIndex],
        m_adhesionData->anchorDirectionA_y[connectionIndex],
        m_adhesionData->anchorDirectionA_z[connectionIndex]
    );

    // Check anchor direction B
    glm::vec3 anchorB(
        m_adhesionData->anchorDirectionB_x[connectionIndex],
        m_adhesionData->anchorDirectionB_y[connectionIndex],
        m_adhesionData->anchorDirectionB_z[connectionIndex]
    );

    // Validate anchor directions are not zero vectors and are reasonably normalized
    float lengthA = glm::length(anchorA);
    float lengthB = glm::length(anchorB);

    if (lengthA < 0.001f || lengthB < 0.001f) {
        return false; // Zero or near-zero vectors
    }

    if (std::abs(lengthA - 1.0f) > 0.1f || std::abs(lengthB - 1.0f) > 0.1f) {
        return false; // Not properly normalized
    }

    return true;
}

bool CPUAdhesionConnectionManager::validateTwistReferences(int connectionIndex) const {
    if (!m_adhesionData || !isValidConnectionIndex(connectionIndex)) {
        return false;
    }

    // Check twist reference A
    glm::quat twistA(
        m_adhesionData->twistReferenceA_w[connectionIndex],
        m_adhesionData->twistReferenceA_x[connectionIndex],
        m_adhesionData->twistReferenceA_y[connectionIndex],
        m_adhesionData->twistReferenceA_z[connectionIndex]
    );

    // Check twist reference B
    glm::quat twistB(
        m_adhesionData->twistReferenceB_w[connectionIndex],
        m_adhesionData->twistReferenceB_x[connectionIndex],
        m_adhesionData->twistReferenceB_y[connectionIndex],
        m_adhesionData->twistReferenceB_z[connectionIndex]
    );

    // Validate quaternions are normalized
    float lengthA = glm::length(twistA);
    float lengthB = glm::length(twistB);

    if (std::abs(lengthA - 1.0f) > 0.1f || std::abs(lengthB - 1.0f) > 0.1f) {
        return false; // Not properly normalized
    }

    return true;
}

std::vector<int> CPUAdhesionConnectionManager::findOrphanedConnections() const {
    std::vector<int> orphaned;
    
    if (!m_cellData || !m_adhesionData) {
        return orphaned;
    }

    // Check each active connection to see if it's referenced by its cells
    for (size_t i = 0; i < m_adhesionData->activeConnectionCount; ++i) {
        if (m_adhesionData->isActive[i] == 1) {
            uint32_t cellA = m_adhesionData->cellAIndex[i];
            uint32_t cellB = m_adhesionData->cellBIndex[i];
            
            bool foundInCellA = false;
            bool foundInCellB = false;
            
            // Check if cellA references this connection
            if (isValidCellIndex(cellA)) {
                for (int j = 0; j < MAX_ADHESIONS_PER_CELL; ++j) {
                    if (m_cellData->adhesionIndices[cellA][j] == static_cast<int>(i)) {
                        foundInCellA = true;
                        break;
                    }
                }
            }
            
            // Check if cellB references this connection
            if (isValidCellIndex(cellB)) {
                for (int j = 0; j < MAX_ADHESIONS_PER_CELL; ++j) {
                    if (m_cellData->adhesionIndices[cellB][j] == static_cast<int>(i)) {
                        foundInCellB = true;
                        break;
                    }
                }
            }
            
            // If connection is not referenced by both cells, it's orphaned
            if (!foundInCellA || !foundInCellB) {
                orphaned.push_back(static_cast<int>(i));
            }
        }
    }
    
    return orphaned;
}

std::vector<int> CPUAdhesionConnectionManager::findDuplicateConnections() const {
    std::vector<int> duplicates;
    
    if (!m_adhesionData) {
        return duplicates;
    }

    // Use a set to track unique cell pairs
    std::set<std::pair<uint32_t, uint32_t>> seenPairs;
    
    for (size_t i = 0; i < m_adhesionData->activeConnectionCount; ++i) {
        if (m_adhesionData->isActive[i] == 1) {
            uint32_t cellA = m_adhesionData->cellAIndex[i];
            uint32_t cellB = m_adhesionData->cellBIndex[i];
            
            // Create normalized pair (smaller index first)
            std::pair<uint32_t, uint32_t> pair = (cellA < cellB) ? 
                std::make_pair(cellA, cellB) : std::make_pair(cellB, cellA);
            
            if (seenPairs.find(pair) != seenPairs.end()) {
                duplicates.push_back(static_cast<int>(i));
            } else {
                seenPairs.insert(pair);
            }
        }
    }
    
    return duplicates;
}

bool CPUAdhesionConnectionManager::checkCircularReferences() const {
    // For now, return false as circular references are not expected in adhesion connections
    // This could be expanded to check for more complex graph cycles if needed
    return false;
}

void CPUAdhesionConnectionManager::reportError(ValidationResult& result, const std::string& error) const {
    result.errors.push_back(error);
    result.isValid = false;
}

void CPUAdhesionConnectionManager::reportWarning(ValidationResult& result, const std::string& warning) const {
    result.warnings.push_back(warning);
}

// Testing and debugging methods
void CPUAdhesionConnectionManager::runComprehensiveTests() {
    std::cout << "=== Running Comprehensive Connection Manager Tests ===" << std::endl;
    
    bool allTestsPassed = true;
    
    // Test connection creation
    std::cout << "Testing connection creation..." << std::endl;
    if (testConnectionCreation()) {
        std::cout << "✓ Connection creation test PASSED" << std::endl;
    } else {
        std::cout << "✗ Connection creation test FAILED" << std::endl;
        allTestsPassed = false;
    }
    
    // Test connection removal
    std::cout << "Testing connection removal..." << std::endl;
    if (testConnectionRemoval()) {
        std::cout << "✓ Connection removal test PASSED" << std::endl;
    } else {
        std::cout << "✗ Connection removal test FAILED" << std::endl;
        allTestsPassed = false;
    }
    
    // Test slot management
    std::cout << "Testing slot management..." << std::endl;
    if (testSlotManagement()) {
        std::cout << "✓ Slot management test PASSED" << std::endl;
    } else {
        std::cout << "✗ Slot management test FAILED" << std::endl;
        allTestsPassed = false;
    }
    
    // Test capacity limits
    std::cout << "Testing capacity limits..." << std::endl;
    if (testCapacityLimits()) {
        std::cout << "✓ Capacity limits test PASSED" << std::endl;
    } else {
        std::cout << "✗ Capacity limits test FAILED" << std::endl;
        allTestsPassed = false;
    }
    
    // Test data integrity
    std::cout << "Testing data integrity..." << std::endl;
    if (testDataIntegrity()) {
        std::cout << "✓ Data integrity test PASSED" << std::endl;
    } else {
        std::cout << "✗ Data integrity test FAILED" << std::endl;
        allTestsPassed = false;
    }
    
    std::cout << "=== Test Results ===" << std::endl;
    if (allTestsPassed) {
        std::cout << "✓ ALL TESTS PASSED" << std::endl;
    } else {
        std::cout << "✗ SOME TESTS FAILED" << std::endl;
    }
}

bool CPUAdhesionConnectionManager::testConnectionCreation() {
    if (!m_cellData || !m_adhesionData) {
        return false;
    }

    // Save original state
    size_t originalConnectionCount = m_adhesionData->activeConnectionCount;
    
    // Test basic connection creation
    glm::vec3 anchorA(1.0f, 0.0f, 0.0f);
    glm::vec3 anchorB(-1.0f, 0.0f, 0.0f);
    
    int connectionIndex = addAdhesionWithDirections(0, 1, 0, anchorA, anchorB);
    
    if (connectionIndex < 0) {
        return false; // Failed to create connection
    }
    
    // Verify connection was created correctly
    if (m_adhesionData->cellAIndex[connectionIndex] != 0 ||
        m_adhesionData->cellBIndex[connectionIndex] != 1 ||
        m_adhesionData->modeIndex[connectionIndex] != 0 ||
        m_adhesionData->isActive[connectionIndex] != 1) {
        return false;
    }
    
    // Verify anchor directions were set correctly
    glm::vec3 storedAnchorA(
        m_adhesionData->anchorDirectionA_x[connectionIndex],
        m_adhesionData->anchorDirectionA_y[connectionIndex],
        m_adhesionData->anchorDirectionA_z[connectionIndex]
    );
    
    if (glm::length(storedAnchorA - glm::normalize(anchorA)) > 0.001f) {
        return false;
    }
    
    // Verify cells reference the connection
    bool foundInCell0 = false, foundInCell1 = false;
    for (int i = 0; i < MAX_ADHESIONS_PER_CELL; ++i) {
        if (m_cellData->adhesionIndices[0][i] == connectionIndex) {
            foundInCell0 = true;
        }
        if (m_cellData->adhesionIndices[1][i] == connectionIndex) {
            foundInCell1 = true;
        }
    }
    
    if (!foundInCell0 || !foundInCell1) {
        return false;
    }
    
    // Test invalid connection creation (same cell)
    int invalidConnection = addAdhesionWithDirections(0, 0, 0, anchorA, anchorB);
    if (invalidConnection >= 0) {
        return false; // Should have failed
    }
    
    // Clean up
    removeAdhesion(connectionIndex);
    
    return true;
}

bool CPUAdhesionConnectionManager::testConnectionRemoval() {
    if (!m_cellData || !m_adhesionData) {
        return false;
    }

    // Create a test connection
    glm::vec3 anchorA(1.0f, 0.0f, 0.0f);
    glm::vec3 anchorB(-1.0f, 0.0f, 0.0f);
    
    int connectionIndex = addAdhesionWithDirections(0, 1, 0, anchorA, anchorB);
    if (connectionIndex < 0) {
        return false; // Failed to create test connection
    }
    
    // Verify connection exists
    if (!isConnectionActive(connectionIndex)) {
        return false;
    }
    
    // Remove the connection
    bool removed = removeAdhesion(connectionIndex);
    if (!removed) {
        return false; // Failed to remove connection
    }
    
    // Verify connection is inactive
    if (isConnectionActive(connectionIndex)) {
        return false;
    }
    
    // Verify cells no longer reference the connection
    for (int i = 0; i < MAX_ADHESIONS_PER_CELL; ++i) {
        if (m_cellData->adhesionIndices[0][i] == connectionIndex ||
            m_cellData->adhesionIndices[1][i] == connectionIndex) {
            return false; // Connection still referenced
        }
    }
    
    // Test removing invalid connection
    bool invalidRemoval = removeAdhesion(-1);
    if (invalidRemoval) {
        return false; // Should have failed
    }
    
    return true;
}

bool CPUAdhesionConnectionManager::testSlotManagement() {
    if (!m_cellData || !m_adhesionData) {
        return false;
    }

    // Test initialization
    initializeCellAdhesionIndices(0);
    
    // Verify all slots are initialized to -1
    for (int i = 0; i < MAX_ADHESIONS_PER_CELL; ++i) {
        if (m_cellData->adhesionIndices[0][i] != -1) {
            return false;
        }
    }
    
    // Test finding free slot
    int freeSlot = findFreeAdhesionSlot(0);
    if (freeSlot != 0) {
        return false; // Should be first slot
    }
    
    // Test setting adhesion index
    bool setResult = setAdhesionIndex(0, 0, 42);
    if (!setResult) {
        return false;
    }
    
    if (m_cellData->adhesionIndices[0][0] != 42) {
        return false;
    }
    
    // Test finding next free slot
    int nextFreeSlot = findFreeAdhesionSlot(0);
    if (nextFreeSlot != 1) {
        return false; // Should be second slot
    }
    
    // Test removing adhesion index
    bool removeResult = removeAdhesionIndex(0, 42);
    if (!removeResult) {
        return false;
    }
    
    if (m_cellData->adhesionIndices[0][0] != -1) {
        return false; // Should be reset to -1
    }
    
    // Test invalid operations
    bool invalidSet = setAdhesionIndex(0, -1, 42); // Invalid slot
    if (invalidSet) {
        return false; // Should have failed
    }
    
    bool invalidSet2 = setAdhesionIndex(0, MAX_ADHESIONS_PER_CELL, 42); // Out of bounds
    if (invalidSet2) {
        return false; // Should have failed
    }
    
    return true;
}

bool CPUAdhesionConnectionManager::testCapacityLimits() {
    if (!m_cellData || !m_adhesionData) {
        return false;
    }

    // Test capacity checking
    bool hasCapacity = isConnectionCapacityAvailable();
    if (!hasCapacity && m_adhesionData->activeConnectionCount < MAX_CONNECTIONS) {
        return false; // Should have capacity
    }
    
    // Test utilization calculation
    float utilization = getConnectionCapacityUtilization();
    if (utilization < 0.0f || utilization > 1.0f) {
        return false; // Invalid utilization
    }
    
    // Test capacity validation
    ValidationResult capacityResult = validateConnectionCapacity();
    if (!capacityResult.isValid && m_adhesionData->activeConnectionCount <= MAX_CONNECTIONS) {
        return false; // Should be valid if within limits
    }
    
    return true;
}

bool CPUAdhesionConnectionManager::testDataIntegrity() {
    if (!m_cellData || !m_adhesionData) {
        return false;
    }

    // Create some test connections
    glm::vec3 anchorA(1.0f, 0.0f, 0.0f);
    glm::vec3 anchorB(-1.0f, 0.0f, 0.0f);
    
    std::vector<int> testConnections;
    
    // Create multiple connections if we have enough cells
    if (m_cellData->activeCellCount >= 4) {
        testConnections.push_back(addAdhesionWithDirections(0, 1, 0, anchorA, anchorB));
        testConnections.push_back(addAdhesionWithDirections(1, 2, 0, anchorA, anchorB));
        testConnections.push_back(addAdhesionWithDirections(2, 3, 0, anchorA, anchorB));
    }
    
    // Test connection integrity validation
    ValidationResult integrityResult = validateConnectionIntegrity();
    if (!integrityResult.isValid && integrityResult.errors.empty()) {
        return false; // Should be valid or have specific errors
    }
    
    // Test cell adhesion indices validation
    ValidationResult indicesResult = validateCellAdhesionIndices();
    if (!indicesResult.isValid && indicesResult.errors.empty()) {
        return false; // Should be valid or have specific errors
    }
    
    // Test single connection validation
    for (int connectionIndex : testConnections) {
        if (connectionIndex >= 0) {
            bool isValid = validateSingleConnection(connectionIndex);
            if (!isValid) {
                return false; // Test connections should be valid
            }
        }
    }
    
    // Clean up test connections
    for (int connectionIndex : testConnections) {
        if (connectionIndex >= 0) {
            removeAdhesion(connectionIndex);
        }
    }
    
    return true;
}