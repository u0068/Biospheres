#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <cstdint>
#include "cpu_soa_data_manager.h"

/**
 * CPU Division Inheritance Handler
 * 
 * Implements complete division inheritance system with geometric anchor placement
 * to achieve behavioral equivalence with the GPU implementation.
 * 
 * Features:
 * - Zone classification system using classifyBondDirection with 2-degree threshold
 * - Inheritance rules: Zone A to child B, Zone B to child A, Zone C to both children
 * - Geometric anchor direction calculation using parent frame positions and orientations
 * - Child-to-child connection creation with proper anchor directions from split direction
 * - Proper adhesion index management during cell division
 * - Connection role preservation (cellA/cellB) during inheritance
 * - Neighbor anchor calculation using relative rotation between cells
 * - Zone classification for child-to-child connections
 * 
 * Fixed Issues:
 * - Child-to-child adhesion now uses split direction from mode (not arbitrary defaults)
 * - Proper zone classification for child-to-child anchors
 * - Correct neighbor anchor direction calculation using relative rotation
 * - Preserves cellA/cellB roles during inheritance
 * - Uses parent's mode index for child-to-child connection
 * 
 * Requirements addressed: 8.1, 8.2, 8.3, 8.4, 8.5, 9.1, 9.2, 9.3, 9.4, 9.5, 10.1, 10.2, 10.3, 10.4, 10.5
 */
class CPUDivisionInheritanceHandler {
public:
    CPUDivisionInheritanceHandler();
    ~CPUDivisionInheritanceHandler();

    /**
     * Main division inheritance function
     * Handles complete adhesion inheritance when a cell divides
     * 
     * @param parentCellIndex Index of the parent cell that is dividing
     * @param childACellIndex Index of child A (gets +splitOffset)
     * @param childBCellIndex Index of child B (gets -splitOffset)
     * @param splitPlane Normal vector of the division plane
     * @param splitOffset Offset vector for child positioning
     * @param orientationA Genome orientation for child A
     * @param orientationB Genome orientation for child B
     * @param childAKeepAdhesion Whether child A should inherit adhesions
     * @param childBKeepAdhesion Whether child B should inherit adhesions
     * @param cells Cell physics data (SoA format)
     * @param adhesions Adhesion connections data (SoA format)
     * @param modeSettings Mode-specific adhesion settings
     */
    void inheritAdhesionsOnDivision(
        uint32_t parentCellIndex,
        uint32_t childACellIndex, uint32_t childBCellIndex,
        const glm::vec3& splitPlane, const glm::vec3& splitOffset,
        const glm::quat& orientationA, const glm::quat& orientationB,
        bool childAKeepAdhesion, bool childBKeepAdhesion,
        CPUCellPhysics_SoA& cells,
        CPUAdhesionConnections_SoA& adhesions,
        const std::vector<GPUModeAdhesionSettings>& modeSettings
    );

    /**
     * Add adhesion connection with proper index management
     * Finds available slots in both cells and creates the connection
     * 
     * @param cellA Index of first cell
     * @param cellB Index of second cell
     * @param anchorDirectionA Anchor direction for cell A (local space)
     * @param anchorDirectionB Anchor direction for cell B (local space)
     * @param modeIndex Mode index for adhesion settings
     * @param cells Cell physics data for adhesion index management
     * @param adhesions Adhesion connections data
     * @return Connection index if successful, -1 if failed
     */
    int addAdhesionWithDirections(
        uint32_t cellA, uint32_t cellB,
        const glm::vec3& anchorDirectionA, const glm::vec3& anchorDirectionB,
        uint32_t modeIndex,
        CPUCellPhysics_SoA& cells,
        CPUAdhesionConnections_SoA& adhesions
    );

    // Performance and validation metrics
    struct InheritanceMetrics {
        size_t parentConnectionsProcessed = 0;
        size_t childAInheritedConnections = 0;
        size_t childBInheritedConnections = 0;
        size_t childToChildConnections = 0;
        size_t zoneAConnections = 0;
        size_t zoneBConnections = 0;
        size_t zoneCConnections = 0;
        float processingTimeMs = 0.0f;
    };

    InheritanceMetrics getLastInheritanceMetrics() const { return m_lastMetrics; }

    /**
     * Test the division inheritance system with a simple scenario
     * Creates test cells and connections, then performs division to validate inheritance
     */
    void testDivisionInheritance(
        CPUCellPhysics_SoA& cells,
        CPUAdhesionConnections_SoA& adhesions
    );

private:
    /**
     * Zone classification for adhesion inheritance
     * Uses 2-degree threshold around equatorial plane
     */
    enum class AdhesionZone {
        ZoneA = 0,  // dot product < 0, negative side of split plane -> inherit to child B
        ZoneB = 1,  // dot product > 0 and |angle - 90°| > 2°, positive side -> inherit to child A
        ZoneC = 2   // |angle - 90°| ≤ 2°, equatorial band -> inherit to both children
    };

    /**
     * Classify bond direction into zones using 2-degree threshold
     * 
     * @param bondDirection Direction vector from parent to neighbor
     * @param splitPlane Normal vector of the division plane
     * @return Zone classification for inheritance rules
     */
    AdhesionZone classifyBondDirection(
        const glm::vec3& bondDirection, 
        const glm::vec3& splitPlane
    );

    /**
     * Calculate child anchor direction using geometric relationships
     * 
     * @param parentAnchor Original anchor direction in parent local space
     * @param neighborPos Position of the neighbor cell
     * @param childPos Position of the child cell
     * @param restLength Rest length for the connection
     * @param parentRadius Radius of the parent cell
     * @param neighborRadius Radius of the neighbor cell
     * @param genomeOrientation Genome orientation for coordinate transformation
     * @return Anchor direction in child local space
     */
    glm::vec3 calculateChildAnchorDirection(
        const glm::vec3& parentAnchor,
        const glm::vec3& neighborPos,
        const glm::vec3& childPos,
        float restLength,
        float parentRadius,
        float neighborRadius,
        const glm::quat& genomeOrientation
    );

    /**
     * Find free adhesion slot in cell's adhesionIndices array
     * 
     * @param cellIndex Index of the cell
     * @param cells Cell physics data containing adhesionIndices
     * @return Slot index (0-19) if available, -1 if no free slots
     */
    int findFreeAdhesionSlot(uint32_t cellIndex, const CPUCellPhysics_SoA& cells);

    /**
     * Set adhesion index in cell's adhesionIndices array
     * 
     * @param cellIndex Index of the cell
     * @param slotIndex Slot index (0-19)
     * @param connectionIndex Connection index to store
     * @param cells Cell physics data to modify
     */
    void setAdhesionIndex(uint32_t cellIndex, int slotIndex, int connectionIndex, CPUCellPhysics_SoA& cells);

    /**
     * Remove adhesion index from cell's adhesionIndices array
     * 
     * @param cellIndex Index of the cell
     * @param connectionIndex Connection index to remove
     * @param cells Cell physics data to modify
     */
    void removeAdhesionIndex(uint32_t cellIndex, int connectionIndex, CPUCellPhysics_SoA& cells);

    /**
     * Initialize all adhesion indices to -1 for a cell
     * 
     * @param cellIndex Index of the cell
     * @param cells Cell physics data to modify
     */
    void initializeAdhesionIndices(uint32_t cellIndex, CPUCellPhysics_SoA& cells);

    /**
     * Find next available connection slot in adhesion array
     * 
     * @param adhesions Adhesion connections data
     * @return Connection index if available, -1 if array is full
     */
    int findFreeConnectionSlot(const CPUAdhesionConnections_SoA& adhesions);

    // Performance tracking
    InheritanceMetrics m_lastMetrics;

    // Constants matching GPU implementation
    static constexpr float EQUATORIAL_THRESHOLD_DEGREES = 2.0f;
    static constexpr float EQUATORIAL_THRESHOLD_RADIANS = EQUATORIAL_THRESHOLD_DEGREES * (3.14159f / 180.0f);
    static constexpr int MAX_ADHESIONS_PER_CELL = 20;
    static constexpr int MAX_CONNECTIONS = 5120;
};