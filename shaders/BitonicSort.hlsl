#define THREAD_GROUP_SIZE 64

#define eLocalBitonicMergeSort  0
#define eLocalDisperse          1
#define eBigFlip                2
#define eBigDisperse            3

struct Constants
{
    float3 cameraPos;
    uint h;
    uint algorithm;
};

ConstantBuffer<Constants> ConstantsCB : register(b0);

struct Splat
{
    float4 position;
    float4 scale;
    float4 color;
    float4 rotation;
};

RWStructuredBuffer<Splat> splats : register(u0);

// Groupshared cache: holds two elements per thread (128 Splats = 8KB).
// Used by local_bms and local_disperse to avoid round-trips to global memory.
groupshared Splat local_splats[THREAD_GROUP_SIZE * 2];

// ---------- comparison ----------

float splatDist(Splat s)
{
    return distance(s.position.xyz, ConstantsCB.cameraPos);
}

// Returns true when i and j should be swapped
// (i.e. i is closer to the camera than j).
// This produces a back-to-front (descending distance) ordering.
bool shouldSwap(Splat a, Splat b)
{
    return splatDist(a) < splatDist(b);
}

// ---------- compare-and-swap on global / local memory ----------

void global_compare_and_swap(uint i, uint j)
{
    if (shouldSwap(splats[i], splats[j]))
    {
        Splat tmp  = splats[i];
        splats[i]  = splats[j];
        splats[j]  = tmp;
    }
}

void local_compare_and_swap(uint i, uint j)
{
    if (shouldSwap(local_splats[i], local_splats[j]))
    {
        Splat tmp         = local_splats[i];
        local_splats[i]   = local_splats[j];
        local_splats[j]   = tmp;
    }
}

// ---------- big (cross-workgroup) kernels — operate on global memory ----------

void big_flip(uint t, uint h)
{
    uint half_h = h >> 1;
    uint q      = ((2 * t) / h) * h;
    global_compare_and_swap(q + (t % half_h),
                            q + h - (t % half_h) - 1);
}

void big_disperse(uint t, uint h)
{
    uint half_h = h >> 1;
    uint q      = ((2 * t) / h) * h;
    global_compare_and_swap(q + (t % half_h),
                            q + (t % half_h) + half_h);
}

// ---------- local (within-workgroup) kernels — operate on groupshared memory ----------

void local_flip(uint t, uint h)
{
    GroupMemoryBarrierWithGroupSync();
    uint half_h = h >> 1;
    uint q      = ((2 * t) / h) * h;
    local_compare_and_swap(q + (t % half_h),
                           q + h - (t % half_h) - 1);
}

// Performs a cascade of disperse steps: h, h/2, h/4 … 2.
// Mirrors the inner loop in the article's local_disperse().
void local_disperse(uint t, uint h)
{
    for (; h > 1; h >>= 1)
    {
        GroupMemoryBarrierWithGroupSync();
        uint half_h = h >> 1;
        uint q      = ((2 * t) / h) * h;
        local_compare_and_swap(q + (t % half_h),
                               q + (t % half_h) + half_h);
    }
}

// Full local bitonic merge sort up to height h.
void local_bms(uint t, uint h)
{
    for (uint hh = 2; hh <= h; hh <<= 1)
    {
        local_flip(t, hh);
        local_disperse(t, hh >> 1);
    }
}

// ---------- entry point ----------

[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 groupID       : SV_GroupID,
          uint3 groupThreadID : SV_GroupThreadID,
          uint3 dispatchID    : SV_DispatchThreadID)
{
    uint t       = groupThreadID.x;   // local thread index  (0 … THREAD_GROUP_SIZE-1)
    uint t_prime = dispatchID.x;      // global thread index (used by big_* kernels)

    // Offset into the global buffer for this workgroup's lane.
    uint offset = THREAD_GROUP_SIZE * 2 * groupID.x;

    // Local kernels need their lane's data pulled into groupshared memory first.
    if (ConstantsCB.algorithm <= eLocalDisperse)
    {
        local_splats[t * 2]     = splats[offset + t * 2];
        local_splats[t * 2 + 1] = splats[offset + t * 2 + 1];
    }

    switch (ConstantsCB.algorithm)
    {
        case eLocalBitonicMergeSort:
            local_bms(t, ConstantsCB.h);
            break;
        case eLocalDisperse:
            local_disperse(t, ConstantsCB.h);
            break;
        case eBigFlip:
            big_flip(t_prime, ConstantsCB.h);
            break;
        case eBigDisperse:
            big_disperse(t_prime, ConstantsCB.h);
            break;
    }

    // Write groupshared results back to global memory.
    if (ConstantsCB.algorithm <= eLocalDisperse)
    {
        GroupMemoryBarrierWithGroupSync();
        splats[offset + t * 2]     = local_splats[t * 2];
        splats[offset + t * 2 + 1] = local_splats[t * 2 + 1];
    }
}
