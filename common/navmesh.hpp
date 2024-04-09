//
// Copyright (c) 2021 Bruce https://github.com/sniper00/moon
//
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <fstream>
#include <array>
#include <random>
#include <cstring>

#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>
#include <DetourCommon.h>
#include <DetourNavMesh.h>
#include <DetourTileCache.h>
#include <DetourTileCacheBuilder.h>
#include <fastlz/fastlz.h>

namespace moon
{
    enum  PolyAreas
    {
        POLYAREA_GROUND = 0,
        POLYAREA_WATER = 1,
        POLYAREA_ROAD = 2,
        POLYAREA_DOOR = 3,
        POLYAREA_GRASS = 4,
        POLYAREA_JUMP = 5,
    };

    enum  PolyFlags
    {
        POLYFLAGS_WALK = 0x01,      // Ability to walk (ground, grass, road)
        POLYFLAGS_SWIM = 0x02,      // Ability to swim (water).
        POLYFLAGS_DOOR = 0x04,      // Ability to move through doors.
        POLYFLAGS_JUMP = 0x08,      // Ability to jump.
        POLYFLAGS_DISABLED = 0x10,  // Disabled polygon
        POLYFLAGS_ALL = 0xffff      // All abilities.
    };

    constexpr float DEFAULT_AREA_COST_GROUND = 1.0f;
    constexpr float DEFAULT_AREA_COST_WATER = 10.0f;
    constexpr float DEFAULT_AREA_COST_ROAD = 1.0f;
    constexpr float DEFAULT_AREA_COST_DOOR = 1.0f;
    constexpr float DEFAULT_AREA_COST_GRASS = 2.0f;
    constexpr float DEFAULT_AREA_COST_JUMP = 1.5f;

    constexpr unsigned short DEFAULT_INCLUDE_FLAGS = POLYFLAGS_ALL ^ POLYFLAGS_DISABLED;
    constexpr unsigned short DEFAULT_EXCLUDE_FLAGS = 0;

    struct LinearAllocator : public dtTileCacheAlloc
    {
        unsigned char* buffer;
        size_t capacity;
        size_t top;
        size_t high;

        LinearAllocator(const size_t cap) : buffer(0), capacity(0), top(0), high(0)
        {
            resize(cap);
        }

        ~LinearAllocator()
        {
            dtFree(buffer);
        }

        void resize(const size_t cap)
        {
            if (buffer) dtFree(buffer);
            buffer = (unsigned char*)dtAlloc(cap, DT_ALLOC_PERM);
            capacity = cap;
        }

        virtual void reset()
        {
            high = dtMax(high, top);
            top = 0;
        }

        virtual void* alloc(const size_t size)
        {
            if (!buffer)
                return 0;
            if (top + size > capacity)
                return 0;
            unsigned char* mem = &buffer[top];
            top += size;
            return mem;
        }

        virtual void free(void* /*ptr*/)
        {
            // Empty
        }
    };

    struct MeshProcess : public dtTileCacheMeshProcess
    {
        /*InputGeom* m_geom;

        inline MeshProcess() : m_geom(0)
        {
        }

        inline void init(InputGeom* geom)
        {
            m_geom = geom;
        }*/

        virtual void process(struct dtNavMeshCreateParams* params,
            unsigned char* polyAreas, unsigned short* polyFlags)
        {
            // Update poly flags from areas.
            for (int i = 0; i < params->polyCount; ++i)
            {
                if (polyAreas[i] == DT_TILECACHE_WALKABLE_AREA)
                    polyAreas[i] = POLYAREA_GROUND;

                if (polyAreas[i] == POLYAREA_GROUND ||
                    polyAreas[i] == POLYAREA_GRASS ||
                    polyAreas[i] == POLYAREA_ROAD)
                {
                    polyFlags[i] = POLYFLAGS_WALK;
                }
                else if (polyAreas[i] == POLYAREA_WATER)
                {
                    polyFlags[i] = POLYFLAGS_SWIM;
                }
                else if (polyAreas[i] == POLYAREA_DOOR)
                {
                    polyFlags[i] = POLYFLAGS_WALK | POLYFLAGS_DOOR;
                }
            }

            // Pass in off-mesh connections.
            //if (m_geom)
            //{
            //	params->offMeshConVerts = m_geom->getOffMeshConnectionVerts();
            //	params->offMeshConRad = m_geom->getOffMeshConnectionRads();
            //	params->offMeshConDir = m_geom->getOffMeshConnectionDirs();
            //	params->offMeshConAreas = m_geom->getOffMeshConnectionAreas();
            //	params->offMeshConFlags = m_geom->getOffMeshConnectionFlags();
            //	params->offMeshConUserID = m_geom->getOffMeshConnectionId();
            //	params->offMeshConCount = m_geom->getOffMeshConnectionCount();
            //}
        }
    };

    struct FastLZCompressor : public dtTileCacheCompressor
    {
        virtual int maxCompressedSize(const int bufferSize)
        {
            return (int)(bufferSize * 1.05f);
        }

        virtual dtStatus compress(const unsigned char* buffer, const int bufferSize,
            unsigned char* compressed, const int /*maxCompressedSize*/, int* compressedSize)
        {
            *compressedSize = fastlz_compress((const void* const)buffer, bufferSize, compressed);
            return DT_SUCCESS;
        }

        virtual dtStatus decompress(const unsigned char* compressed, const int compressedSize,
            unsigned char* buffer, const int maxBufferSize, int* bufferSize)
        {
            *bufferSize = fastlz_decompress(compressed, compressedSize, buffer, maxBufferSize);
            return *bufferSize < 0 ? DT_FAILURE : DT_SUCCESS;
        }
    };

    class Filter {
    public:
        Filter()
            : mFilter(new dtQueryFilter())
        {
            mFilter->setAreaCost(POLYAREA_GROUND, DEFAULT_AREA_COST_GROUND);
            mFilter->setAreaCost(POLYAREA_WATER, DEFAULT_AREA_COST_WATER);
            mFilter->setAreaCost(POLYAREA_ROAD, DEFAULT_AREA_COST_ROAD);
            mFilter->setAreaCost(POLYAREA_DOOR, DEFAULT_AREA_COST_DOOR);
            mFilter->setAreaCost(POLYAREA_GRASS, DEFAULT_AREA_COST_GRASS);
            mFilter->setAreaCost(POLYAREA_JUMP, DEFAULT_AREA_COST_JUMP);

            mFilter->setIncludeFlags(DEFAULT_INCLUDE_FLAGS);
            mFilter->setExcludeFlags(DEFAULT_EXCLUDE_FLAGS);
        }

        virtual ~Filter() {

        }

        dtQueryFilter& Get() { return *mFilter; }

        void SetAreaCost(const int i, const float cost) {
            mFilter->setAreaCost(i, cost);
        }

        void SetIncludeFlags(const unsigned short flags) {
            mFilter->setIncludeFlags(flags);
        }

        void SetExcludeFlags(const unsigned short flags) {
            mFilter->setExcludeFlags(flags);
        }
    protected:
        std::unique_ptr<dtQueryFilter> mFilter;
    };


    class navmesh
    {
    private:
        static constexpr int NAVMESHSET_MAGIC = 'M' << 24 | 'S' << 16 | 'E' << 8 | 'T'; //'MSET';
        static constexpr int NAVMESHSET_VERSION = 1;
        static constexpr int MAX_POLYS = 2048;
        static constexpr int NAV_ERROR_NEARESTPOLY = -2;

        static constexpr int TILECACHESET_MAGIC = 'T' << 24 | 'S' << 16 | 'E' << 8 | 'T';
        static constexpr int TILECACHESET_VERSION = 1;

        //Sample.cpp
        struct NavMeshSetHeader
        {
            int magic;
            int version;
            int numTiles;
            dtNavMeshParams params;
        };

        struct NavMeshTileHeader
        {
            dtTileRef tileRef;
            int dataSize;
        };

        //Sample_TempObstacles.cpp
        struct TileCacheSetHeader
        {
            int32_t magic;
            int32_t version;
            int32_t numTiles;
            dtNavMeshParams meshParams;
            dtTileCacheParams cacheParams;
        };

        struct TileCacheTileHeader
        {
            dtCompressedTileRef tileRef;
            int32_t dataSize;
        };

        template<typename T, void(*F)(T*)>
        struct unique_deleter
        {
            void operator()(T* p)
            {
                F(p);
            }
        };

        using dtNavMeshDeleter = unique_deleter< dtNavMesh, dtFreeNavMesh>;
        using dtTileCacheDeleter = unique_deleter< dtTileCache, dtFreeTileCache>;
        using dtNavMeshQueryDeleter = unique_deleter< dtNavMeshQuery, dtFreeNavMeshQuery>;

        struct navmesh_context
        {
            std::unique_ptr<dtNavMesh, dtNavMeshDeleter> mesh = nullptr;
            std::unique_ptr<dtTileCache, dtTileCacheDeleter> tilecache = nullptr;
            std::unique_ptr<LinearAllocator> talloc;
            std::unique_ptr<FastLZCompressor> tcomp;
            std::unique_ptr<MeshProcess> tmproc;

            navmesh_context() = default;

            navmesh_context(const navmesh_context&) = delete;
            navmesh_context(navmesh_context&& other) = default;
            navmesh_context& operator=(navmesh_context&& other) = default;
        };

        static std::string read_all(const std::string& path, std::ios::openmode Mode)
        {
            std::fstream is(path, Mode);
            if (is.is_open())
            {
                // get length of file:
                is.seekg(0, is.end);
                size_t length = static_cast<size_t>(is.tellg());
                is.seekg(0, is.beg);

                std::string tmp;
                tmp.resize(length);
                is.read(&tmp.front(), length);
                is.close();
                return tmp;
            }
            return std::string();
        }

        inline static thread_local std::mt19937 generator{std::random_device{}()};
        static inline float randf()
        {
            std::uniform_real_distribution<float> dis(0.0f, 1.0f);
            return dis(generator);
        }

        void coord_transform(float* p) const
        {
            if ((coord_mask_ & negative_x_axis))
                p[0] = -p[0];
            if ((coord_mask_ & negative_y_axis))
                p[1] = -p[1];
            if ((coord_mask_ & negative_z_axis))
                p[2] = -p[2];
        }

    public:
        enum coord_transform_mask
        {
            negative_x_axis = 1 << 0,
            negative_y_axis = 1 << 1,
            negative_z_axis = 1 << 2,
        };

        static bool load_static(const std::string& meshfile, std::string& err)
        {
            auto content = read_all(meshfile, std::ios::binary | std::ios::in);
            if (content.size() < sizeof(NavMeshSetHeader))
            {
                err = "meshfile can not find or format error";
                return false;
            }

            size_t offset = 0;
            NavMeshSetHeader header = *(NavMeshSetHeader*)(content.data() + offset);
            if (header.magic != NAVMESHSET_MAGIC)
            {
                err = "'NAVMESHSET_MAGIC' not match";
                return false;
            }

            offset += sizeof(NavMeshSetHeader);

            if (header.version != NAVMESHSET_VERSION)
            {
                err = "'NAVMESHSET_VERSION' not match";
                return false;
            }

            auto mesh = std::unique_ptr<dtNavMesh, dtNavMeshDeleter>(dtAllocNavMesh());
            if (nullptr == mesh)
            {
                err = "dtAllocNavMesh failed";
                return false;
            }

            dtStatus status = mesh->init(&header.params);
            if (dtStatusFailed(status))
            {
                err = "mesh init failed";
                return false;
            }

            // Read tiles.
            bool success = true;

            for (int i = 0; i < header.numTiles; ++i)
            {
                if (content.size() - offset < sizeof(NavMeshTileHeader))
                {
                    success = false;
                    status = DT_FAILURE;
                    break;
                }

                NavMeshTileHeader tileHeader = *(NavMeshTileHeader*)(content.data() + offset);
                offset += sizeof(NavMeshTileHeader);

                if (!tileHeader.tileRef || !tileHeader.dataSize)
                {
                    success = false;
                    status = DT_FAILURE + DT_INVALID_PARAM;
                    break;
                }

                unsigned char* tileData = (unsigned char*)dtAlloc(tileHeader.dataSize, DT_ALLOC_PERM);
                if (!tileData)
                {
                    success = false;
                    status = DT_FAILURE + DT_OUT_OF_MEMORY;
                    break;
                }

                memcpy(tileData, content.data() + offset, tileHeader.dataSize);
                offset += tileHeader.dataSize;

                status = mesh->addTile(tileData
                    , tileHeader.dataSize
                    , DT_TILE_FREE_DATA
                    , tileHeader.tileRef
                    , 0);

                if (dtStatusFailed(status))
                {
                    success = false;
                    break;
                }
            }

            if (!success)
            {
                err = "load tiles ailed";
                return false;
            }

            navmesh_context ctx;
            ctx.mesh = std::move(mesh);
            static_mesh_.emplace(meshfile, std::move(ctx));
            return true;
        }

        bool load_dynamic(const std::string& meshfile, std::string& err)
        {
            auto content = read_all(meshfile, std::ios::binary | std::ios::in);
            if (content.size() < sizeof(TileCacheSetHeader))
            {
                err = "meshfile format error";
                return false;
            }

            size_t offset = 0;

            TileCacheSetHeader header = *(TileCacheSetHeader*)(content.data() + offset);
            if (header.magic != TILECACHESET_MAGIC)
            {
                err = "'TILECACHESET_MAGIC' not match";
                return false;
            }

            offset += sizeof(TileCacheSetHeader);

            if (header.version != TILECACHESET_VERSION)
            {
                err = "'TILECACHESET_VERSION' not match";
                return false;
            }

            navmesh_context ctx;
            ctx.mesh = std::unique_ptr<dtNavMesh, dtNavMeshDeleter>(dtAllocNavMesh());
            if (nullptr == ctx.mesh)
            {
                err = "dtAllocNavMesh failed";
                return false;
            }

            dtStatus status = ctx.mesh->init(&header.meshParams);
            if (dtStatusFailed(status))
            {
                err = "mesh init failed";
                return false;
            }

            ctx.tilecache = std::unique_ptr<dtTileCache, dtTileCacheDeleter>(dtAllocTileCache());
            if (nullptr == ctx.tilecache)
            {
                err = "dtAllocTileCache failed";
                return false;
            }

            ctx.talloc = std::make_unique< LinearAllocator>(32 * 1024);
            ctx.tcomp = std::make_unique< FastLZCompressor>();
            ctx.tmproc = std::make_unique< MeshProcess>();

            status = ctx.tilecache->init(&header.cacheParams, ctx.talloc.get(), ctx.tcomp.get(), ctx.tmproc.get());
            if (dtStatusFailed(status))
            {
                err = "tileCache init";
                return false;
            }

            // Read tiles.
            bool success = true;
            for (int i = 0; i < header.numTiles; ++i)
            {
                if (content.size() - offset < sizeof(TileCacheTileHeader))
                {
                    success = false;
                    status = DT_FAILURE;
                    break;
                }

                TileCacheTileHeader tileHeader = *(TileCacheTileHeader*)(content.data() + offset);
                offset += sizeof(TileCacheTileHeader);

                if (!tileHeader.tileRef || !tileHeader.dataSize)
                {
                    success = false;
                    status = DT_FAILURE + DT_INVALID_PARAM;
                    break;
                }

                unsigned char* tileData = (unsigned char*)dtAlloc(tileHeader.dataSize, DT_ALLOC_PERM);
                if (!tileData)
                {
                    success = false;
                    status = DT_FAILURE + DT_OUT_OF_MEMORY;
                    break;
                }

                memcpy(tileData, content.data() + offset, tileHeader.dataSize);
                offset += tileHeader.dataSize;

                dtCompressedTileRef tile = 0;
                status = ctx.tilecache->addTile(tileData, tileHeader.dataSize, DT_COMPRESSEDTILE_FREE_DATA, &tile);
                if (dtStatusFailed(status))
                {
                    success = false;
                    break;
                }

                if (tile)
                {
                    status = ctx.tilecache->buildNavMeshTile(tile, ctx.mesh.get());
                    if (dtStatusFailed(status))
                    {
                        success = false;
                        break;
                    }
                }
                else
                {
                    success = false;
                    break;
                }
            }

            if (!success)
            {
                err = "load tilecache failed";
                return false;
            }

            meshQuery = std::unique_ptr<dtNavMeshQuery, dtNavMeshQueryDeleter>(dtAllocNavMeshQuery());
            meshQuery->init(ctx.mesh.get(), 65535);

            dynamic_ = std::move(ctx);
            return true;
        }

        navmesh(const std::string& meshfile, int coord_mask = 0)
            :coord_mask_(coord_mask)
        {
            if (meshfile.empty())
                return;

            if (auto iter = static_mesh_.find(meshfile); iter != static_mesh_.end())
            {
                meshQuery = std::unique_ptr<dtNavMeshQuery, dtNavMeshQueryDeleter>(dtAllocNavMeshQuery());
                if (nullptr != meshQuery)
                {
                    dtStatus status = meshQuery->init(iter->second.mesh.get(), 65535);
                    if (dtStatusFailed(status))
                    {
                        meshQuery = nullptr;
                    }
                }
            }
        }

        bool find_straight_path(float sx, float sy, float sz, float ex, float ey, float ez, std::vector<float>& paths)
        {
            static thread_local  std::array<dtPolyRef, MAX_POLYS> mPolys;
            static thread_local  std::array<float, MAX_POLYS * 3> mStraightPath;
            static thread_local  std::array<uint8_t, MAX_POLYS> mStraightPathFlags;
            static thread_local  std::array<dtPolyRef, MAX_POLYS> mStraightPathPolys;

            if (!meshQuery)
            {
                return false;
            }

            status_.clear();

            float spos[3] = { sx, sy, sz };
            float epos[3] = { ex, ey, ez };

            coord_transform(spos);
            coord_transform(epos);

            const float extents[3] = { 8.0f, 4.f,8.0f };
            const float large_extents[3] = { 64.0f, 4.f, 64.0f };

            dtPolyRef startRef = 0;
            dtPolyRef endRef = 0;

            dtStatus status = meshQuery->findNearestPoly(spos, extents, &queryFilter, &startRef, nullptr);
            if (!dtStatusSucceed(status))
            {
                return false;
            }

            status = meshQuery->findNearestPoly(epos, extents, &queryFilter, &endRef, nullptr);
            if (!dtStatusSucceed(status))
            {
                return false;
            }

            if (!endRef)
            {
                status = meshQuery->findNearestPoly(epos, large_extents, &queryFilter, &endRef, nullptr);
                if (!dtStatusSucceed(status))
                {
                    return false;
                }
            }

            if (!startRef || !endRef)
            {
                status_ = "find_straight_path could not find any nearby poly's";
                return false;
            }


            int nPolys = 0;
            int nStraightPath = 0;

            if (dtVdist2DSqr(spos, epos) < 10000.0f)
            {
                float t = 0.0f;
                meshQuery->raycast(startRef, spos, epos, &queryFilter, &t, nullptr, mPolys.data(), &nPolys, MAX_POLYS);
                if (t > 1)// can direct reach
                {
                    paths.push_back(sx);
                    paths.push_back(sy);
                    paths.push_back(sz);
                    paths.push_back(ex);
                    paths.push_back(ey);
                    paths.push_back(ez);
                    return true;
                }
                nPolys = 0;
                nStraightPath = 0;
            }


            status = meshQuery->findPath(startRef, endRef, spos, epos, &queryFilter, mPolys.data(), &nPolys, MAX_POLYS);
            if (!dtStatusSucceed(status))
            {
                return false;
            }

            if (status & DT_OUT_OF_NODES)
            {
                status_ = "find_straight_path findPath: DT_OUT_OF_NODES!";
                return false;
            }

            //if (status & DT_PARTIAL_RESULT)
            //{
            //    //This happens when the A* open list is exhausted, and the goal has not been reached.
            //    //In these situations it returns the best path it can findand sets this flag to indicate that the path is not complete.
            //    return false;
            //}

            if (nPolys)
            {
                float tmp[3];
                dtVcopy(tmp, epos);

                if (mPolys[(size_t)nPolys - 1] != endRef)
                {
                    // In case of partial path, make sure the end point is clamped to the last polygon.
                    status = meshQuery->closestPointOnPoly(mPolys[(size_t)nPolys - 1], epos, tmp, 0);
                    if (!dtStatusSucceed(status))
                    {
                        return false;
                    }
                }

                status = meshQuery->findStraightPath(spos, epos, mPolys.data(), nPolys, mStraightPath.data(), mStraightPathFlags.data(), mStraightPathPolys.data(), &nStraightPath, MAX_POLYS);
                if (!dtStatusSucceed(status))
                {
                    return false;
                }

                if (status & DT_BUFFER_TOO_SMALL)
                {
                    return false;
                }

                for (int i = 0; i < nStraightPath * 3; )
                {
                    paths.push_back(mStraightPath[i++]);
                    paths.push_back(mStraightPath[i++]);
                    paths.push_back(mStraightPath[i++]);
                    coord_transform(paths.data() + (paths.size() - 3));
                }
                return true;
            }
            return false;
        }

        bool valid(float x, float y, float z) const
        {
            if (!meshQuery)
                return false;

            float pos[3] = { x, y, z };

            coord_transform(pos);

            const float extents[3] = { 2.0f, 4.f,2.0f };

            dtPolyRef Ref = 0;

            float nearst[3];
            dtStatus status = meshQuery->findNearestPoly(pos, extents, &queryFilter, &Ref, nearst);
            if (!dtStatusSucceed(status))
            {
                return false;
            }

            if (dtVdist2D(nearst, pos) > 0.0f)
            {
                return false;
            }

            return meshQuery->isValidPolyRef(Ref, &queryFilter);
        }

        bool random_position(float* out)
        {
            if (!meshQuery)
                return false;

            float rpt[3];
            dtPolyRef ref;
            dtStatus status = meshQuery->findRandomPoint(&filter_.Get(), randf, &ref, rpt);
            if (!dtStatusSucceed(status)) {
                return false;
            }

            out[0] = rpt[0];
            out[1] = rpt[1];
            out[2] = rpt[2];

            coord_transform(out);
            return true;
        }

        bool random_position_around_circle(float x, float y, float z, float r, float* out)
        {
            if (!meshQuery)
                return false;

            const float extents[3] = { 2.f, 4.f, 2.f };
            dtQueryFilter filter;
            filter.setIncludeFlags(0xffff);
            filter.setExcludeFlags(0);

            float pos[3] = { x, y, z };
            coord_transform(pos);

            dtPolyRef startRef = 0;
            float startNearestPt[3];
            dtStatus status = meshQuery->findNearestPoly(pos, extents, &filter, &startRef, startNearestPt);
            if (!dtStatusSucceed(status))
            {
                return false;
            }

            dtPolyRef randRef = 0;
            float rpt[3];
            status = meshQuery->findRandomPointAroundCircle(
                startRef,
                startNearestPt,
                r,
                &filter,
                randf,
                &randRef,
                rpt
            );

            if (!dtStatusSucceed(status)) {
                return false;
            }

            out[0] = rpt[0];
            out[1] = rpt[1];
            out[2] = rpt[2];

            coord_transform(out);

            return true;
        }

        bool recast(float sx, float sy, float sz, float ex, float ey, float ez, float* hitPos) const
        {
            thread_local static dtPolyRef polys[MAX_POLYS];

            if (!meshQuery)
            {
                return false;
            }

            float spos[3] = { sx, sy, sz };
            float epos[3] = { ex, ey, ez };

            coord_transform(spos);
            coord_transform(epos);

            dtPolyRef startRef = 0;
            dtPolyRef endRef = 0;

            const float extents[3] = { 8.0f, 4.f,8.0f };

            dtStatus status = meshQuery->findNearestPoly(spos, extents, &queryFilter, &startRef, nullptr);
            if (!dtStatusSucceed(status))
            {
                return false;
            }
            status = meshQuery->findNearestPoly(epos, extents, &queryFilter, &endRef, nullptr);
            if (!dtStatusSucceed(status))
            {
                return false;
            }

            float t = 0.0f;
            int npolys;
            meshQuery->raycast(startRef, spos, epos, &queryFilter, &t, nullptr, polys, &npolys, MAX_POLYS);
            bool hit = false;
            if (t > 1)
            {
                //No hit
                dtVcopy(hitPos, epos);
            }
            else
            {
                // Hit
                hit = true;
                dtVlerp(hitPos, spos, epos, t);
            }

            // Adjust height.
            if (npolys > 0)
            {
                float h = 0;
                meshQuery->getPolyHeight(polys[npolys - 1], hitPos, &h);
                hitPos[1] = h;
            }
            coord_transform(hitPos);
            return hit;
        }

        unsigned int add_capsule_obstacle(float x, float y, float z, float radius, float height)
        {
            if (nullptr == dynamic_.mesh || nullptr == dynamic_.tilecache)
                return 0;

            float pos[3] = { x, y, z };
            coord_transform(pos);

            dtObstacleRef obstacleId;
            dtStatus status = dynamic_.tilecache->addObstacle(pos, radius, height, &obstacleId);
            if (!dtStatusSucceed(status)) {
                return 0;
            }
            return (unsigned int)obstacleId;
        }

        bool remove_obstacle(unsigned int obstacleId)
        {
            if (nullptr == dynamic_.mesh || nullptr == dynamic_.tilecache)
                return false;
            dtStatus status = dynamic_.tilecache->removeObstacle((dtObstacleRef)obstacleId);
            return dtStatusSucceed(status);
        }

        void clear_all_obstacle()
        {
            if (nullptr == dynamic_.mesh || nullptr == dynamic_.tilecache)
                return;

            for (int i = 0; i < dynamic_.tilecache->getObstacleCount(); ++i)
            {
                const dtTileCacheObstacle* ob = dynamic_.tilecache->getObstacle(i);
                if (ob->state == DT_OBSTACLE_EMPTY) continue;
                dynamic_.tilecache->removeObstacle(dynamic_.tilecache->getObstacleRef(ob));
            }
        }

        void update(float dt)
        {
            if (nullptr == dynamic_.mesh || nullptr == dynamic_.tilecache)
                return;
            dynamic_.tilecache->update(dt, dynamic_.mesh.get());
        }

        const std::string& get_status() const
        {
            return status_;
        }
    private:
        inline static std::unordered_map<std::string, navmesh_context> static_mesh_;
        int coord_mask_ = 0;
        std::unique_ptr<dtNavMeshQuery, dtNavMeshQueryDeleter> meshQuery;
        navmesh_context dynamic_;
        Filter filter_;
        dtQueryFilter queryFilter;
        std::string status_;
    };

}

