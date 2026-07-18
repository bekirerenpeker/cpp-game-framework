#include "graphics/tilemap/TilemapRenderer.hpp"
#include "core/Time.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "core/window_management/WindowManager.hpp"
#include "graphics/Color.hpp"
#include "graphics/Renderer.hpp"
#include "graphics/tilemap/TilemapManager.hpp"
#include "graphics/tilemap/Tileset.hpp"

namespace Engine {

void TilemapRenderer::init(GlShader* shader, size_t maxQuadCount)
{
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

void TilemapRenderer::render(TilemapComponent& tilemap, IdType windowId)
{
    const Mat4& viewProj = Renderer::get().getViewProjMat();

    if (!m_initialized) {
        LOG_WARNING("TilemapRenderer::render() called before init(); skipping");
        return;
    }
    if (!tilemap.m_tileset) {
        LOG_WARNING("render() called on a TilemapComponent with no tileset; skipping");
        return;
    }

    Window* context = WindowManager::get().getWindow(windowId);
    if (!context) {
        LOG_WARNING("TilemapRenderer::render() got an invalid windowId; skipping");
        return;
    }

    Tileset& tileset = *tilemap.m_tileset;
    const WorldBounds& visible = Renderer::get().getVisibleWorldBounds();

    for (auto& [key, chunk] : tilemap.m_chunks) {
        if (chunk.isDirty && chunkVisible(chunk, visible)) buildChunk(tilemap, chunk, tileset);
    }

    // VAOs are not shared across GL contexts, so use this context's own VAO,
    // created and configured the first time we draw into this window.
    GlVertexArray*& vao = context->vertexArray(this);
    if (!vao) {
        vao = new GlVertexArray();
        m_batch.configureVao(*vao);
    }
    m_batch.setVao(vao);

    m_batch.setViewProjMat(viewProj);

    const GlTexture* texture = &tileset.getTexture();

    for (auto& [key, chunk] : tilemap.m_chunks) {
        if (!chunkVisible(chunk, visible)) continue;
        for (size_t i = 0; i + 4 <= chunk.mesh.size(); i += 4) {
            BatchRenderer<TileVertex>::Quad quad = m_batch.nextQuad(texture);
            for (int v = 0; v < 4; v++) {
                quad.verts[v] = chunk.mesh[i + v];
                quad.verts[v].texIndex = quad.texIndex;
            }
        }
    }

    const float time = Time::get().currTime();
    for (auto& [key, chunk] : tilemap.m_chunks) {
        if (!chunkVisible(chunk, visible)) continue;
        for (const AnimatedTileInstance& a : chunk.animatedTiles) {
            TextureAtlas::Region uv = tileset.getTileUV(a.tileId, time, Vec2(a.x, a.y));
            const float x1 = a.x + 1.0f, y1 = a.y + 1.0f;

            BatchRenderer<TileVertex>::Quad quad = m_batch.nextQuad(texture);
            quad.verts[0] = {
                Vec2(a.x, a.y), Vec2(uv.uvMin.x, uv.uvMin.y), COLOR_WHITE, quad.texIndex
            };
            quad.verts[1] = {
                Vec2(x1, a.y), Vec2(uv.uvMax.x, uv.uvMin.y), COLOR_WHITE, quad.texIndex
            };
            quad.verts[2] = {
                Vec2(x1, y1), Vec2(uv.uvMax.x, uv.uvMax.y), COLOR_WHITE, quad.texIndex
            };
            quad.verts[3] = {
                Vec2(a.x, y1), Vec2(uv.uvMin.x, uv.uvMax.y), COLOR_WHITE, quad.texIndex
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
            if (tile.textureId == 0) continue;

            const Tileset::TileDefinition* def = tileset.getTile(tile.textureId);
            if (!def) continue;

            const float x0 = static_cast<float>(baseX + lx);
            const float y0 = static_cast<float>(baseY + ly);

            // Animated tiles change UVs every frame, so they're kept out of the
            // static mesh and re-emitted at draw time instead.
            if (def->type == TileType::Animated) {
                chunk.animatedTiles.push_back({x0, y0, tile.textureId});
                continue;
            }

            const float x1 = x0 + 1.0f;
            const float y1 = y0 + 1.0f;

            // Rule tiles resolve region + rotation from a live 8-neighbor bitmask
            // here at bake time, so it costs nothing per frame.
            if (def->type == TileType::Rule) {
                uint8_t mask =
                    computeNeighborMask(tilemap, tileset, tile.textureId, baseX + lx, baseY + ly);
                Tileset::RuleTileUV ruv = tileset.getRuleTileUV(tile.textureId, mask);
                std::array<Vec2, 4> uv = rotatedUVCorners(ruv.region, ruv.rotation);

                chunk.mesh.push_back({Vec2(x0, y0), uv[0], COLOR_WHITE, 0});
                chunk.mesh.push_back({Vec2(x1, y0), uv[1], COLOR_WHITE, 0});
                chunk.mesh.push_back({Vec2(x1, y1), uv[2], COLOR_WHITE, 0});
                chunk.mesh.push_back({Vec2(x0, y1), uv[3], COLOR_WHITE, 0});
                continue;
            }

            const TextureAtlas::Region uv = tileset.getTileUV(tile.textureId, 0.0f, Vec2(x0, y0));
            const Vec2 uvMin = uv.uvMin, uvMax = uv.uvMax;

            chunk.mesh.push_back({Vec2(x0, y0), Vec2(uvMin.x, uvMin.y), COLOR_WHITE, 0});
            chunk.mesh.push_back({Vec2(x1, y0), Vec2(uvMax.x, uvMin.y), COLOR_WHITE, 0});
            chunk.mesh.push_back({Vec2(x1, y1), Vec2(uvMax.x, uvMax.y), COLOR_WHITE, 0});
            chunk.mesh.push_back({Vec2(x0, y1), Vec2(uvMin.x, uvMax.y), COLOR_WHITE, 0});
        }
    }

    chunk.isDirty = false;
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
        if (tileset.tilesConnect(selfId, neighbor.textureId)) mask |= static_cast<uint8_t>(1 << i);
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

bool TilemapRenderer::chunkVisible(const TilemapChunk& chunk, const WorldBounds& bounds)
{
    constexpr int S = TilemapChunk::CHUNK_SIZE;
    const float minX = static_cast<float>(chunk.chunkX * S);
    const float minY = static_cast<float>(chunk.chunkY * S);
    return minX <= bounds.max.x && minX + S >= bounds.min.x && minY <= bounds.max.y &&
           minY + S >= bounds.min.y;
}

}   // namespace Engine
