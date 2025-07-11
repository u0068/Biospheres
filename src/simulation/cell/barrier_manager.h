#pragma once
#include <glad/glad.h>

struct CellBarrierManager
{
    struct BarrierStats {
        int totalBarriers = 0;
        int batchedBarriers = 0;
        int flushCalls = 0;
        float barrierEfficiency = 0.0f; // batchedBarriers / totalBarriers
        
        void reset() {
            totalBarriers = 0;
            batchedBarriers = 0;
            flushCalls = 0;
            barrierEfficiency = 0.0f;
        }
        
        void updateEfficiency() {
            if (totalBarriers > 0) {
                barrierEfficiency = static_cast<float>(batchedBarriers) / static_cast<float>(totalBarriers);
            }
        }
    };

    struct BarrierBatch {
        GLbitfield pendingBarriers = 0;
        bool needsFlush = false;
        mutable BarrierStats* stats = nullptr; // Reference to stats for tracking
        
        void addBarrier(GLbitfield barrier) {
            pendingBarriers |= barrier;
            if (stats) {
                stats->totalBarriers++;
                if (pendingBarriers != barrier) {
                    stats->batchedBarriers++; // This barrier was batched with others
                }
            }
        }
        
        void flush() {
            if (pendingBarriers != 0) {
                glMemoryBarrier(pendingBarriers);
                pendingBarriers = 0;
                needsFlush = false;
                if (stats) {
                    stats->flushCalls++;
                }
            }
        }
        
        void clear() {
            pendingBarriers = 0;
            needsFlush = false;
        }
        
        void setStats(BarrierStats* statsPtr) {
            stats = statsPtr;
        }
    };

    mutable BarrierStats barrierStats;
    mutable BarrierBatch barrierBatch;

    // Constructor and destructor
    CellBarrierManager();
    ~CellBarrierManager();

    // Barrier management
    void addBarrier(GLbitfield barrier) const { barrierBatch.addBarrier(barrier); }
    void flushBarriers() const { barrierBatch.flush(); }
    void clearBarriers() const { barrierBatch.clear(); }

    // Statistics
    const BarrierStats& getBarrierStats() const { return barrierStats; }
    void resetBarrierStats() const { barrierStats.reset(); }
}; 