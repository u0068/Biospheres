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
    
    // Padding to maintain 16-byte alignment for the entire struct
    uint _padding[8];
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



// Particle structure - stored per voxel in the spatial grid
struct Particle {
    vec3 position;      // World position
    float lifetime;     // Remaining lifetime (0 = dead)
    vec3 velocity;      // Velocity for movement
    float maxLifetime;  // Maximum lifetime for fade calculation
    vec4 color;         // RGBA color
};