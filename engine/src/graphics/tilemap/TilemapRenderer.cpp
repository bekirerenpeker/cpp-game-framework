#include "graphics/tilemap/TilemapRenderer.hpp"
#include "core/Time.hpp"
#include "core/file_management/FileManager.hpp"
#include "core/resource_management/ResourceManager.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "core/window_management/ViewContext.hpp"
#include "core/window_management/WindowManager.hpp"
#include "ecs/registry/View.hpp"
#include "graphics/Color.hpp"
#include "graphics/tilemap/TilemapManager.hpp"
#include "graphics/tilemap/Tileset.hpp"
#include "utils/math/Mat4.hpp"

namespace Engine {

void TilemapRenderer::init(GlShader* shader, size_t maxQuadCount)
{
    if (m_initialized) return;

    if (!shader) {
        m_defaultShaderId = ResourceManager::get().addResource<GlShader>(
            FileManager::get().engineAsset("shaders/TilemapShader.glsl")
        );
        shader = ResourceManager::get().getResource<GlShader>(m_defaultShaderId);
    }

    m_batch.init(
        maxQuadCount,
        {
            {GlDataType::Float, 2},
            {GlDataType::Float, 2},
            {GlDataType::Float, 4},
            {  GlDataType::Int, 1},
    },
        shader
    );
    m_initialized = true;
}

void TilemapRenderer::setShader(GlShader* shader) { m_batch.setShader(shader); }

void TilemapRenderer::render(Registry& registry)
{
    View<TransformComponent, TilemapComponent> view(registry);
    for (const auto& [entity, transform, tilemap] : view) {
        render(tilemap, TileGrid::fromTransform(transform));
    }
}

void TilemapRenderer::render(TilemapComponent& tilemap) { render(tilemap, TileGrid {}); }

void TilemapRenderer::render(TilemapComponent& tilemap, const TileGrid& grid)
{
    if (!m_initialized) {
        LOG_WARNING("TilemapRenderer::render() called before init(); skipping");
        return;
    }
    if (!tilemap.tileset) {
        LOG_WARNING("render() called on a TilemapComponent with no tileset; skipping");
        return;
    }
    if (!grid.isValid()) return;

    Window* context = ViewContext::get().getActiveWindow();
    if (!context) {
        LOG_WARNING("TilemapRenderer::render() called with no active window; skipping");
        return;
    }

    Tileset& tileset = *tilemap.tileset;

    // VAOs are not shared across GL contexts, so use this context's own VAO,
    // created and configured the first time we draw into this window.
    GlVertexArray*& vao = context->vertexArray(this);
    if (!vao) {
        vao = new GlVertexArray();
        m_batch.configureVao(*vao);
    }
    m_batch.setVao(vao);

    // Chunk meshes are baked in tile units, so where the tilemap sits rides in the matrix and
    // moving or resizing it never invalidates a chunk.
    Mat4 model = Mat4::translateMat(grid.origin) * Mat4::scaleMat(grid.tileSize);
    m_batch.setViewProjMat(ViewContext::get().getViewProjMat() * model);

    const GlTexture* texture = &tileset.getTexture();
    const float time = Time::get().getCurrTime();

    for (auto& [key, chunk] : tilemap.chunks) {
        if (!chunkVisible(chunk, grid)) continue;
        if (chunk.isDirty) buildChunk(tilemap, chunk, tileset);

        for (size_t i = 0; i + 4 <= chunk.mesh.size(); i += 4) {
            BatchRenderer<TileVertex>::Quad quad = m_batch.nextQuad(texture);
            for (int v = 0; v < 4; v++) {
                quad.verts[v] = chunk.mesh[i + v];
                quad.verts[v].texIndex = quad.texIndex;
            }
        }

        for (const AnimatedTileInstance& a : chunk.animatedTiles) {
            TextureAtlas::Region uv = tileset.getTileUV(a.tileId, time, a.pos);
            const Vec2 max = a.pos + VEC2_ONE;

            BatchRenderer<TileVertex>::Quad quad = m_batch.nextQuad(texture);
            quad.verts[0] = {a.pos, Vec2(uv.uvMin.x, uv.uvMin.y), COLOR_WHITE, quad.texIndex};
            quad.verts[1] = {
                Vec2(max.x, a.pos.y), Vec2(uv.uvMax.x, uv.uvMin.y), COLOR_WHITE, quad.texIndex
            };
            quad.verts[2] = {max, Vec2(uv.uvMax.x, uv.uvMax.y), COLOR_WHITE, quad.texIndex};
            quad.verts[3] = {
                Vec2(a.pos.x, max.y), Vec2(uv.uvMin.x, uv.uvMax.y), COLOR_WHITE, quad.texIndex
            };
        }
    }

    m_batch.flush();
}

void TilemapRenderer::buildChunk(TilemapComponent& tilemap, TilemapChunk& chunk, Tileset& tileset)
{
    constexpr int S = TilemapChunk::CHUNK_SIZE;

    chunk.mesh.clear();
    chunk.animatedTiles.clear();
    chunk.mesh.reserve(static_cast<size_t>(S) * S * 4);

    const int baseX = chunk.chunkX * S;
    const int baseY = chunk.chunkY * S;

    for (int ly = 0; ly < S; ly++) {
        for (int lx = 0; lx < S; lx++) {
            const TileData& tile = chunk.tiles[ly * S + lx];
            if (tile.tileId == 0) continue;

            const Tileset::TileDefinition* def = tileset.getTile(tile.tileId);
            if (!def) continue;

            const Vec2 min(static_cast<float>(baseX + lx), static_cast<float>(baseY + ly));

            // Animated tiles change UVs every frame, so they're kept out of the
            // static mesh and re-emitted at draw time instead.
            if (def->type == TileType::Animated) {
                chunk.animatedTiles.push_back({min, tile.tileId});
                continue;
            }

            const Vec2 max = min + VEC2_ONE;

            // Rule tiles resolve region + rotation from a live 8-neighbor bitmask
            // here at bake time, so it costs nothing per frame.
            if (def->type == TileType::Rule) {
                uint8_t mask =
                    computeNeighborMask(tilemap, tileset, tile.tileId, baseX + lx, baseY + ly);
                Tileset::RuleTileUV ruv = tileset.getRuleTileUV(tile.tileId, mask);
                emitQuad(chunk, min, max, rotatedUVCorners(ruv.region, ruv.rotation));
                continue;
            }

            const TextureAtlas::Region uv = tileset.getTileUV(tile.tileId, 0.0f, min);
            emitQuad(
                chunk, min, max,
                {uv.uvMin, Vec2(uv.uvMax.x, uv.uvMin.y), uv.uvMax, Vec2(uv.uvMin.x, uv.uvMax.y)}
            );
        }
    }

    chunk.isDirty = false;
}

void TilemapRenderer::emitQuad(
    TilemapChunk& chunk, Vec2 min, Vec2 max, const std::array<Vec2, 4>& uv
)
{
    chunk.mesh.push_back({min, uv[0], COLOR_WHITE, 0});
    chunk.mesh.push_back({Vec2(max.x, min.y), uv[1], COLOR_WHITE, 0});
    chunk.mesh.push_back({max, uv[2], COLOR_WHITE, 0});
    chunk.mesh.push_back({Vec2(min.x, max.y), uv[3], COLOR_WHITE, 0});
}

uint8_t TilemapRenderer::computeNeighborMask(
    TilemapComponent& tilemap, Tileset& tileset, uint16_t selfId, int gx, int gy
)
{
    // Clockwise from N (up): N, NE, E, SE, S, SW, W, NW.
    static constexpr int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static constexpr int dy[8] = {1, 1, 0, -1, -1, -1, 0, 1};

    uint8_t mask = 0;
    for (int i = 0; i < 8; i++) {
        TileData neighbor = TilemapManager::get().getAt(tilemap, gx + dx[i], gy + dy[i]);
        if (tileset.tilesConnect(selfId, neighbor.tileId)) mask |= static_cast<uint8_t>(1 << i);
    }
    return mask;
}

std::array<Vec2, 4>
TilemapRenderer::rotatedUVCorners(const TextureAtlas::Region& region, int rotation)
{
    // Corner order matches vertex order (bottom-left, bottom-right, top-right,
    // top-left), which runs counter-clockwise. Shifting backward through this
    // list makes the sampled image content appear rotated clockwise by
    // `rotation` quarter turns.
    std::array<Vec2, 4> corners = {
        region.uvMin,
        Vec2(region.uvMax.x, region.uvMin.y),
        region.uvMax,
        Vec2(region.uvMin.x, region.uvMax.y),
    };
    int shift = ((rotation % 4) + 4) % 4;
    std::array<Vec2, 4> out;
    for (int i = 0; i < 4; i++) out[i] = corners[(i - shift + 4) % 4];
    return out;
}

bool TilemapRenderer::chunkVisible(const TilemapChunk& chunk, const TileGrid& grid)
{
    constexpr int S = TilemapChunk::CHUNK_SIZE;
    const Vec2 halfExtents = grid.tileSize * (S * 0.5f);
    const Vec2 min = grid.tileMin(chunk.chunkX * S, chunk.chunkY * S);
    return ViewContext::get().isVisible(min + halfExtents, halfExtents);
}

}   // namespace Engine
