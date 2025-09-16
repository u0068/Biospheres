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
    vec4 signallingSubstances; // 4 substances for now
    int modeIndex;  // absolute index of the cell's mode
    float age; // also used for split timer
    float toxins;
    float nitrates;
    int adhesionIndices[20];
};

struct AdhesionSettings
{
    bool canBreak;
    float breakForce;
    float restLength;
    float linearSpringStiffness;
    float linearSpringDamping;
    float orientationSpringStiffness;
    float orientationSpringDamping;
    float maxAngularDeviation;      // Does nothing yet
};

// GPU Mode structure
struct GPUMode {
    vec4 color;           // R, G, B, padding
    vec4 orientationA;    // Quaternion
    vec4 orientationB;    // Quaternion
    vec4 splitDirection;  // x, y, z, padding
    ivec2 childModes;     // mode indices for children
    float splitInterval;
    int genomeOffset;
    AdhesionSettings adhesionSettings;
    int parentMakeAdhesion; // Boolean flag for adhesion creation
    int childAKeepAdhesion; // Boolean flag for child A to keep adhesion
    int childBKeepAdhesion; // Boolean flag for child B to keep adhesion
    int maxAdhesions;       // Max number of adhesions
};

// Adhesion connection structure - stores permanent connections between sibling cells
struct AdhesionConnection {
    uint cellAIndex;      // Index of first cell in the connection
    uint cellBIndex;      // Index of second cell in the connection
    uint modeIndex;       // Mode index for the connection ( to lookup adhesion settings )
    uint isActive;        // Whether this connection is still active (1 = active, 0 = inactive)
    vec3 anchorDirectionA; // Anchor direction for cell A in local cell space (normalized)
    float paddingA;       // Padding to ensure 16-byte alignment
    vec3 anchorDirectionB; // Anchor direction for cell B in local cell space (normalized)
    float paddingB;       // Padding to ensure 16-byte alignment
};