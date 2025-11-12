#pragma once

#include <vector>
#include <array>
#include <unordered_set>
#include <functional>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "cpu_soa_data_manager.h"
#include "../cell/common_structs.h"
#include "../../core/config.h"

/**
 * CPU Adhesion Connection Manager
 * 
 * Implements complete connection management and validation system for CPU adhesion physics.
 * Handles proper adhesion index slot management (20 slots per cell, -1 for empty).
 * Provides connection creation, removal, and cleanup with proper index management.
 * Enforces connection capacity limits (5,120 maximum) and data integrity validation.
 * 
 * Requirements addressed: 10.1, 10.2, 10.3, 10.4, 10.5, 7.4, 7.5
 */
class CPUAdhesionConnectionManager {
public:
    CPUAdhesionConnectionManager();
    ~CPUAdhesionConnectionManager();

    // Connection creation with proper slot management (Requirement 10.2)
    int addAdhesionWithDirections(
        uint32_t cellA, uint32_t cellB, uint32_t modeIndex,
        const glm::vec3& anchorDirectionA, const glm::vec3& anchorDirectionB,
        float restLength = 1.0f
    );

    // Connection removal and cleanup (Requirement 10.3)
    bool removeAdhesion(int connectionIndex);
    void removeAllConnectionsForCell(uint32_t cellIndex);
    void cleanupInactiveConnections();

    // Adhesion index slot management (Requirement 10.1)
    void initializeCellAdhesionIndices(uint32_t cellIndex);
    int findFreeAdhesionSlot(uint32_t cellIndex) const;
    bool setAdhesionIndex(uint32_t cellIndex, int slotIndex, int connectionIndex);
    bool removeAdhesionIndex(uint32_t cellIndex, int connectionIndex);

    // Data integrity validation (Requirement 10.4)
    struct ValidationResult {
        bool isValid = true;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        size_t totalConnections = 0;
        size_t activeConnections = 0;
        size_t invalidConnections = 0;
        size_t orphanedConnections = 0;
        size_t duplicateConnections = 0;
    };

    ValidationResult validateConnectionIntegrity() const;
    ValidationResult validateCellAdhesionIndices() const;
    ValidationResult validateConnectionCapacity() const;
    bool validateSingleConnection(int connectionIndex) const;

    // Connection capacity management (Requirement 10.5)
    bool isConnectionCapacityAvailable() const;
    size_t getActiveConnectionCount() const;
    size_t getMaxConnectionCapacity() const { return MAX_CONNECTIONS; }
    float getConnectionCapacityUtilization() const;

    // Connection queries and information
    std::vector<int> getConnectionsForCell(uint32_t cellIndex) const;
    std::vector<uint32_t> getConnectedCells(uint32_t cellIndex) const;
    bool areCellsConnected(uint32_t cellA, uint32_t cellB) const;
    int findConnectionBetweenCells(uint32_t cellA, uint32_t cellB) const;

    // Data access
    void setCellData(CPUCellPhysics_SoA* cellData) { m_cellData = cellData; }
    void setAdhesionData(CPUAdhesionConnections_SoA* adhesionData) { m_adhesionData = adhesionData; }
    
    // System information and statistics
    struct ConnectionStatistics {
        size_t totalSlots = 0;
        size_t usedSlots = 0;
        size_t freeSlots = 0;
        float averageConnectionsPerCell = 0.0f;
        uint32_t maxConnectionsOnSingleCell = 0;
        uint32_t cellsWithMaxConnections = 0;
        size_t connectionArrayUtilization = 0;
    };

    ConnectionStatistics getConnectionStatistics() const;
    void printConnectionStatistics() const;
    void printValidationReport(const ValidationResult& result) const;

    // Testing and debugging
    void runComprehensiveTests();
    bool testConnectionCreation();
    bool testConnectionRemoval();
    bool testSlotManagement();
    bool testCapacityLimits();
    bool testDataIntegrity();

private:
    // Data references (set externally)
    CPUCellPhysics_SoA* m_cellData = nullptr;
    CPUAdhesionConnections_SoA* m_adhesionData = nullptr;

    // Connection management constants
    static constexpr int MAX_ADHESIONS_PER_CELL = 20;
    static constexpr size_t MAX_CONNECTIONS = 5120; // 20 × 256 cells

    // Internal helper methods
    int findFreeConnectionSlot() const;
    bool isValidCellIndex(uint32_t cellIndex) const;
    bool isValidConnectionIndex(int connectionIndex) const;
    bool isConnectionActive(int connectionIndex) const;
    
    // Connection slot management
    void markConnectionActive(int connectionIndex);
    void markConnectionInactive(int connectionIndex);
    void updateActiveConnectionCount();

    // Validation helpers
    bool validateConnectionIndices(int connectionIndex) const;
    bool validateAnchorDirections(int connectionIndex) const;
    bool validateTwistReferences(int connectionIndex) const;
    std::vector<int> findOrphanedConnections() const;
    std::vector<int> findDuplicateConnections() const;
    bool checkCircularReferences() const;

    // Error reporting
    void reportError(ValidationResult& result, const std::string& error) const;
    void reportWarning(ValidationResult& result, const std::string& warning) const;
};