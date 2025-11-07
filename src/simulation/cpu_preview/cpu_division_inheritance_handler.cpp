#include "cpu_division_inheritance_handler.h"
#include <chrono>
#include <cmath>
#include <algorithm>
#include <iostream>

CPUDivisionInheritanceHandler::CPUDivisionInheritanceHandler() {
    // Initialize metrics
    m_lastMetrics = InheritanceMetrics{};
}

CPUDivisionInheritanceHandler::~CPUDivisionInheritanceHandler() {
    // Cleanup handled by RAII
}

void CPUDivisionInheritanceHandler::inheritAdhesionsOnDivision(
    uint32_t parentCellIndex,
    uint32_t childACellIndex, uint32_t childBCellIndex,
    const glm::vec3& splitPlane, const glm::vec3& splitOffset,
    const glm::quat& orientationA, const glm::quat& orientationB,
    bool childAKeepAdhesion, bool childBKeepAdhesion,
    CPUCellPhysics_SoA& cells,
    CPUAdhesionConnections_SoA& adhesions,
    const std::vector<GPUModeAdhesionSettings>& modeSettings) {
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Reset metrics
    m_lastMetrics = InheritanceMetrics{};
    
    // Initialize adhesion indices for child cells (Requirement 10.4)
    initializeAdhesionIndices(childACellIndex, cells);
    initializeAdhesionIndices(childBCellIndex, cells);
    
    // Get parent cell properties
    glm::vec3 parentPos(cells.pos_x[parentCellIndex], cells.pos_y[parentCellIndex], cells.pos_z[parentCellIndex]);
    float parentRadius = cells.radius[parentCellIndex];
    
    // Get child positions
    glm::vec3 childAPos(cells.pos_x[childACellIndex], cells.pos_y[childACellIndex], cells.pos_z[childACellIndex]);
    glm::vec3 childBPos(cells.pos_x[childBCellIndex], cells.pos_y[childBCellIndex], cells.pos_z[childBCellIndex]);
    
    // Collect parent's adhesion connections
    std::vector<uint32_t> parentConnections;
    
    // Find all connections involving the parent cell
    for (size_t i = 0; i < adhesions.activeConnectionCount; ++i) {
        if (adhesions.isActive[i] == 0) continue;
        
        if (adhesions.cellAIndex[i] == parentCellIndex || adhesions.cellBIndex[i] == parentCellIndex) {
            parentConnections.push_back(static_cast<uint32_t>(i));
        }
    }
    
    m_lastMetrics.parentConnectionsProcessed = parentConnections.size();
    
    // Process each parent connection for inheritance
    for (uint32_t connectionIndex : parentConnections) {
        // Determine which cell is the neighbor (not the parent)
        uint32_t neighborIndex;
        bool parentIsA = (adhesions.cellAIndex[connectionIndex] == parentCellIndex);
        
        if (parentIsA) {
            neighborIndex = adhesions.cellBIndex[connectionIndex];
        } else {
            neighborIndex = adhesions.cellAIndex[connectionIndex];
        }
        
        // Get neighbor position and properties
        glm::vec3 neighborPos(cells.pos_x[neighborIndex], cells.pos_y[neighborIndex], cells.pos_z[neighborIndex]);
        float neighborRadius = cells.radius[neighborIndex];
        
        // Calculate bond direction from parent to neighbor
        glm::vec3 bondDirection = glm::normalize(neighborPos - parentPos);
        
        // Classify bond into zones using 2-degree threshold (Requirement 8.1)
        AdhesionZone zone = classifyBondDirection(bondDirection, splitPlane);
        
        // Update zone statistics
        switch (zone) {
            case AdhesionZone::ZoneA: m_lastMetrics.zoneAConnections++; break;
            case AdhesionZone::ZoneB: m_lastMetrics.zoneBConnections++; break;
            case AdhesionZone::ZoneC: m_lastMetrics.zoneCConnections++; break;
        }
        
        // Get original connection properties
        uint32_t modeIndex = adhesions.modeIndex[connectionIndex];
        glm::vec3 originalAnchorA(
            adhesions.anchorDirectionA_x[connectionIndex],
            adhesions.anchorDirectionA_y[connectionIndex],
            adhesions.anchorDirectionA_z[connectionIndex]
        );
        glm::vec3 originalAnchorB(
            adhesions.anchorDirectionB_x[connectionIndex],
            adhesions.anchorDirectionB_y[connectionIndex],
            adhesions.anchorDirectionB_z[connectionIndex]
        );
        
        // Get rest length from mode settings
        float restLength = 1.0f; // Default
        if (modeIndex < modeSettings.size()) {
            restLength = modeSettings[modeIndex].restLength;
        }
        
        // Get parent and neighbor genome orientations for proper anchor calculation
        // In CPU SoA, genome orientation is stored as quat_x, quat_y, quat_z, quat_w
        glm::quat parentOrientation(
            cells.quat_w[parentCellIndex],
            cells.quat_x[parentCellIndex],
            cells.quat_y[parentCellIndex],
            cells.quat_z[parentCellIndex]
        );
        glm::quat neighborOrientation(
            cells.quat_w[neighborIndex],
            cells.quat_x[neighborIndex],
            cells.quat_y[neighborIndex],
            cells.quat_z[neighborIndex]
        );
        
        // Get local anchor direction in parent's frame
        glm::vec3 localAnchorDirection = parentIsA ? originalAnchorA : originalAnchorB;
        
        // Calculate center-to-center distance using parent's adhesion rest length
        float centerToCenterDist = restLength + parentRadius + neighborRadius;
        
        // Apply inheritance rules based on zone classification (Requirements 8.2, 8.3, 8.4)
        if (zone == AdhesionZone::ZoneA && childBKeepAdhesion) {
            // Zone A to child B (Requirement 8.2)
            
            // Calculate positions in parent frame for geometric anchor placement
            glm::vec3 childBPos_parentFrame = -splitPlane * glm::length(splitOffset);
            glm::vec3 neighborPos_parentFrame = localAnchorDirection * centerToCenterDist;
            
            // Child anchor: direction from child to neighbor, transformed by genome orientation
            glm::vec3 directionToNeighbor_parentFrame = glm::normalize(neighborPos_parentFrame - childBPos_parentFrame);
            glm::vec3 childAnchorDirection = glm::normalize(glm::inverse(orientationB) * directionToNeighbor_parentFrame);
            
            // Neighbor anchor: direction from neighbor to child, transformed to neighbor's frame
            glm::vec3 directionToChild_parentFrame = glm::normalize(childBPos_parentFrame - neighborPos_parentFrame);
            glm::quat relativeRotation = glm::inverse(neighborOrientation) * parentOrientation;
            glm::vec3 neighborAnchorDirection = glm::normalize(relativeRotation * directionToChild_parentFrame);
            
            // Preserve original side assignment: if neighbor was originally cellA, keep them as cellA
            int newConnectionIndex;
            if (parentIsA) {
                // Parent was cellA, neighbor was cellB, so neighbor becomes cellB
                newConnectionIndex = addAdhesionWithDirections(
                    childBCellIndex, neighborIndex,
                    childAnchorDirection, neighborAnchorDirection,
                    modeIndex, cells, adhesions
                );
            } else {
                // Parent was cellB, neighbor was cellA, so neighbor becomes cellA
                newConnectionIndex = addAdhesionWithDirections(
                    neighborIndex, childBCellIndex,
                    neighborAnchorDirection, childAnchorDirection,
                    modeIndex, cells, adhesions
                );
            }
            
            if (newConnectionIndex >= 0) {
                m_lastMetrics.childBInheritedConnections++;
            }
        }
        else if (zone == AdhesionZone::ZoneB && childAKeepAdhesion) {
            // Zone B to child A (Requirement 8.3)
            
            // Calculate positions in parent frame for geometric anchor placement
            glm::vec3 childAPos_parentFrame = splitPlane * glm::length(splitOffset);
            glm::vec3 neighborPos_parentFrame = localAnchorDirection * centerToCenterDist;
            
            // Child anchor: direction from child to neighbor, transformed by genome orientation
            glm::vec3 directionToNeighbor_parentFrame = glm::normalize(neighborPos_parentFrame - childAPos_parentFrame);
            glm::vec3 childAnchorDirection = glm::normalize(glm::inverse(orientationA) * directionToNeighbor_parentFrame);
            
            // Neighbor anchor: direction from neighbor to child, transformed to neighbor's frame
            glm::vec3 directionToChild_parentFrame = glm::normalize(childAPos_parentFrame - neighborPos_parentFrame);
            glm::quat relativeRotation = glm::inverse(neighborOrientation) * parentOrientation;
            glm::vec3 neighborAnchorDirection = glm::normalize(relativeRotation * directionToChild_parentFrame);
            
            // Preserve original side assignment: if neighbor was originally cellA, keep them as cellA
            int newConnectionIndex;
            if (parentIsA) {
                // Parent was cellA, neighbor was cellB, so neighbor becomes cellB
                newConnectionIndex = addAdhesionWithDirections(
                    childACellIndex, neighborIndex,
                    childAnchorDirection, neighborAnchorDirection,
                    modeIndex, cells, adhesions
                );
            } else {
                // Parent was cellB, neighbor was cellA, so neighbor becomes cellA
                newConnectionIndex = addAdhesionWithDirections(
                    neighborIndex, childACellIndex,
                    neighborAnchorDirection, childAnchorDirection,
                    modeIndex, cells, adhesions
                );
            }
            
            if (newConnectionIndex >= 0) {
                m_lastMetrics.childAInheritedConnections++;
            }
        }
        else if (zone == AdhesionZone::ZoneC) {
            // Zone C to both children (Requirement 8.4)
            if (childAKeepAdhesion) {
                // Calculate positions in parent frame for geometric anchor placement
                glm::vec3 childAPos_parentFrame = splitPlane * glm::length(splitOffset);
                glm::vec3 neighborPos_parentFrame = localAnchorDirection * centerToCenterDist;
                
                // Child anchor: direction from child to neighbor, transformed by genome orientation
                glm::vec3 directionToNeighbor_parentFrame = glm::normalize(neighborPos_parentFrame - childAPos_parentFrame);
                glm::vec3 childAnchorDirection = glm::normalize(glm::inverse(orientationA) * directionToNeighbor_parentFrame);
                
                // Neighbor anchor: direction from neighbor to child, transformed to neighbor's frame
                glm::vec3 directionToChild_parentFrame = glm::normalize(childAPos_parentFrame - neighborPos_parentFrame);
                glm::quat relativeRotation = glm::inverse(neighborOrientation) * parentOrientation;
                glm::vec3 neighborAnchorDirection = glm::normalize(relativeRotation * directionToChild_parentFrame);
                
                // Preserve original side assignment
                int newConnectionIndex;
                if (parentIsA) {
                    newConnectionIndex = addAdhesionWithDirections(
                        childACellIndex, neighborIndex,
                        childAnchorDirection, neighborAnchorDirection,
                        modeIndex, cells, adhesions
                    );
                } else {
                    newConnectionIndex = addAdhesionWithDirections(
                        neighborIndex, childACellIndex,
                        neighborAnchorDirection, childAnchorDirection,
                        modeIndex, cells, adhesions
                    );
                }
                
                if (newConnectionIndex >= 0) {
                    m_lastMetrics.childAInheritedConnections++;
                }
            }
            
            if (childBKeepAdhesion) {
                // Calculate positions in parent frame for geometric anchor placement
                glm::vec3 childBPos_parentFrame = -splitPlane * glm::length(splitOffset);
                glm::vec3 neighborPos_parentFrame = localAnchorDirection * centerToCenterDist;
                
                // Child anchor: direction from child to neighbor, transformed by genome orientation
                glm::vec3 directionToNeighbor_parentFrame = glm::normalize(neighborPos_parentFrame - childBPos_parentFrame);
                glm::vec3 childAnchorDirection = glm::normalize(glm::inverse(orientationB) * directionToNeighbor_parentFrame);
                
                // Neighbor anchor: direction from neighbor to child, transformed to neighbor's frame
                glm::vec3 directionToChild_parentFrame = glm::normalize(childBPos_parentFrame - neighborPos_parentFrame);
                glm::quat relativeRotation = glm::inverse(neighborOrientation) * parentOrientation;
                glm::vec3 neighborAnchorDirection = glm::normalize(relativeRotation * directionToChild_parentFrame);
                
                // Preserve original side assignment
                int newConnectionIndex;
                if (parentIsA) {
                    newConnectionIndex = addAdhesionWithDirections(
                        childBCellIndex, neighborIndex,
                        childAnchorDirection, neighborAnchorDirection,
                        modeIndex, cells, adhesions
                    );
                } else {
                    newConnectionIndex = addAdhesionWithDirections(
                        neighborIndex, childBCellIndex,
                        neighborAnchorDirection, childAnchorDirection,
                        modeIndex, cells, adhesions
                    );
                }
                
                if (newConnectionIndex >= 0) {
                    m_lastMetrics.childBInheritedConnections++;
                }
            }
        }
        
        // Mark original connection as inactive (Requirement 10.3)
        adhesions.isActive[connectionIndex] = 0;
    }
    
    // Create child-to-child connection using parent's mode settings (Requirement 8.5)
    // Get parent's mode index to check parentMakeAdhesion flag
    uint32_t parentModeIndex = 0; // Default
    if (parentCellIndex < cells.activeCellCount) {
        // Mode index should be stored in cells data - for now use first mode
        // In GPU version this comes from cell.modeIndex
        parentModeIndex = 0;
    }
    
    // Check if parent mode allows child-to-child adhesion creation
    bool parentMakeAdhesion = true; // Default to true if mode settings not available
    if (parentModeIndex < modeSettings.size()) {
        // Note: GPUModeAdhesionSettings doesn't have parentMakeAdhesion field yet
        // This should be added to match GPU implementation
        // For now, assume true
        parentMakeAdhesion = true;
    }
    
    if (childAKeepAdhesion && childBKeepAdhesion && parentMakeAdhesion) {
        // Calculate child-to-child anchor directions using split direction from mode
        // This matches GPU implementation in cell_update_internal.comp
        
        // Split direction in parent's local frame (from mode settings)
        glm::vec3 splitDirLocal = glm::normalize(splitPlane);
        
        // Direction vectors in parent's local frame
        // Child A is at +offset, child B is at -offset
        glm::vec3 directionAtoB_parentLocal = -splitDirLocal;  // A points toward B (at -offset)
        glm::vec3 directionBtoA_parentLocal = splitDirLocal;   // B points toward A (at +offset)
        
        // Transform to each child's local space using genome-derived orientation deltas
        glm::quat invDeltaA = glm::inverse(orientationA);
        glm::vec3 anchorDirectionA = glm::normalize(invDeltaA * directionAtoB_parentLocal);
        
        glm::quat invDeltaB = glm::inverse(orientationB);
        glm::vec3 anchorDirectionB = glm::normalize(invDeltaB * directionBtoA_parentLocal);
        
        // Classify zones using genome-derived anchors (matches GPU implementation)
        AdhesionZone childZoneA = classifyBondDirection(anchorDirectionA, splitPlane);
        AdhesionZone childZoneB = classifyBondDirection(anchorDirectionB, splitPlane);
        
        // Create child-to-child connection with parent's mode index
        int childConnectionIndex = addAdhesionWithDirections(
            childACellIndex, childBCellIndex,
            anchorDirectionA, anchorDirectionB,
            parentModeIndex, cells, adhesions
        );
        
        if (childConnectionIndex >= 0) {
            // Update zone information in the connection
            adhesions.zoneA[childConnectionIndex] = static_cast<uint32_t>(childZoneA);
            adhesions.zoneB[childConnectionIndex] = static_cast<uint32_t>(childZoneB);
            
            m_lastMetrics.childToChildConnections++;
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    m_lastMetrics.processingTimeMs = duration.count() / 1000.0f;
}

CPUDivisionInheritanceHandler::AdhesionZone CPUDivisionInheritanceHandler::classifyBondDirection(
    const glm::vec3& bondDirection, 
    const glm::vec3& splitPlane) {
    
    // Calculate dot product with split plane normal
    float dotProduct = glm::dot(bondDirection, splitPlane);
    
    // Calculate angle from the equatorial plane (90 degrees to split plane)
    float angle = std::acos(std::abs(dotProduct));
    float angleFromEquator = std::abs(angle - (3.14159f / 2.0f)); // |angle - 90°|
    
    // Check if within equatorial threshold (2 degrees)
    if (angleFromEquator <= EQUATORIAL_THRESHOLD_RADIANS) {
        return AdhesionZone::ZoneC; // Equatorial band
    }
    
    // Classify based on which side of the split plane
    if (dotProduct < 0.0f) {
        return AdhesionZone::ZoneA; // Negative side -> inherit to child B
    } else {
        return AdhesionZone::ZoneB; // Positive side -> inherit to child A
    }
}

glm::vec3 CPUDivisionInheritanceHandler::calculateChildAnchorDirection(
    const glm::vec3& parentAnchor,
    const glm::vec3& neighborPos,
    const glm::vec3& childPos,
    float restLength,
    float parentRadius,
    float neighborRadius,
    const glm::quat& genomeOrientation) {
    
    // Calculate geometric relationship (Requirement 9.1, 9.2)
    glm::vec3 childToNeighbor = neighborPos - childPos;
    float distance = glm::length(childToNeighbor);
    
    // Handle degenerate case
    if (distance < 0.001f) {
        return glm::normalize(parentAnchor);
    }
    
    // Calculate expected distance based on radii and rest length
    float expectedDistance = restLength + parentRadius + neighborRadius;
    
    // Normalize direction
    glm::vec3 direction = childToNeighbor / distance;
    
    // Transform to child local space using inverse genome orientation (Requirement 9.3)
    glm::quat inverseOrientation = glm::inverse(genomeOrientation);
    glm::vec3 localDirection = inverseOrientation * direction;
    
    return glm::normalize(localDirection);
}

int CPUDivisionInheritanceHandler::addAdhesionWithDirections(
    uint32_t cellA, uint32_t cellB,
    const glm::vec3& anchorDirectionA, const glm::vec3& anchorDirectionB,
    uint32_t modeIndex,
    CPUCellPhysics_SoA& cells,
    CPUAdhesionConnections_SoA& adhesions) {
    
    // Find free slots in both cells (Requirement 10.2)
    int slotA = findFreeAdhesionSlot(cellA, cells);
    int slotB = findFreeAdhesionSlot(cellB, cells);
    
    if (slotA < 0 || slotB < 0) {
        return -1; // No free slots available (Requirement 10.5)
    }
    
    // Find free connection slot
    int connectionIndex = findFreeConnectionSlot(adhesions);
    if (connectionIndex < 0) {
        return -1; // Connection array is full
    }
    
    // Create the connection
    adhesions.cellAIndex[connectionIndex] = cellA;
    adhesions.cellBIndex[connectionIndex] = cellB;
    adhesions.modeIndex[connectionIndex] = modeIndex;
    adhesions.isActive[connectionIndex] = 1;
    adhesions.zoneA[connectionIndex] = 0; // Default zone
    adhesions.zoneB[connectionIndex] = 0; // Default zone
    
    // Set anchor directions
    adhesions.anchorDirectionA_x[connectionIndex] = anchorDirectionA.x;
    adhesions.anchorDirectionA_y[connectionIndex] = anchorDirectionA.y;
    adhesions.anchorDirectionA_z[connectionIndex] = anchorDirectionA.z;
    adhesions.anchorDirectionB_x[connectionIndex] = anchorDirectionB.x;
    adhesions.anchorDirectionB_y[connectionIndex] = anchorDirectionB.y;
    adhesions.anchorDirectionB_z[connectionIndex] = anchorDirectionB.z;
    
    // Set default twist references (identity quaternions)
    adhesions.twistReferenceA_x[connectionIndex] = 0.0f;
    adhesions.twistReferenceA_y[connectionIndex] = 0.0f;
    adhesions.twistReferenceA_z[connectionIndex] = 0.0f;
    adhesions.twistReferenceA_w[connectionIndex] = 1.0f;
    adhesions.twistReferenceB_x[connectionIndex] = 0.0f;
    adhesions.twistReferenceB_y[connectionIndex] = 0.0f;
    adhesions.twistReferenceB_z[connectionIndex] = 0.0f;
    adhesions.twistReferenceB_w[connectionIndex] = 1.0f;
    
    // Update adhesion indices in both cells (Requirement 10.1)
    setAdhesionIndex(cellA, slotA, connectionIndex, cells);
    setAdhesionIndex(cellB, slotB, connectionIndex, cells);
    
    // Update connection count
    if (static_cast<size_t>(connectionIndex) >= adhesions.activeConnectionCount) {
        adhesions.activeConnectionCount = static_cast<size_t>(connectionIndex) + 1;
    }
    
    return connectionIndex;
}

int CPUDivisionInheritanceHandler::findFreeAdhesionSlot(uint32_t cellIndex, const CPUCellPhysics_SoA& cells) {
    // Find first available slot (value < 0) in the cell's adhesionIndices array (Requirement 10.2)
    for (int i = 0; i < MAX_ADHESIONS_PER_CELL; ++i) {
        if (cells.adhesionIndices[cellIndex][i] < 0) {
            return i;
        }
    }
    return -1; // No free slots available
}

void CPUDivisionInheritanceHandler::setAdhesionIndex(uint32_t cellIndex, int slotIndex, int connectionIndex, CPUCellPhysics_SoA& cells) {
    // Set the adhesion index in the cell's adhesionIndices array (Requirement 10.1)
    if (slotIndex >= 0 && slotIndex < MAX_ADHESIONS_PER_CELL) {
        cells.adhesionIndices[cellIndex][slotIndex] = connectionIndex;
    }
}

void CPUDivisionInheritanceHandler::removeAdhesionIndex(uint32_t cellIndex, int connectionIndex, CPUCellPhysics_SoA& cells) {
    // Remove the adhesion index from the cell's adhesionIndices array (Requirement 10.3)
    for (int i = 0; i < MAX_ADHESIONS_PER_CELL; ++i) {
        if (cells.adhesionIndices[cellIndex][i] == connectionIndex) {
            cells.adhesionIndices[cellIndex][i] = -1; // Mark as empty
            break;
        }
    }
}

void CPUDivisionInheritanceHandler::initializeAdhesionIndices(uint32_t cellIndex, CPUCellPhysics_SoA& cells) {
    // Initialize all 20 adhesionIndices slots to -1 to indicate empty slots (Requirement 10.1)
    for (int i = 0; i < MAX_ADHESIONS_PER_CELL; ++i) {
        cells.adhesionIndices[cellIndex][i] = -1;
    }
}

int CPUDivisionInheritanceHandler::findFreeConnectionSlot(const CPUAdhesionConnections_SoA& adhesions) {
    // Find first inactive connection slot
    for (size_t i = 0; i < MAX_CONNECTIONS; ++i) {
        if (i >= adhesions.activeConnectionCount || adhesions.isActive[i] == 0) {
            return static_cast<int>(i);
        }
    }
    return -1; // No free slots
}

void CPUDivisionInheritanceHandler::testDivisionInheritance(
    CPUCellPhysics_SoA& cells,
    CPUAdhesionConnections_SoA& adhesions) {
    
    std::cout << "=== Testing Division Inheritance System ===\n";
    
    // Create test scenario with 3 cells: parent (0), neighbor (1), and space for child (2)
    if (cells.activeCellCount < 2) {
        std::cout << "❌ Need at least 2 cells for division inheritance test\n";
        return;
    }
    
    // Set up parent cell at origin
    uint32_t parentIndex = 0;
    cells.pos_x[parentIndex] = 0.0f;
    cells.pos_y[parentIndex] = 0.0f;
    cells.pos_z[parentIndex] = 0.0f;
    cells.radius[parentIndex] = 1.0f;
    
    // Set up neighbor cell
    uint32_t neighborIndex = 1;
    cells.pos_x[neighborIndex] = 3.0f; // Positioned to the right
    cells.pos_y[neighborIndex] = 0.0f;
    cells.pos_z[neighborIndex] = 0.0f;
    cells.radius[neighborIndex] = 1.0f;
    
    // Create adhesion connection between parent and neighbor
    glm::vec3 anchorA = glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f)); // Point toward neighbor
    glm::vec3 anchorB = glm::normalize(glm::vec3(-1.0f, 0.0f, 0.0f)); // Point toward parent
    
    int connectionIndex = addAdhesionWithDirections(
        parentIndex, neighborIndex,
        anchorA, anchorB,
        0, // Default mode
        cells, adhesions
    );
    
    if (connectionIndex < 0) {
        std::cout << "❌ Failed to create test adhesion connection\n";
        return;
    }
    
    std::cout << "✓ Created test connection between parent (0) and neighbor (1)\n";
    std::cout << "  Connection index: " << connectionIndex << "\n";
    std::cout << "  Parent anchor: (" << anchorA.x << ", " << anchorA.y << ", " << anchorA.z << ")\n";
    std::cout << "  Neighbor anchor: (" << anchorB.x << ", " << anchorB.y << ", " << anchorB.z << ")\n";
    
    // Create child cell (simulating division)
    uint32_t childIndex = 2;
    if (cells.activeCellCount <= childIndex) {
        cells.activeCellCount = childIndex + 1;
    }
    
    // Position child cells as if division occurred along X axis
    cells.pos_x[parentIndex] = 0.5f;  // Child A (parent index)
    cells.pos_y[parentIndex] = 0.0f;
    cells.pos_z[parentIndex] = 0.0f;
    cells.radius[parentIndex] = 1.0f;
    
    cells.pos_x[childIndex] = -0.5f;  // Child B (new index)
    cells.pos_y[childIndex] = 0.0f;
    cells.pos_z[childIndex] = 0.0f;
    cells.radius[childIndex] = 1.0f;
    
    // Initialize child adhesion indices
    initializeAdhesionIndices(childIndex, cells);
    
    std::cout << "✓ Set up division scenario:\n";
    std::cout << "  Child A (parent index 0) at: (0.5, 0, 0)\n";
    std::cout << "  Child B (new index 2) at: (-0.5, 0, 0)\n";
    std::cout << "  Neighbor (index 1) at: (3, 0, 0)\n";
    
    // Test zone classification
    glm::vec3 bondDirection = glm::normalize(glm::vec3(3.0f, 0.0f, 0.0f) - glm::vec3(0.0f, 0.0f, 0.0f));
    glm::vec3 splitPlane = glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f)); // Y axis (perpendicular to X split)
    
    AdhesionZone zone = classifyBondDirection(bondDirection, splitPlane);
    std::cout << "✓ Bond direction classification: ";
    switch (zone) {
        case AdhesionZone::ZoneA: std::cout << "Zone A (should inherit to child B)\n"; break;
        case AdhesionZone::ZoneB: std::cout << "Zone B (should inherit to child A)\n"; break;
        case AdhesionZone::ZoneC: std::cout << "Zone C (should inherit to both children)\n"; break;
    }
    
    // Create mode settings for testing
    std::vector<GPUModeAdhesionSettings> modeSettings;
    GPUModeAdhesionSettings defaultMode{};
    defaultMode.restLength = 2.0f;
    defaultMode.linearSpringStiffness = 150.0f;
    modeSettings.push_back(defaultMode);
    
    // Perform inheritance
    glm::vec3 splitOffset = glm::vec3(0.5f, 0.0f, 0.0f);
    glm::quat orientationA = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity
    glm::quat orientationB = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity
    
    size_t connectionsBefore = adhesions.activeConnectionCount;
    
    inheritAdhesionsOnDivision(
        parentIndex,    // Parent cell index
        parentIndex,    // Child A index (reuses parent index)
        childIndex,     // Child B index
        splitPlane,     // Division plane normal
        splitOffset,    // Split offset vector
        orientationA,   // Child A genome orientation
        orientationB,   // Child B genome orientation
        true,           // Child A keep adhesion
        true,           // Child B keep adhesion
        cells,          // Cell physics data
        adhesions,      // Adhesion connections data
        modeSettings    // Mode-specific adhesion settings
    );
    
    size_t connectionsAfter = adhesions.activeConnectionCount;
    
    std::cout << "✓ Division inheritance completed\n";
    std::cout << "  Connections before: " << connectionsBefore << "\n";
    std::cout << "  Connections after: " << connectionsAfter << "\n";
    
    // Display metrics
    InheritanceMetrics metrics = getLastInheritanceMetrics();
    std::cout << "  Inheritance metrics:\n";
    std::cout << "    Parent connections processed: " << metrics.parentConnectionsProcessed << "\n";
    std::cout << "    Child A inherited connections: " << metrics.childAInheritedConnections << "\n";
    std::cout << "    Child B inherited connections: " << metrics.childBInheritedConnections << "\n";
    std::cout << "    Child-to-child connections: " << metrics.childToChildConnections << "\n";
    std::cout << "    Zone A connections: " << metrics.zoneAConnections << "\n";
    std::cout << "    Zone B connections: " << metrics.zoneBConnections << "\n";
    std::cout << "    Zone C connections: " << metrics.zoneCConnections << "\n";
    std::cout << "    Processing time: " << metrics.processingTimeMs << " ms\n";
    
    // Validate results
    bool testPassed = true;
    
    if (metrics.parentConnectionsProcessed == 0) {
        std::cout << "❌ No parent connections were processed\n";
        testPassed = false;
    }
    
    if (metrics.childAInheritedConnections == 0 && metrics.childBInheritedConnections == 0) {
        std::cout << "❌ No connections were inherited by children\n";
        testPassed = false;
    }
    
    if (connectionsAfter <= connectionsBefore) {
        std::cout << "❌ Expected more connections after inheritance\n";
        testPassed = false;
    }
    
    if (testPassed) {
        std::cout << "✅ Division inheritance test PASSED\n";
    } else {
        std::cout << "❌ Division inheritance test FAILED\n";
    }
    
    std::cout << "=== Division Inheritance Test Complete ===\n\n";
}