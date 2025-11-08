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
    const GPUMode& parentMode,
    const std::vector<GPUMode>& allModes) {
    
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

    // Get child mode indices from parent mode (used for all inherited connections)
    uint32_t childAModeIndex = parentMode.childModes.x;
    uint32_t childBModeIndex = parentMode.childModes.y;

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
        
        // CRITICAL FIX: Use stored anchor direction in LOCAL SPACE (matches GPU implementation)
        // GPU uses the anchor direction stored in the adhesion connection, NOT world-space positions
        // Get the parent's anchor direction from the connection (in parent's local frame)
        glm::vec3 localAnchorDirection;
        if (parentIsA) {
            localAnchorDirection = glm::vec3(
                adhesions.anchorDirectionA_x[connectionIndex],
                adhesions.anchorDirectionA_y[connectionIndex],
                adhesions.anchorDirectionA_z[connectionIndex]
            );
        } else {
            localAnchorDirection = glm::vec3(
                adhesions.anchorDirectionB_x[connectionIndex],
                adhesions.anchorDirectionB_y[connectionIndex],
                adhesions.anchorDirectionB_z[connectionIndex]
            );
        }

        // CRITICAL FIX: Classify using LOCAL anchor direction and splitDirection from genome
        // This matches GPU implementation exactly - zones are classified in parent's local frame
        glm::vec3 splitDirection = glm::vec3(parentMode.splitDirection);
        AdhesionZone zone = classifyBondDirection(localAnchorDirection, splitDirection);

        // Extract split direction and offset for geometric calculations (matching GPU)
        float splitMagnitude = glm::length(splitDirection);
        glm::vec3 splitDir_parent = (splitMagnitude < 0.0001f) ? glm::vec3(0.0f, 0.0f, 1.0f) : (splitDirection / splitMagnitude);
        float splitOffsetMagnitude = (splitMagnitude < 0.0001f) ? 0.0f : (splitMagnitude * 0.5f);
        
        // Update zone statistics
        switch (zone) {
            case AdhesionZone::ZoneA: m_lastMetrics.zoneAConnections++; break;
            case AdhesionZone::ZoneB: m_lastMetrics.zoneBConnections++; break;
            case AdhesionZone::ZoneC: m_lastMetrics.zoneCConnections++; break;
        }
        
        // Get original connection properties
        uint32_t connectionModeIndex = adhesions.modeIndex[connectionIndex];

        // For the neighbor, we don't have direct mode info in the SoA, so we use index 0 as default
        // In a full implementation, this should be tracked separately
        uint32_t neighborModeIndex = 0;
        
        // Get rest length from mode settings
        float restLength = 1.0f; // Default
        if (connectionModeIndex < allModes.size()) {
            restLength = allModes[connectionModeIndex].adhesionSettings.restLength;
        }
        
        // Get parent and neighbor GENOME orientations for proper anchor calculation (GPU uses genomeOrientation)
        // In CPU SoA, genome orientation is stored as genomeQuat_x, genomeQuat_y, genomeQuat_z, genomeQuat_w
        glm::quat parentOrientation(
            cells.genomeQuat_w[parentCellIndex],
            cells.genomeQuat_x[parentCellIndex],
            cells.genomeQuat_y[parentCellIndex],
            cells.genomeQuat_z[parentCellIndex]
        );
        glm::quat neighborOrientation(
            cells.genomeQuat_w[neighborIndex],
            cells.genomeQuat_x[neighborIndex],
            cells.genomeQuat_y[neighborIndex],
            cells.genomeQuat_z[neighborIndex]
        );
        
        // localAnchorDirection already extracted above (in parent's local frame)
        
        // Calculate center-to-center distance using parent's adhesion rest length
        float centerToCenterDist = restLength + parentRadius + neighborRadius;
        
        // Get parent genome orientation for relative rotation calculations
        glm::quat parentGenomeOrientation(
            cells.genomeQuat_w[parentCellIndex],
            cells.genomeQuat_x[parentCellIndex],
            cells.genomeQuat_y[parentCellIndex],
            cells.genomeQuat_z[parentCellIndex]
        );
        
        // Apply inheritance rules based on zone classification (Requirements 8.2, 8.3, 8.4)
        if (zone == AdhesionZone::ZoneA && childBKeepAdhesion) {
            // Zone A to child B (Requirement 8.2)

            // Calculate positions in parent frame for geometric anchor placement (MATCHES GPU)
            glm::vec3 childBPos_parentFrame = -splitDir_parent * splitOffsetMagnitude;
            glm::vec3 neighborPos_parentFrame = localAnchorDirection * centerToCenterDist;
            
            // Child anchor: direction from child to neighbor, transformed by genome orientation
            glm::vec3 directionToNeighbor_parentFrame = glm::normalize(neighborPos_parentFrame - childBPos_parentFrame);
            glm::vec3 childAnchorDirection = glm::normalize(glm::inverse(orientationB) * directionToNeighbor_parentFrame);
            
            // Neighbor anchor: direction from neighbor to child, transformed to neighbor's frame
            glm::vec3 directionToChild_parentFrame = glm::normalize(childBPos_parentFrame - neighborPos_parentFrame);
            glm::quat relativeRotation = glm::inverse(neighborOrientation) * parentGenomeOrientation;
            glm::vec3 neighborAnchorDirection = glm::normalize(relativeRotation * directionToChild_parentFrame);
            
            // Preserve original side assignment: if neighbor was originally cellA, keep them as cellA
            int newConnectionIndex;
            if (parentIsA) {
                // Parent was cellA, neighbor was cellB, so neighbor becomes cellB
                newConnectionIndex = addAdhesionWithDirections(
                    childBCellIndex, neighborIndex,
                    childAnchorDirection, neighborAnchorDirection,
                    connectionModeIndex, cells, adhesions, allModes,
                    childBModeIndex, neighborModeIndex
                );
            } else {
                // Parent was cellB, neighbor was cellA, so neighbor becomes cellA
                newConnectionIndex = addAdhesionWithDirections(
                    neighborIndex, childBCellIndex,
                    neighborAnchorDirection, childAnchorDirection,
                    connectionModeIndex, cells, adhesions, allModes,
                    neighborModeIndex, childBModeIndex
                );
            }
            
            if (newConnectionIndex >= 0) {
                m_lastMetrics.childBInheritedConnections++;
            }
        }
        else if (zone == AdhesionZone::ZoneB && childAKeepAdhesion) {
            // Zone B to child A (Requirement 8.3)

            // Calculate positions in parent frame for geometric anchor placement (MATCHES GPU)
            glm::vec3 childAPos_parentFrame = splitDir_parent * splitOffsetMagnitude;
            glm::vec3 neighborPos_parentFrame = localAnchorDirection * centerToCenterDist;
            
            // Child anchor: direction from child to neighbor, transformed by genome orientation
            glm::vec3 directionToNeighbor_parentFrame = glm::normalize(neighborPos_parentFrame - childAPos_parentFrame);
            glm::vec3 childAnchorDirection = glm::normalize(glm::inverse(orientationA) * directionToNeighbor_parentFrame);
            
            // Neighbor anchor: direction from neighbor to child, transformed to neighbor's frame
            glm::vec3 directionToChild_parentFrame = glm::normalize(childAPos_parentFrame - neighborPos_parentFrame);
            glm::quat relativeRotation = glm::inverse(neighborOrientation) * parentGenomeOrientation;
            glm::vec3 neighborAnchorDirection = glm::normalize(relativeRotation * directionToChild_parentFrame);
            
            // Preserve original side assignment: if neighbor was originally cellA, keep them as cellA
            int newConnectionIndex;
            if (parentIsA) {
                // Parent was cellA, neighbor was cellB, so neighbor becomes cellB
                newConnectionIndex = addAdhesionWithDirections(
                    childACellIndex, neighborIndex,
                    childAnchorDirection, neighborAnchorDirection,
                    connectionModeIndex, cells, adhesions, allModes,
                    childAModeIndex, neighborModeIndex
                );
            } else {
                // Parent was cellB, neighbor was cellA, so neighbor becomes cellA
                newConnectionIndex = addAdhesionWithDirections(
                    neighborIndex, childACellIndex,
                    neighborAnchorDirection, childAnchorDirection,
                    connectionModeIndex, cells, adhesions, allModes,
                    neighborModeIndex, childAModeIndex
                );
            }
            
            if (newConnectionIndex >= 0) {
                m_lastMetrics.childAInheritedConnections++;
            }
        }
        else if (zone == AdhesionZone::ZoneC) {
            // Zone C to both children (Requirement 8.4)
            if (childAKeepAdhesion) {
                // Calculate positions in parent frame for geometric anchor placement (MATCHES GPU)
                glm::vec3 childAPos_parentFrame = splitDir_parent * splitOffsetMagnitude;
                glm::vec3 neighborPos_parentFrame = localAnchorDirection * centerToCenterDist;
                
                // Child anchor: direction from child to neighbor, transformed by genome orientation
                glm::vec3 directionToNeighbor_parentFrame = glm::normalize(neighborPos_parentFrame - childAPos_parentFrame);
                glm::vec3 childAnchorDirection = glm::normalize(glm::inverse(orientationA) * directionToNeighbor_parentFrame);
                
                // Neighbor anchor: direction from neighbor to child, transformed to neighbor's frame
                glm::vec3 directionToChild_parentFrame = glm::normalize(childAPos_parentFrame - neighborPos_parentFrame);
                glm::quat relativeRotation = glm::inverse(neighborOrientation) * parentGenomeOrientation;
                glm::vec3 neighborAnchorDirection = glm::normalize(relativeRotation * directionToChild_parentFrame);
                
                // Preserve original side assignment
                int newConnectionIndex;
                if (parentIsA) {
                    newConnectionIndex = addAdhesionWithDirections(
                        childACellIndex, neighborIndex,
                        childAnchorDirection, neighborAnchorDirection,
                        connectionModeIndex, cells, adhesions, allModes,
                        childAModeIndex, neighborModeIndex
                    );
                } else {
                    newConnectionIndex = addAdhesionWithDirections(
                        neighborIndex, childACellIndex,
                        neighborAnchorDirection, childAnchorDirection,
                        connectionModeIndex, cells, adhesions, allModes,
                        neighborModeIndex, childAModeIndex
                    );
                }
                
                if (newConnectionIndex >= 0) {
                    m_lastMetrics.childAInheritedConnections++;
                }
            }
            
            if (childBKeepAdhesion) {
                // Calculate positions in parent frame for geometric anchor placement (MATCHES GPU)
                glm::vec3 childBPos_parentFrame = -splitDir_parent * splitOffsetMagnitude;
                glm::vec3 neighborPos_parentFrame = localAnchorDirection * centerToCenterDist;
                
                // Child anchor: direction from child to neighbor, transformed by genome orientation
                glm::vec3 directionToNeighbor_parentFrame = glm::normalize(neighborPos_parentFrame - childBPos_parentFrame);
                glm::vec3 childAnchorDirection = glm::normalize(glm::inverse(orientationB) * directionToNeighbor_parentFrame);
                
                // Neighbor anchor: direction from neighbor to child, transformed to neighbor's frame
                glm::vec3 directionToChild_parentFrame = glm::normalize(childBPos_parentFrame - neighborPos_parentFrame);
                glm::quat relativeRotation = glm::inverse(neighborOrientation) * parentGenomeOrientation;
                glm::vec3 neighborAnchorDirection = glm::normalize(relativeRotation * directionToChild_parentFrame);
                
                // Preserve original side assignment
                int newConnectionIndex;
                if (parentIsA) {
                    newConnectionIndex = addAdhesionWithDirections(
                        childBCellIndex, neighborIndex,
                        childAnchorDirection, neighborAnchorDirection,
                        connectionModeIndex, cells, adhesions, allModes,
                        childBModeIndex, neighborModeIndex
                    );
                } else {
                    newConnectionIndex = addAdhesionWithDirections(
                        neighborIndex, childBCellIndex,
                        neighborAnchorDirection, childAnchorDirection,
                        connectionModeIndex, cells, adhesions, allModes,
                        neighborModeIndex, childBModeIndex
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
    // Check if parent mode allows child-to-child adhesion creation
    bool parentMakeAdhesion = (parentMode.parentMakeAdhesion != 0);

    if (childAKeepAdhesion && childBKeepAdhesion && parentMakeAdhesion) {
        // Calculate child-to-child anchor directions using split direction from mode
        // This matches GPU implementation in cell_update_internal.comp

        // CRITICAL FIX: Use mode.splitDirection from genome, NOT splitPlane parameter
        glm::vec3 splitDirLocal = glm::vec3(parentMode.splitDirection);

        // Direction vectors in parent's local frame
        // Child A is at +offset, child B is at -offset
        glm::vec3 directionAtoB_parentLocal = -splitDirLocal;  // A points toward B (at -offset)
        glm::vec3 directionBtoA_parentLocal = splitDirLocal;   // B points toward A (at +offset)

        // Transform to each child's local space using genome-derived orientation deltas
        glm::quat invDeltaA = glm::inverse(orientationA);
        glm::vec3 anchorDirectionA = glm::normalize(invDeltaA * directionAtoB_parentLocal);

        glm::quat invDeltaB = glm::inverse(orientationB);
        glm::vec3 anchorDirectionB = glm::normalize(invDeltaB * directionBtoA_parentLocal);

        // NOTE: childAModeIndex and childBModeIndex already defined earlier in the function

        // Use parent's mode index for the child-to-child adhesion connection settings
        // (This determines which adhesion settings are used for the connection)
        uint32_t parentModeIndexForConnection = 0; // Default - in full implementation should track this

        // Create child-to-child connection with parent's mode index
        // Zones are automatically classified inside addAdhesionWithDirections
        int childConnectionIndex = addAdhesionWithDirections(
            childACellIndex, childBCellIndex,
            anchorDirectionA, anchorDirectionB,
            parentModeIndexForConnection, cells, adhesions, allModes,
            childAModeIndex, childBModeIndex
        );

        if (childConnectionIndex >= 0) {
            m_lastMetrics.childToChildConnections++;
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    m_lastMetrics.processingTimeMs = duration.count() / 1000.0f;
}

CPUDivisionInheritanceHandler::AdhesionZone CPUDivisionInheritanceHandler::classifyBondDirection(
    const glm::vec3& bondDirection,
    const glm::vec3& splitDirection) {

    // CRITICAL FIX: Match GPU implementation exactly
    // GPU classifies based on angle from splitDirection (NOT perpendicular plane!)
    // Zone A: dot < 0 (pointing opposite to split direction)
    // Zone B: dot > 0 and not equatorial (pointing same as split direction)
    // Zone C: angle ≈ 90° from split direction (equatorial band ±2°)

    float dotProduct = glm::dot(bondDirection, splitDirection);
    float angle = std::acos(glm::clamp(dotProduct, -1.0f, 1.0f)) * (180.0f / 3.14159f); // Convert to degrees
    float halfWidth = EQUATORIAL_THRESHOLD_DEGREES; // 2 degrees
    float equatorialAngle = 90.0f;

    // Check if within equatorial threshold (90° ± 2°)
    if (std::abs(angle - equatorialAngle) <= halfWidth) {
        return AdhesionZone::ZoneC; // Equatorial band
    }
    // Classify based on which side relative to split direction
    else if (dotProduct > 0.0f) {
        return AdhesionZone::ZoneB; // Positive dot product (same direction as split)
    } else {
        return AdhesionZone::ZoneA; // Negative dot product (opposite to split)
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
    CPUAdhesionConnections_SoA& adhesions,
    const std::vector<GPUMode>& allModes,
    uint32_t cellAModeIndex,
    uint32_t cellBModeIndex) {

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

    // CRITICAL FIX: Classify zones using each cell's mode splitDirection (matches GPU)
    // Get modes for zone classification
    const GPUMode& modeA = (cellAModeIndex < allModes.size()) ? allModes[cellAModeIndex] : allModes[0];
    const GPUMode& modeB = (cellBModeIndex < allModes.size()) ? allModes[cellBModeIndex] : allModes[0];

    // Classify zones using anchor directions and each cell's own split direction
    AdhesionZone zoneA = classifyBondDirection(anchorDirectionA, glm::vec3(modeA.splitDirection));
    AdhesionZone zoneB = classifyBondDirection(anchorDirectionB, glm::vec3(modeB.splitDirection));

    adhesions.zoneA[connectionIndex] = static_cast<uint32_t>(zoneA);
    adhesions.zoneB[connectionIndex] = static_cast<uint32_t>(zoneB);
    
    // Set anchor directions
    adhesions.anchorDirectionA_x[connectionIndex] = anchorDirectionA.x;
    adhesions.anchorDirectionA_y[connectionIndex] = anchorDirectionA.y;
    adhesions.anchorDirectionA_z[connectionIndex] = anchorDirectionA.z;
    adhesions.anchorDirectionB_x[connectionIndex] = anchorDirectionB.x;
    adhesions.anchorDirectionB_y[connectionIndex] = anchorDirectionB.y;
    adhesions.anchorDirectionB_z[connectionIndex] = anchorDirectionB.z;

    // CRITICAL FIX: Set twist references from genome orientations (NOT identity quaternions)
    // This matches GPU implementation: twistReferenceA = cellA.genomeOrientation
    // Get genome orientations from cells
    glm::quat genomeOrientationA(
        cells.genomeQuat_w[cellA],
        cells.genomeQuat_x[cellA],
        cells.genomeQuat_y[cellA],
        cells.genomeQuat_z[cellA]
    );
    glm::quat genomeOrientationB(
        cells.genomeQuat_w[cellB],
        cells.genomeQuat_x[cellB],
        cells.genomeQuat_y[cellB],
        cells.genomeQuat_z[cellB]
    );

    // Set twist references from genome orientations
    adhesions.twistReferenceA_x[connectionIndex] = genomeOrientationA.x;
    adhesions.twistReferenceA_y[connectionIndex] = genomeOrientationA.y;
    adhesions.twistReferenceA_z[connectionIndex] = genomeOrientationA.z;
    adhesions.twistReferenceA_w[connectionIndex] = genomeOrientationA.w;
    adhesions.twistReferenceB_x[connectionIndex] = genomeOrientationB.x;
    adhesions.twistReferenceB_y[connectionIndex] = genomeOrientationB.y;
    adhesions.twistReferenceB_z[connectionIndex] = genomeOrientationB.z;
    adhesions.twistReferenceB_w[connectionIndex] = genomeOrientationB.w;
    
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
    
    // Create mode data for testing (needed early for addAdhesionWithDirections)
    std::vector<GPUMode> allModes;
    GPUMode defaultMode{};
    defaultMode.splitDirection = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f); // Split along Y axis
    defaultMode.adhesionSettings.restLength = 2.0f;
    defaultMode.adhesionSettings.linearSpringStiffness = 150.0f;
    defaultMode.parentMakeAdhesion = 1; // Enable child-to-child connections
    defaultMode.childAKeepAdhesion = 1;
    defaultMode.childBKeepAdhesion = 1;
    defaultMode.childModes = glm::ivec2(0, 0); // Both children use same mode
    allModes.push_back(defaultMode);

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
        cells, adhesions, allModes,
        0, 0 // Both cells use mode 0 for testing
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
        defaultMode,    // Parent mode data
        allModes        // All mode data
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