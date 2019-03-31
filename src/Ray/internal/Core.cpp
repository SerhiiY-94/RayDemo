#include "Core.h"

#include <cassert>
#include <cmath>
#include <cstring>

#include <deque>
#include <vector>

#include "BVHSplit.h"

namespace Ray {
const float axis_aligned_normal_eps = 0.000001f;

force_inline Ref::simd_fvec3 cross(const Ref::simd_fvec3 &v1, const Ref::simd_fvec3 &v2) {
    return { v1[1] * v2[2] - v1[2] * v2[1],
             v1[2] * v2[0] - v1[0] * v2[2],
             v1[0] * v2[1] - v1[1] * v2[0] };
}

// "Insert" two 0 bits after each of the 10 low bits of x
force_inline uint32_t Part1By2(uint32_t x) {
    x = x & 0b00000000000000000000001111111111;                 // x = ---- ---- ---- ---- ---- --98 7654 3210
    x = (x ^ (x << 16)) & 0b00000011000000000000000011111111;   // x = ---- --98 ---- ---- ---- ---- 7654 3210
    x = (x ^ (x << 8)) & 0b00000011000000001111000000001111;    // x = ---- --98 ---- ---- 7654 ---- ---- 3210
    x = (x ^ (x << 4)) & 0b00000011000011000011000011000011;    // x = ---- --98 ---- 76-- --54 ---- 32-- --10
    x = (x ^ (x << 2)) & 0b00001001001001001001001001001001;    // x = ---- 9--8 --7- -6-- 5--4 --3- -2-- 1--0
    return x;
}

force_inline uint32_t EncodeMorton3(uint32_t x, uint32_t y, uint32_t z) {
    return (Part1By2(z) << 2) + (Part1By2(y) << 1) + Part1By2(x);
}

struct prim_chunk_t {
    uint32_t code, base, size;
};

const int BitsPerDim = 10;
const int BitsTotal = 3 * BitsPerDim;

void radix_sort_prim_chunks(prim_chunk_t *begin, prim_chunk_t *end, prim_chunk_t *begin1) {
    prim_chunk_t *end1 = begin1 + (end - begin);

    const int bits_per_pass = 6;
    const int bucket_size = 1 << bits_per_pass;
    const int bit_mask = bucket_size - 1;

    for (int shift = 0; shift < BitsTotal; shift += bits_per_pass) {
        size_t count[bucket_size] = {};
        for (prim_chunk_t *p = begin; p != end; p++) {
            count[(p->code >> shift) & bit_mask]++;
        }
        prim_chunk_t *bucket[bucket_size], *q = begin1;
        for (int i = 0; i < bucket_size; q += count[i++]) {
            bucket[i] = q;
        }
        for (prim_chunk_t *p = begin; p != end; p++) {
            *bucket[(p->code >> shift) & bit_mask]++ = *p;
        }
        std::swap(begin, begin1);
        std::swap(end, end1);
    }
}

void sort_mort_codes(uint32_t *morton_codes, size_t prims_count, uint32_t *out_indices) {
    std::vector<prim_chunk_t> run_chunks;
    run_chunks.reserve(prims_count);

    for (uint32_t start = 0, end = 1; end <= (uint32_t)prims_count; end++) {
        if (end == (uint32_t)prims_count ||
            (morton_codes[start] != morton_codes[end])) {

            run_chunks.push_back({ morton_codes[start], start, end - start });

            start = end;
        }
    }

    std::vector<prim_chunk_t> run_chunks2(run_chunks.size());

    radix_sort_prim_chunks(&run_chunks[0], &run_chunks[0] + run_chunks.size(), &run_chunks2[0]);
    std::swap(run_chunks, run_chunks2);

    size_t counter = 0;
    for (const auto &ch : run_chunks) {
        for (uint32_t j = 0; j < ch.size; j++) {
            morton_codes[counter] = ch.code;
            out_indices[counter++] = ch.base + j;
        }
    }
}
}

// Used for fast color conversion
const float Ray::uint8_to_float_table[] = {
    0.000000000f, 0.003921569f, 0.007843138f, 0.011764706f, 0.015686275f, 0.019607844f, 0.023529412f, 0.027450981f, 0.031372551f, 0.035294119f, 0.039215688f, 0.043137256f, 0.047058824f, 0.050980393f, 0.054901961f, 0.058823530f,
    0.062745102f, 0.066666670f, 0.070588239f, 0.074509807f, 0.078431375f, 0.082352944f, 0.086274512f, 0.090196081f, 0.094117649f, 0.098039217f, 0.101960786f, 0.105882354f, 0.109803922f, 0.113725491f, 0.117647059f, 0.121568628f,
    0.125490203f, 0.129411772f, 0.133333340f, 0.137254909f, 0.141176477f, 0.145098045f, 0.149019614f, 0.152941182f, 0.156862751f, 0.160784319f, 0.164705887f, 0.168627456f, 0.172549024f, 0.176470593f, 0.180392161f, 0.184313729f,
    0.188235298f, 0.192156866f, 0.196078435f, 0.200000003f, 0.203921571f, 0.207843140f, 0.211764708f, 0.215686277f, 0.219607845f, 0.223529413f, 0.227450982f, 0.231372550f, 0.235294119f, 0.239215687f, 0.243137255f, 0.247058824f,
    0.250980407f, 0.254901975f, 0.258823544f, 0.262745112f, 0.266666681f, 0.270588249f, 0.274509817f, 0.278431386f, 0.282352954f, 0.286274523f, 0.290196091f, 0.294117659f, 0.298039228f, 0.301960796f, 0.305882365f, 0.309803933f,
    0.313725501f, 0.317647070f, 0.321568638f, 0.325490206f, 0.329411775f, 0.333333343f, 0.337254912f, 0.341176480f, 0.345098048f, 0.349019617f, 0.352941185f, 0.356862754f, 0.360784322f, 0.364705890f, 0.368627459f, 0.372549027f,
    0.376470596f, 0.380392164f, 0.384313732f, 0.388235301f, 0.392156869f, 0.396078438f, 0.400000006f, 0.403921574f, 0.407843143f, 0.411764711f, 0.415686280f, 0.419607848f, 0.423529416f, 0.427450985f, 0.431372553f, 0.435294122f,
    0.439215690f, 0.443137258f, 0.447058827f, 0.450980395f, 0.454901963f, 0.458823532f, 0.462745100f, 0.466666669f, 0.470588237f, 0.474509805f, 0.478431374f, 0.482352942f, 0.486274511f, 0.490196079f, 0.494117647f, 0.498039216f,
    0.501960814f, 0.505882382f, 0.509803951f, 0.513725519f, 0.517647088f, 0.521568656f, 0.525490224f, 0.529411793f, 0.533333361f, 0.537254930f, 0.541176498f, 0.545098066f, 0.549019635f, 0.552941203f, 0.556862772f, 0.560784340f,
    0.564705908f, 0.568627477f, 0.572549045f, 0.576470613f, 0.580392182f, 0.584313750f, 0.588235319f, 0.592156887f, 0.596078455f, 0.600000024f, 0.603921592f, 0.607843161f, 0.611764729f, 0.615686297f, 0.619607866f, 0.623529434f,
    0.627451003f, 0.631372571f, 0.635294139f, 0.639215708f, 0.643137276f, 0.647058845f, 0.650980413f, 0.654901981f, 0.658823550f, 0.662745118f, 0.666666687f, 0.670588255f, 0.674509823f, 0.678431392f, 0.682352960f, 0.686274529f,
    0.690196097f, 0.694117665f, 0.698039234f, 0.701960802f, 0.705882370f, 0.709803939f, 0.713725507f, 0.717647076f, 0.721568644f, 0.725490212f, 0.729411781f, 0.733333349f, 0.737254918f, 0.741176486f, 0.745098054f, 0.749019623f,
    0.752941191f, 0.756862760f, 0.760784328f, 0.764705896f, 0.768627465f, 0.772549033f, 0.776470602f, 0.780392170f, 0.784313738f, 0.788235307f, 0.792156875f, 0.796078444f, 0.800000012f, 0.803921580f, 0.807843149f, 0.811764717f,
    0.815686285f, 0.819607854f, 0.823529422f, 0.827450991f, 0.831372559f, 0.835294127f, 0.839215696f, 0.843137264f, 0.847058833f, 0.850980401f, 0.854901969f, 0.858823538f, 0.862745106f, 0.866666675f, 0.870588243f, 0.874509811f,
    0.878431380f, 0.882352948f, 0.886274517f, 0.890196085f, 0.894117653f, 0.898039222f, 0.901960790f, 0.905882359f, 0.909803927f, 0.913725495f, 0.917647064f, 0.921568632f, 0.925490201f, 0.929411769f, 0.933333337f, 0.937254906f,
    0.941176474f, 0.945098042f, 0.949019611f, 0.952941179f, 0.956862748f, 0.960784316f, 0.964705884f, 0.968627453f, 0.972549021f, 0.976470590f, 0.980392158f, 0.984313726f, 0.988235295f, 0.992156863f, 0.996078432f, 1.000000000f,
};

// Used to convert 16x16 sphere sector coordinates to single value
const uint8_t Ray::morton_table_16[] = { 0, 1, 4, 5, 16, 17, 20, 21, 64, 65, 68, 69, 80, 81, 84, 85 };

// Used to convert 256x256x256 grid coordinates to single value, i think it leads to more uniform distribution than (z << 16) | (y << 8) | (x << 0)
const int Ray::morton_table_256[] = {
    0,          1,          8,          9,          64,         65,         72,         73,         512,        513,        520,        521,        576,        577,        584,        585,
    4096,       4097,       4104,       4105,       4160,       4161,       4168,       4169,       4608,       4609,       4616,       4617,       4672,       4673,       4680,       4681,
    32768,      32769,      32776,      32777,      32832,      32833,      32840,      32841,      33280,      33281,      33288,      33289,      33344,      33345,      33352,      33353,
    36864,      36865,      36872,      36873,      36928,      36929,      36936,      36937,      37376,      37377,      37384,      37385,      37440,      37441,      37448,      37449,
    262144,     262145,     262152,     262153,     262208,     262209,     262216,     262217,     262656,     262657,     262664,     262665,     262720,     262721,     262728,     262729,
    266240,     266241,     266248,     266249,     266304,     266305,     266312,     266313,     266752,     266753,     266760,     266761,     266816,     266817,     266824,     266825,
    294912,     294913,     294920,     294921,     294976,     294977,     294984,     294985,     295424,     295425,     295432,     295433,     295488,     295489,     295496,     295497,
    299008,     299009,     299016,     299017,     299072,     299073,     299080,     299081,     299520,     299521,     299528,     299529,     299584,     299585,     299592,     299593,
    2097152,    2097153,    2097160,    2097161,    2097216,    2097217,    2097224,    2097225,    2097664,    2097665,    2097672,    2097673,    2097728,    2097729,    2097736,    2097737,
    2101248,    2101249,    2101256,    2101257,    2101312,    2101313,    2101320,    2101321,    2101760,    2101761,    2101768,    2101769,    2101824,    2101825,    2101832,    2101833,
    2129920,    2129921,    2129928,    2129929,    2129984,    2129985,    2129992,    2129993,    2130432,    2130433,    2130440,    2130441,    2130496,    2130497,    2130504,    2130505,
    2134016,    2134017,    2134024,    2134025,    2134080,    2134081,    2134088,    2134089,    2134528,    2134529,    2134536,    2134537,    2134592,    2134593,    2134600,    2134601,
    2359296,    2359297,    2359304,    2359305,    2359360,    2359361,    2359368,    2359369,    2359808,    2359809,    2359816,    2359817,    2359872,    2359873,    2359880,    2359881,
    2363392,    2363393,    2363400,    2363401,    2363456,    2363457,    2363464,    2363465,    2363904,    2363905,    2363912,    2363913,    2363968,    2363969,    2363976,    2363977,
    2392064,    2392065,    2392072,    2392073,    2392128,    2392129,    2392136,    2392137,    2392576,    2392577,    2392584,    2392585,    2392640,    2392641,    2392648,    2392649,
    2396160,    2396161,    2396168,    2396169,    2396224,    2396225,    2396232,    2396233,    2396672,    2396673,    2396680,    2396681,    2396736,    2396737,    2396744,    2396745
};

// Used to bind horizontal vector angle to sector on sphere
const float Ray::omega_step = 0.0625f;
const char Ray::omega_table[] = { 15, 14, 13, 12, 12, 11, 11, 11, 10, 10, 9, 9, 9, 8, 8, 8, 8, 7, 7, 7, 6, 6, 6, 5, 5, 4, 4, 4, 3, 3, 2, 1, 0 };

// Used to bind vectical vector angle to sector on sphere
const float Ray::phi_step = 0.125f;
const char Ray::phi_table[][17] = { { 2,  2,  2,  2,  2,  3,  3,  3,  4,  4,  4,  4,  5,  5,  5,  5,  6  },
                                    { 1,  2,  2,  2,  2,  2,  3,  3,  4,  4,  4,  5,  5,  5,  5,  6,  6  },
                                    { 1,  1,  2,  2,  2,  2,  3,  3,  4,  4,  4,  5,  5,  5,  6,  6,  6  },
                                    { 1,  1,  1,  2,  2,  2,  3,  3,  4,  4,  4,  5,  5,  6,  6,  6,  6  },
                                    { 1,  1,  1,  1,  2,  2,  2,  3,  4,  4,  5,  5,  6,  6,  6,  6,  6  },
                                    { 0,  1,  1,  1,  1,  2,  2,  3,  4,  4,  5,  6,  6,  6,  6,  6,  7  },
                                    { 0,  0,  0,  0,  1,  1,  2,  2,  4,  5,  6,  6,  6,  7,  7,  7,  7  },
                                    { 0,  0,  0,  0,  0,  0,  1,  2,  4,  6,  6,  7,  7,  7,  7,  7,  7  },
                                    { 15, 15, 15, 15, 15, 15, 15, 15, 8,  8,  8,  8,  8,  8,  8,  8,  8  },
                                    { 15, 15, 15, 15, 15, 15, 14, 14, 12, 10, 9,  8,  8,  8,  8,  8,  8  },
                                    { 15, 15, 15, 15, 14, 14, 14, 13, 12, 10, 10, 9,  9,  8,  8,  8,  8  },
                                    { 15, 14, 14, 14, 14, 14, 13, 12, 12, 11, 10, 10, 9,  9,  9,  9,  8  },
                                    { 14, 14, 14, 14, 14, 13, 13, 12, 12, 11, 10, 10, 10, 9,  9,  9,  9  },
                                    { 14, 14, 14, 14, 13, 13, 12, 12, 12, 11, 11, 10, 10, 10, 9,  9,  9  },
                                    { 14, 14, 14, 13, 13, 13, 12, 12, 12, 11, 11, 10, 10, 10, 10, 9,  9  },
                                    { 14, 14, 13, 13, 13, 13, 12, 12, 12, 11, 11, 10, 10, 10, 10, 10, 9  },
                                    { 14, 13, 13, 13, 13, 12, 12, 12, 12, 11, 11, 11, 10, 10, 10, 10, 10 } };

const int Ray::sampling_pattern[] = { 0, 1, 2, 3,   1, 2, 3, 0,
                                      2, 3, 0, 1,   3, 0, 1, 2,
                                      1, 2, 3, 0,   2, 1, 0, 3,
                                      3, 0, 1, 2,   0, 3, 2, 1,

                                      2, 3, 0, 1,   1, 0, 3, 2,
                                      0, 1, 2, 3,   0, 3, 2, 1,
                                      3, 2, 1, 0,   3, 2, 1, 0,
                                      1, 0, 3, 2,   2, 1, 0, 3 };

bool Ray::PreprocessTri(const float *p, int stride, tri_accel_t *acc) {
    // from "Ray-Triangle Intersection Algorithm for Modern CPU Architectures" [2007]

    if (!stride) stride = 3;

    // edges
    float e0[3] = { p[stride] - p[0], p[stride + 1] - p[1], p[stride + 2] - p[2] },
          e1[3] = { p[stride * 2] - p[0], p[stride * 2 + 1] - p[1], p[stride * 2 + 2] - p[2] };

    bool is_degenerate =
        (std::abs(e0[0]) < FLT_EPS && std::abs(e0[1]) < FLT_EPS && std::abs(e0[2]) < FLT_EPS) ||
        (std::abs(e1[0]) < FLT_EPS && std::abs(e1[1]) < FLT_EPS && std::abs(e1[2]) < FLT_EPS);

    float n[3] = { e0[1] * e1[2] - e0[2] * e1[1],
                   e0[2] * e1[0] - e0[0] * e1[2],
                   e0[0] * e1[1] - e0[1] * e1[0]
                 };

    int w, u, v;
    if (std::abs(n[0]) > std::abs(n[1]) && std::abs(n[0]) > std::abs(n[2])) {
        w = 0;
        u = 1;
        v = 2;
    } else if (std::abs(n[1]) > std::abs(n[0]) && std::abs(n[1]) > std::abs(n[2])) {
        w = 1;
        u = 0;
        v = 2;
    } else {
        w = 2;
        u = 0;
        v = 1;
    }

    if (std::abs(n[w]) < FLT_EPS) {
        n[w] = 1.0f;
    }

    acc->nu = n[u] / n[w];
    acc->nv = n[v] / n[w];
    acc->pu = p[u];
    acc->pv = p[v];
    acc->np = acc->nu * acc->pu + acc->nv * acc->pv + p[w];

    int sign = w == 1 ? -1 : 1;
    acc->e0u = sign * e0[u] / n[w];
    acc->e0v = sign * e0[v] / n[w];
    acc->e1u = sign * e1[u] / n[w];
    acc->e1v = sign * e1[v] / n[w];

    acc->ci = w;
    if (std::abs(acc->nu) < axis_aligned_normal_eps && std::abs(acc->nv) < axis_aligned_normal_eps) {
        acc->ci |= TRI_AXIS_ALIGNED_BIT;
    }
    if (n[w] < 0) {
        acc->ci |= TRI_INV_NORMAL_BIT;
    }
    assert((acc->ci & TRI_W_BITS) == w);
    return !is_degenerate;
}

void Ray::ExtractPlaneNormal(const tri_accel_t &tri, float *out_normal) {
    const int _next_u[] = { 1, 0, 0 }, _next_v[] = { 2, 2, 1 };

    const int _iw = tri.ci & TRI_W_BITS;
    out_normal[_iw] = 1.0f;
    out_normal[_next_u[_iw]] = tri.nu;
    out_normal[_next_v[_iw]] = tri.nv;
    float inv_l = std::sqrt(out_normal[0] * out_normal[0] + out_normal[1] * out_normal[1] + out_normal[2] * out_normal[2]);
    if (tri.ci & TRI_INV_NORMAL_BIT) inv_l = -inv_l;
    out_normal[0] *= inv_l;
    out_normal[1] *= inv_l;
    out_normal[2] *= inv_l;
}

uint32_t Ray::PreprocessMesh(const float *attrs, const uint32_t *vtx_indices, size_t vtx_indices_count, eVertexLayout layout, int base_vertex,
                             bool allow_spatial_splits, bool use_fast_bvh_build,
                             std::vector<bvh_node_t> &out_nodes, std::vector<tri_accel_t> &out_tris, std::vector<uint32_t> &out_tri_indices) {
    assert(vtx_indices_count && vtx_indices_count % 3 == 0);

    std::vector<prim_t> primitives;

    size_t tris_start = out_tris.size();
    size_t tris_count = vtx_indices_count / 3;
    out_tris.resize(tris_start + tris_count);

    const float *positions = attrs;
    size_t attr_stride = AttrStrides[layout];

    for (size_t j = 0; j < vtx_indices_count; j += 3) {
        float p[9];

        uint32_t i0 = vtx_indices[j + 0] + base_vertex,
                 i1 = vtx_indices[j + 1] + base_vertex,
                 i2 = vtx_indices[j + 2] + base_vertex;

        memcpy(&p[0], &positions[i0 * attr_stride], 3 * sizeof(float));
        memcpy(&p[3], &positions[i1 * attr_stride], 3 * sizeof(float));
        memcpy(&p[6], &positions[i2 * attr_stride], 3 * sizeof(float));

        PreprocessTri(&p[0], 0, &out_tris[tris_start + j / 3]);

        Ref::simd_fvec3 _min = min(Ref::simd_fvec3{ &p[0] }, min(Ref::simd_fvec3{ &p[3] }, Ref::simd_fvec3{ &p[6] })),
                        _max = max(Ref::simd_fvec3{ &p[0] }, max(Ref::simd_fvec3{ &p[3] }, Ref::simd_fvec3{ &p[6] }));

        primitives.push_back({ i0, i1, i2, _min, _max });
    }

    size_t indices_start = out_tri_indices.size();
    uint32_t num_out_nodes;
    if (!use_fast_bvh_build) {
        split_settings_t s;
        s.allow_spatial_splits = allow_spatial_splits;
        num_out_nodes = PreprocessPrims_SAH(&primitives[0], primitives.size(), positions, attr_stride, s, out_nodes, out_tri_indices);
    } else {
        num_out_nodes = PreprocessPrims_HLBVH(&primitives[0], primitives.size(), out_nodes, out_tri_indices);
    }

    for (size_t i = indices_start; i < out_tri_indices.size(); i++) {
        out_tri_indices[i] += (uint32_t)tris_start;
    }

    return num_out_nodes;
}

uint32_t Ray::EmitLBVH_Recursive(const prim_t *prims, const uint32_t *indices, const uint32_t *morton_codes, uint32_t prim_index, uint32_t prim_count, uint32_t index_offset, int bit_index, std::vector<bvh_node_t> &out_nodes) {
    if (bit_index == -1 || prim_count < 8) {
        Ref::simd_fvec3 bbox_min = { std::numeric_limits<float>::max() },
                        bbox_max = { std::numeric_limits<float>::lowest() };

        for (uint32_t i = prim_index; i < prim_index + prim_count; i++) {
            bbox_min = min(bbox_min, prims[indices[i]].bbox_min);
            bbox_max = max(bbox_max, prims[indices[i]].bbox_max);
        }

        uint32_t node_index = (uint32_t)out_nodes.size();

        out_nodes.emplace_back();
        auto &node = out_nodes.back();

        node.prim_index = LEAF_NODE_BIT + prim_index + index_offset;
        node.prim_count = prim_count;

        memcpy(&node.bbox_min[0], &bbox_min[0], 3 * sizeof(float));
        memcpy(&node.bbox_max[0], &bbox_max[0], 3 * sizeof(float));

        return node_index;
    } else {
        uint32_t mask = 1u << bit_index;

        if ((morton_codes[prim_index] & mask) == (morton_codes[prim_index + prim_count - 1] & mask)) {
            return EmitLBVH_Recursive(prims, indices, morton_codes, prim_index, prim_count, index_offset, bit_index - 1, out_nodes);
        }

        uint32_t search_start = prim_index, search_end = search_start + prim_count - 1;
        while (search_start + 1 != search_end) {
            uint32_t mid = (search_start + search_end) / 2;
            if ((morton_codes[search_start] & mask) == (morton_codes[mid] & mask)) {
                search_start = mid;
            } else {
                search_end = mid;
            }
        }

        uint32_t split_offset = search_end - prim_index;

        uint32_t node_index = (uint32_t)out_nodes.size();
        out_nodes.emplace_back();

        uint32_t child0 = EmitLBVH_Recursive(prims, indices, morton_codes, prim_index, split_offset, index_offset, bit_index - 1, out_nodes);
        uint32_t child1 = EmitLBVH_Recursive(prims, indices, morton_codes, prim_index + split_offset, prim_count - split_offset, index_offset, bit_index - 1, out_nodes);

        uint32_t space_axis = bit_index % 3;
        if (out_nodes[child0].bbox_min[space_axis] > out_nodes[child1].bbox_min[space_axis]) {
            std::swap(child0, child1);
        }

        auto &par_node = out_nodes[node_index];
        par_node.left_child = child0;
        par_node.right_child = (space_axis << 30) + child1;

        for (int i = 0; i < 3; i++) {
            par_node.bbox_min[i] = std::min(out_nodes[child0].bbox_min[i], out_nodes[child1].bbox_min[i]);
            par_node.bbox_max[i] = std::max(out_nodes[child0].bbox_max[i], out_nodes[child1].bbox_max[i]);
        }

        return node_index;
    }
}

uint32_t Ray::EmitLBVH_NonRecursive(const prim_t *prims, const uint32_t *indices, const uint32_t *morton_codes, uint32_t prim_index, uint32_t prim_count, uint32_t index_offset, int bit_index, std::vector<bvh_node_t> &out_nodes) {
    struct proc_item_t {
        int bit_index;
        uint32_t prim_index, prim_count;
        uint32_t split_offset, node_index;
    };

    proc_item_t proc_stack[256];
    uint32_t stack_size = 0;

    uint32_t root_node_index = (uint32_t)out_nodes.size();
    out_nodes.emplace_back();
    proc_stack[stack_size++] = { bit_index, prim_index, prim_count, 0xffffffff, root_node_index };

    while (stack_size) {
        proc_item_t &cur = proc_stack[stack_size - 1];

        if (cur.bit_index == -1 || cur.prim_count < 8) {
            Ref::simd_fvec3 bbox_min = { std::numeric_limits<float>::max() },
                bbox_max = { std::numeric_limits<float>::lowest() };

            for (uint32_t i = cur.prim_index; i < cur.prim_index + cur.prim_count; i++) {
                bbox_min = min(bbox_min, prims[indices[i]].bbox_min);
                bbox_max = max(bbox_max, prims[indices[i]].bbox_max);
            }

            auto &node = out_nodes[cur.node_index];

            node.prim_index = LEAF_NODE_BIT + cur.prim_index + index_offset;
            node.prim_count = cur.prim_count;

            memcpy(&node.bbox_min[0], &bbox_min[0], 3 * sizeof(float));
            memcpy(&node.bbox_max[0], &bbox_max[0], 3 * sizeof(float));
        } else {
            if (cur.split_offset == 0xffffffff) {
                uint32_t mask = 1u << cur.bit_index;

                uint32_t search_start = cur.prim_index, search_end = search_start + cur.prim_count - 1;

                if ((morton_codes[search_start] & mask) == (morton_codes[search_end] & mask)) {
                    cur.bit_index--;
                    continue;
                }

                while (search_start + 1 != search_end) {
                    uint32_t mid = (search_start + search_end) / 2;
                    if ((morton_codes[search_start] & mask) == (morton_codes[mid] & mask)) {
                        search_start = mid;
                    } else {
                        search_end = mid;
                    }
                }

                cur.split_offset = search_end - cur.prim_index;

                uint32_t child0 = (uint32_t)out_nodes.size();
                out_nodes.emplace_back();

                uint32_t child1 = (uint32_t)out_nodes.size();
                out_nodes.emplace_back();

                out_nodes[cur.node_index].left_child = child0;
                out_nodes[cur.node_index].right_child = child1;

                proc_stack[stack_size++] = { cur.bit_index - 1, cur.prim_index + cur.split_offset, cur.prim_count - cur.split_offset, 0xffffffff, child1 };
                proc_stack[stack_size++] = { cur.bit_index - 1, cur.prim_index, cur.split_offset, 0xffffffff, child0 };
                continue;
            } else {
                auto &node = out_nodes[cur.node_index];

                for (int i = 0; i < 3; i++) {
                    node.bbox_min[i] = std::min(out_nodes[node.left_child].bbox_min[i], out_nodes[node.right_child].bbox_min[i]);
                    node.bbox_max[i] = std::max(out_nodes[node.left_child].bbox_max[i], out_nodes[node.right_child].bbox_max[i]);
                }

                uint32_t space_axis = cur.bit_index % 3;
                if (out_nodes[node.left_child].bbox_min[space_axis] > out_nodes[node.right_child].bbox_min[space_axis]) {
                    std::swap(node.left_child, node.right_child);
                }
                node.right_child += (space_axis << 30);
            }
        }

        stack_size--;
    }

    return root_node_index;
}

uint32_t Ray::PreprocessPrims_SAH(const prim_t *prims, size_t prims_count, const float *positions, size_t stride,
                                  const split_settings_t &s, std::vector<bvh_node_t> &out_nodes, std::vector<uint32_t> &out_indices) {
    struct prims_coll_t {
        std::vector<uint32_t> indices;
        Ref::simd_fvec3 min = { std::numeric_limits<float>::max() }, max = { std::numeric_limits<float>::lowest() };
        prims_coll_t() {}
        prims_coll_t(std::vector<uint32_t> &&_indices, const Ref::simd_fvec3 &_min, const Ref::simd_fvec3 &_max)
            : indices(std::move(_indices)), min(_min), max(_max) {
        }
    };

    std::deque<prims_coll_t> prim_lists;
    prim_lists.emplace_back();

    size_t num_nodes = out_nodes.size();
    auto root_node_index = (uint32_t)num_nodes;

    for (size_t j = 0; j < prims_count; j++) {
        prim_lists.back().indices.push_back((uint32_t)j);
        prim_lists.back().min = min(prim_lists.back().min, prims[j].bbox_min);
        prim_lists.back().max = max(prim_lists.back().max, prims[j].bbox_max);
    }

    Ref::simd_fvec3 root_min = prim_lists.back().min,
                    root_max = prim_lists.back().max;

    while (!prim_lists.empty()) {
        auto split_data = SplitPrimitives_SAH(prims, prim_lists.back().indices, positions, stride, prim_lists.back().min, prim_lists.back().max, root_min, root_max, s);
        prim_lists.pop_back();

#ifdef USE_STACKLESS_BVH_TRAVERSAL
        uint32_t leaf_index = (uint32_t)out_nodes.size(),
                 parent_index = 0xffffffff;

        if (leaf_index) {
            // skip bound checks in debug mode
            const bvh_node_t *_out_nodes = &out_nodes[0];
            for (uint32_t i = leaf_index - 1; i >= root_node_index; i--) {
                if (!(_out_nodes[i].prim_index & LEAF_NODE_BIT) &&
                    (_out_nodes[i].left_child == leaf_index || (_out_nodes[i].right_child & RIGHT_CHILD_BITS) == leaf_index)) {
                    parent_index = (uint32_t)i;
                    break;
                }
            }
        }
#endif

        if (split_data.right_indices.empty()) {
            Ref::simd_fvec3 bbox_min = split_data.left_bounds[0],
                            bbox_max = split_data.left_bounds[1];

            out_nodes.emplace_back();
            auto &n = out_nodes.back();

            n.prim_index = LEAF_NODE_BIT + (uint32_t)out_indices.size();
            n.prim_count = (uint32_t)split_data.left_indices.size();
            memcpy(&n.bbox_min[0], &bbox_min[0], 3 * sizeof(float));
            memcpy(&n.bbox_max[0], &bbox_max[0], 3 * sizeof(float));
#ifdef USE_STACKLESS_BVH_TRAVERSAL
            n.parent_index = parent_index;
#endif
            out_indices.insert(out_indices.end(), split_data.left_indices.begin(), split_data.left_indices.end());
        } else {
            auto index = (uint32_t)num_nodes;

            uint32_t space_axis = 0;
            Ref::simd_fvec3 c_left = (split_data.left_bounds[0] + split_data.left_bounds[1]) / 2,
                            c_right = (split_data.right_bounds[0] + split_data.right_bounds[1]) / 2;

            Ref::simd_fvec3 dist = abs(c_left - c_right);

            if (dist[0] > dist[1] && dist[0] > dist[2]) {
                space_axis = 0;
            } else if (dist[1] > dist[0] && dist[1] > dist[2]) {
                space_axis = 1;
            } else {
                space_axis = 2;
            }

            Ref::simd_fvec3 bbox_min = min(split_data.left_bounds[0], split_data.right_bounds[0]),
                            bbox_max = max(split_data.left_bounds[1], split_data.right_bounds[1]);

            out_nodes.emplace_back();
            auto &n = out_nodes.back();
            n.left_child = index + 1;
            n.right_child = (space_axis << 30) + index + 2;
            memcpy(&n.bbox_min[0], &bbox_min[0], 3 * sizeof(float));
            memcpy(&n.bbox_max[0], &bbox_max[0], 3 * sizeof(float));
#ifdef USE_STACKLESS_BVH_TRAVERSAL
            n.parent_index = parent_index;
#endif
            prim_lists.emplace_front(std::move(split_data.left_indices), split_data.left_bounds[0], split_data.left_bounds[1]);
            prim_lists.emplace_front(std::move(split_data.right_indices), split_data.right_bounds[0], split_data.right_bounds[1]);

            num_nodes += 2;
        }
    }

    return (uint32_t)(out_nodes.size() - root_node_index);
}

uint32_t Ray::PreprocessPrims_HLBVH(const prim_t *prims, size_t prims_count, std::vector<bvh_node_t> &out_nodes, std::vector<uint32_t> &out_indices) {
    std::vector<uint32_t> morton_codes(prims_count);

    Ref::simd_fvec3 whole_min = { std::numeric_limits<float>::max() },
                    whole_max = { std::numeric_limits<float>::lowest() };

    uint32_t indices_start = (uint32_t)out_indices.size();

    for (uint32_t j = 0; j < prims_count; j++) {
        whole_min = min(whole_min, prims[j].bbox_min);
        whole_max = max(whole_max, prims[j].bbox_max);

        out_indices.push_back(j);
    }

    uint32_t *indices = &out_indices[indices_start];

    const Ref::simd_fvec3 scale = (1 << BitsPerDim) / (whole_max - whole_min);

    // compute morton codes
    for (size_t i = 0; i < prims_count; i++) {
        Ref::simd_fvec3 center = 0.5f * (prims[i].bbox_min + prims[i].bbox_max);
        Ref::simd_fvec3 code = (center - whole_min) * scale;

        uint32_t x = (uint32_t)code[0],
                 y = (uint32_t)code[1],
                 z = (uint32_t)code[2];

        uint32_t mort = EncodeMorton3(x, y, z);
        morton_codes[i] = mort;
    }

    sort_mort_codes(&morton_codes[0], morton_codes.size(), indices);

    struct treelet_t {
        uint32_t index, count;
        uint32_t node_index;
    };

    std::vector<treelet_t> treelets;
    treelets.reserve(1 << 12); // Top-level bvh can have up to 4096 items

    // Use upper 12 bits to extract treelets
    for (uint32_t start = 0, end = 1; end <= (uint32_t)morton_codes.size(); end++) {
        uint32_t mask = 0b00111111111111000000000000000000;
        if (end == (uint32_t)morton_codes.size() ||
            ((morton_codes[start] & mask) != (morton_codes[end] & mask))) {

            treelets.push_back({ start, end - start });

            start = end;
        }
    }

    std::vector<bvh_node_t> bottom_nodes;

    // Build bottom-level hierarchy from each treelet using LBVH
    const int start_bit = 29 - 12;
    for (auto &tr : treelets) {
        tr.node_index = EmitLBVH_NonRecursive(prims, indices, &morton_codes[0], tr.index, tr.count, indices_start, start_bit, bottom_nodes);
    }

    std::vector<prim_t> top_prims;
    for (const auto &tr : treelets) {
        const auto &node = bottom_nodes[tr.node_index];

        top_prims.emplace_back();
        auto &p = top_prims.back();
        memcpy(&p.bbox_min[0], node.bbox_min, 3 * sizeof(float));
        memcpy(&p.bbox_max[0], node.bbox_max, 3 * sizeof(float));
    }

    uint32_t top_nodes_start = (uint32_t)out_nodes.size();

    std::vector<uint32_t> top_indices;

    // Force spliting until each primitive will be in separate leaf node
    split_settings_t s;
    s.oversplit_threshold = std::numeric_limits<float>::max();
    s.node_traversal_cost = 0.0f;
    s.allow_spatial_splits = false;

    // Build top level hierarchy using SAH
    uint32_t new_nodes_count = PreprocessPrims_SAH(&top_prims[0], top_prims.size(), nullptr, 0, s, out_nodes, top_indices);
    unused(new_nodes_count);

    uint32_t bottom_nodes_start = (uint32_t)out_nodes.size();

    // Replace leaf nodes of top-level bvh with bottom level nodes
    for (uint32_t i = top_nodes_start; i < (uint32_t)out_nodes.size(); i++) {
        auto &n = out_nodes[i];
        if (!(n.prim_index & LEAF_NODE_BIT)) {
            auto &left = out_nodes[n.left_child],
                 &right = out_nodes[n.right_child & RIGHT_CHILD_BITS];

            if (left.prim_index & LEAF_NODE_BIT) {
                assert(left.prim_count == 1);
                uint32_t index = (left.prim_index & PRIM_INDEX_BITS);

                const auto &tr = treelets[top_indices[index]];
                n.left_child = bottom_nodes_start + tr.node_index;
            }

            if (right.prim_index & LEAF_NODE_BIT) {
                assert(right.prim_count == 1);
                uint32_t index = (right.prim_index & PRIM_INDEX_BITS);

                const auto &tr = treelets[top_indices[index]];
                n.right_child = (n.right_child & SEP_AXIS_BITS) + bottom_nodes_start + tr.node_index;
            }
        }
    }

    // Remove top-level leaf nodes
    for (auto it = out_nodes.begin() + top_nodes_start; it != out_nodes.end(); ) {
        if (it->prim_index & LEAF_NODE_BIT) {
            uint32_t index = (uint32_t)std::distance(out_nodes.begin(), it);

            it = out_nodes.erase(it);
            bottom_nodes_start--;

            for (auto next_it = out_nodes.begin() + top_nodes_start; next_it != out_nodes.end(); ++next_it) {
                if (!(next_it->prim_index & LEAF_NODE_BIT)) {
                    if (next_it->left_child > index) {
                        next_it->left_child--;
                    }
                    if ((next_it->right_child & RIGHT_CHILD_BITS) > index) {
                        next_it->right_child--;
                    }
                }
            }
        } else {
            ++it;
        }
    }

    uint32_t bottom_nodes_offset = bottom_nodes_start;

    // Offset nodes in bottom-level bvh
    for (auto &n : bottom_nodes) {
        if (!(n.prim_index & LEAF_NODE_BIT)) {
            n.left_child += bottom_nodes_offset;
            n.right_child += bottom_nodes_offset;
        }
    }

    out_nodes.insert(out_nodes.end(), bottom_nodes.begin(), bottom_nodes.end());

    return (uint32_t)(out_nodes.size() - top_nodes_start);
}

bool Ray::NaiivePluckerTest(const float p[9], const float o[3], const float d[3]) {
    // plucker coordinates for edges
    float e0[6] = { p[6] - p[0], p[7] - p[1], p[8] - p[2],
                    p[7] * p[2] - p[8] * p[1],
                    p[8] * p[0] - p[6] * p[2],
                    p[6] * p[1] - p[7] * p[0]
                  },
                e1[6] = { p[3] - p[6], p[4] - p[7], p[5] - p[8],
                        p[4] * p[8] - p[5] * p[7],
                        p[5] * p[6] - p[3] * p[8],
                        p[3] * p[7] - p[4] * p[6]
                        },
                          e2[6] = { p[0] - p[3], p[1] - p[4], p[2] - p[5],
                                    p[1] * p[5] - p[2] * p[4],
                                    p[2] * p[3] - p[0] * p[5],
                                    p[0] * p[4] - p[1] * p[3]
                                  };

    // plucker coordinates for Ray
    float R[6] = { d[1] * o[2] - d[2] * o[1],
                   d[2] * o[0] - d[0] * o[2],
                   d[0] * o[1] - d[1] * o[0],
                   d[0], d[1], d[2]
                 };

    float t0 = 0, t1 = 0, t2 = 0;
    for (int w = 0; w < 6; w++) {
        t0 += e0[w] * R[w];
        t1 += e1[w] * R[w];
        t2 += e2[w] * R[w];
    }

    return (t0 <= 0 && t1 <= 0 && t2 <= 0) || (t0 >= 0 && t1 >= 0 && t2 >= 0);
}

void Ray::ConstructCamera(eCamType type, eFilterType filter, const float origin[3], const float fwd[3], float fov, float gamma, float focus_distance, float focus_factor, camera_t *cam) {
    if (type == Persp) {
        Ref::simd_fvec3 o = { origin };
        Ref::simd_fvec3 f = { fwd };
        Ref::simd_fvec3 u = { 0, 1, 0 };

        Ref::simd_fvec3 s = normalize(cross(f, u));
        u = cross(s, f);

        cam->type = type;
        cam->filter = filter;
        cam->fov = fov;
        cam->gamma = gamma;
        cam->focus_distance = focus_distance;
        cam->focus_factor = focus_factor;
        memcpy(&cam->origin[0], &o[0], 3 * sizeof(float));
        memcpy(&cam->fwd[0], &f[0], 3 * sizeof(float));
        memcpy(&cam->side[0], &s[0], 3 * sizeof(float));
        memcpy(&cam->up[0], &u[0], 3 * sizeof(float));
    } else if (type == Ortho) {
        // TODO!
    }
}

void Ray::TransformBoundingBox(const float bbox_min[3], const float bbox_max[3], const float *xform, float out_bbox_min[3], float out_bbox_max[3]) {
    out_bbox_min[0] = out_bbox_max[0] = xform[12];
    out_bbox_min[1] = out_bbox_max[1] = xform[13];
    out_bbox_min[2] = out_bbox_max[2] = xform[14];

    for (int j = 0; j < 3; j++) {
        for (int i = 0; i < 3; i++) {
            float a = xform[i * 4 + j] * bbox_min[i];
            float b = xform[i * 4 + j] * bbox_max[i];

            if (a < b) {
                out_bbox_min[j] += a;
                out_bbox_max[j] += b;
            } else {
                out_bbox_min[j] += b;
                out_bbox_max[j] += a;
            }
        }
    }
}

void Ray::InverseMatrix(const float mat[16], float out_mat[16]) {
    float A2323 = mat[10] * mat[15] - mat[11] * mat[14];
    float A1323 = mat[9] * mat[15] - mat[11] * mat[13];
    float A1223 = mat[9] * mat[14] - mat[10] * mat[13];
    float A0323 = mat[8] * mat[15] - mat[11] * mat[12];
    float A0223 = mat[8] * mat[14] - mat[10] * mat[12];
    float A0123 = mat[8] * mat[13] - mat[9] * mat[12];
    float A2313 = mat[6] * mat[15] - mat[7] * mat[14];
    float A1313 = mat[5] * mat[15] - mat[7] * mat[13];
    float A1213 = mat[5] * mat[14] - mat[6] * mat[13];
    float A2312 = mat[6] * mat[11] - mat[7] * mat[10];
    float A1312 = mat[5] * mat[11] - mat[7] * mat[9];
    float A1212 = mat[5] * mat[10] - mat[6] * mat[9];
    float A0313 = mat[4] * mat[15] - mat[7] * mat[12];
    float A0213 = mat[4] * mat[14] - mat[6] * mat[12];
    float A0312 = mat[4] * mat[11] - mat[7] * mat[8];
    float A0212 = mat[4] * mat[10] - mat[6] * mat[8];
    float A0113 = mat[4] * mat[13] - mat[5] * mat[12];
    float A0112 = mat[4] * mat[9] - mat[5] * mat[8];

    float inv_det = 1.0f / (mat[0] * (mat[5] * A2323 - mat[6] * A1323 + mat[7] * A1223)
                            - mat[1] * (mat[4] * A2323 - mat[6] * A0323 + mat[7] * A0223)
                            + mat[2] * (mat[4] * A1323 - mat[5] * A0323 + mat[7] * A0123)
                            - mat[3] * (mat[4] * A1223 - mat[5] * A0223 + mat[6] * A0123));

    out_mat[0] = inv_det *   (mat[5] * A2323 - mat[6] * A1323 + mat[7] * A1223);
    out_mat[1] = inv_det * -(mat[1] * A2323 - mat[2] * A1323 + mat[3] * A1223);
    out_mat[2] = inv_det *   (mat[1] * A2313 - mat[2] * A1313 + mat[3] * A1213);
    out_mat[3] = inv_det * -(mat[1] * A2312 - mat[2] * A1312 + mat[3] * A1212);
    out_mat[4] = inv_det * -(mat[4] * A2323 - mat[6] * A0323 + mat[7] * A0223);
    out_mat[5] = inv_det *   (mat[0] * A2323 - mat[2] * A0323 + mat[3] * A0223);
    out_mat[6] = inv_det * -(mat[0] * A2313 - mat[2] * A0313 + mat[3] * A0213);
    out_mat[7] = inv_det *   (mat[0] * A2312 - mat[2] * A0312 + mat[3] * A0212);
    out_mat[8] = inv_det *   (mat[4] * A1323 - mat[5] * A0323 + mat[7] * A0123);
    out_mat[9] = inv_det * -(mat[0] * A1323 - mat[1] * A0323 + mat[3] * A0123);
    out_mat[10] = inv_det *   (mat[0] * A1313 - mat[1] * A0313 + mat[3] * A0113);
    out_mat[11] = inv_det * -(mat[0] * A1312 - mat[1] * A0312 + mat[3] * A0112);
    out_mat[12] = inv_det * -(mat[4] * A1223 - mat[5] * A0223 + mat[6] * A0123);
    out_mat[13] = inv_det *   (mat[0] * A1223 - mat[1] * A0223 + mat[2] * A0123);
    out_mat[14] = inv_det * -(mat[0] * A1213 - mat[1] * A0213 + mat[2] * A0113);
    out_mat[15] = inv_det *   (mat[0] * A1212 - mat[1] * A0212 + mat[2] * A0112);
}