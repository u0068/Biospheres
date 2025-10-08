// Cell data structure for compute shader
struct ComputeCell {
    // Physics:
    vec4 positionAndMass;   // x, y, z, mass
    vec4 velocity;          // x, y, z, padding
    vec4 acceleration;      // x, y, z, padding
    vec4 prevAcceleration;  // x, y, z, padding
    vec4 orientation;       // Quaternion to (prevent gimbal lock)
    vec4 angularVelocity;   // Pseudo-vector for easy math
    vec4 angularAcceleration;       // Pseudo-vector for easy math
    vec4 prevAngularAcceleration;   // Pseudo-vector for easy math
    // Internal:
    vec4 signallingSubstances;  // 4 substances for now
    int modeIndex;              // absolute index of the cell's mode
    float age;                  // also used for split timer
    float toxins;
    float nitrates;
    int adhesionIndices[20];
    
    // Lineage tracking (AA.BB.C format)
    uint parentLineageId;       // AA: Parent's unique ID (0 for root cells)
    uint uniqueId;              // BB: This cell's unique ID
    uint childNumber;           // C: Child number (1 or 2, 0 for root cells)
    uint _lineagePadding;       // Padding to maintain 16-byte alignment
};

struct GPUModeAdhesionSettings
{
    int canBreak;                 // bool -> int (4 bytes)
    float breakForce;
    float restLength;
    float linearSpringStiffness;
    float linearSpringDamping;
    float orientationSpringStiffness;
    float orientationSpringDamping;
    float maxAngularDeviation;
    float twistConstraintStiffness;
    float twistConstraintDamping;
    int enableTwistConstraint;    // bool -> int (4 bytes)
    int _padding;                 // pad to 48 bytes
};

// GPU Mode structure
struct GPUMode {
    vec4 color;           // 16 bytes
    vec4 orientationA;    // 16 bytes
    vec4 orientationB;    // 16 bytes
    vec4 splitDirection;  // 16 bytes
    ivec2 childModes;     // 8 bytes
    float splitInterval;  // 4 bytes
    int genomeOffset;     // 4 bytes (total 16 bytes)
    GPUModeAdhesionSettings adhesionSettings; // 48 bytes
    int parentMakeAdhesion; // 4 bytes
    int childAKeepAdhesion; // 4 bytes
    int childBKeepAdhesion; // 4 bytes
    int maxAdhesions;       // 4 bytes (total 16 bytes)
    float flagellocyteThrustForce; // 4 bytes
    float _padding1;      // 4 bytes
    float _padding2;      // 4 bytes
    float _padding3;      // 4 bytes (total 16 bytes)
};

// Adhesion connection structure - stores permanent connections between sibling cells
struct AdhesionConnection {
    uint cellAIndex;      // Index of first cell in the connection
    uint cellBIndex;      // Index of second cell in the connection
    uint modeIndex;       // Mode index for the connection ( to lookup adhesion settings )
    uint isActive;        // Whether this connection is still active (1 = active, 0 = inactive)
    uint zoneA;           // Zone classification for cell A (0=ZoneA, 1=ZoneB, 2=ZoneC)
    uint zoneB;           // Zone classification for cell B (0=ZoneA, 1=ZoneB, 2=ZoneC)
    vec3 anchorDirectionA; // Anchor direction for cell A in local cell space (normalized)
    float paddingA;       // Padding to ensure 16-byte alignment
    vec3 anchorDirectionB; // Anchor direction for cell B in local cell space (normalized)
    float paddingB;       // Padding to ensure 16-byte alignment
    vec4 twistReferenceA; // Reference quaternion for twist constraint for cell A (16 bytes)
    vec4 twistReferenceB; // Reference quaternion for twist constraint for cell B (16 bytes)
    uint _padding[2];     // Padding to ensure 16-byte alignment (96 bytes total)
};

// Adhesion diagnostic entry structure - matches C++ AdhesionDiagnosticEntry
struct AdhesionDiagnosticEntry {
    uint connectionIndex;
    uint cellA;
    uint cellB;
    uint modeIndex;
    uint reasonCode; // See reason code definitions in C++ code
    vec3 anchorDirA;
    float _pad0; // padding to 16 bytes alignment
    vec3 anchorDirB;
    float _pad1; // padding to 16 bytes alignment
    uint frameIndex;
    uint splitEventID;
    uint parentCellID;
    uint inheritanceType; // 0=none, 1=childA_keeps, 2=childB_keeps, 3=both_keep, 4=neither_keep
    uint originalConnectionIndex; // Index of the original connection being inherited from
    float adhesionZone; // Dot product with split direction (inheritance decision factor)
    float _pad2; // padding to 16-byte alignment
    float _pad3; // padding to 16-byte alignment
    float _pad4; // padding to 16-byte alignment
    float _pad5; // additional padding for 16-byte alignment
    float _pad6; // additional padding for 16-byte alignment
};