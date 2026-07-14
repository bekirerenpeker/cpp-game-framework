#include "graphics/tilemap/TilemapRenderer.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "graphics/Color.hpp"
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

void TilemapRenderer::render(TilemapComponent& tilemap, const Mat4& viewProj)
{
    if (!m_initialized) {
        LOG_WARNING("TilemapRenderer::render() called before init(); skipping");
        return;
    }
    if (!tilemap.m_tileset) {
        LOG_WARNING("render() called on a TilemapComponent with no tileset; skipping");
        return;
    }

    Tileset& tileset = *tilemap.m_tileset;

    for (auto& [key, chunk] : tilemap.m_chunks) {
        if (chunk.isDirty) buildChunk(chunk, tileset);
    }

    m_batch.setViewProjMat(viewProj);

    const GlTexture* texture = &tileset.getTexture();
    for (auto& [key, chunk] : tilemap.m_chunks) {
        for (size_t i = 0; i + 4 <= chunk.mesh.size(); i += 4) {
            BatchRenderer<TileVertex>::Quad quad = m_batch.nextQuad(texture);
            for (int v = 0; v < 4; v++) {
                quad.verts[v] = chunk.mesh[i + v];
                quad.verts[v].texIndex = quad.texIndex;
            }
        }
    }

    m_batch.flush();
}

size_t TilemapRenderer::totalIndexCount(const TilemapComponent& tilemap) const
{
    size_t total = 0;
    for (const auto& [key, chunk] : tilemap.m_chunks) total += chunk.indexCount;
    return total;
}

void TilemapRenderer::buildChunk(TilemapChunk& chunk, Tileset& tileset)
{
    constexpr int S = TilemapChunk::CHUNK_SIZE;

    chunk.mesh.clear();
    chunk.mesh.reserve(static_cast<size_t>(S) * S * 4);

    const int baseX = chunk.chunkX * S;
    const int baseY = chunk.chunkY * S;

    // Compact non-empty tiles so the shared sequential index buffer stays valid.
    for (int ly = 0; ly < S; ly++) {
        for (int lx = 0; lx < S; lx++) {
            const TileData& tile = chunk.tiles[ly * S + lx];
            if (tile.textureId == 0) continue;

            const Tileset::TileDefinition* def = tileset.getTile(tile.textureId);
            if (!def) continue;

            const float x0 = static_cast<float>(baseX + lx);
            const float y0 = static_cast<float>(baseY + ly);
            const float x1 = x0 + 1.0f;
            const float y1 = y0 + 1.0f;
            const Vec2 uvMin = def->uvMin, uvMax = def->uvMax;

            chunk.mesh.push_back({Vec2(x0, y0), Vec2(uvMin.x, uvMin.y), COLOR_WHITE, 0});
            chunk.mesh.push_back({Vec2(x1, y0), Vec2(uvMax.x, uvMin.y), COLOR_WHITE, 0});
            chunk.mesh.push_back({Vec2(x1, y1), Vec2(uvMax.x, uvMax.y), COLOR_WHITE, 0});
            chunk.mesh.push_back({Vec2(x0, y1), Vec2(uvMin.x, uvMax.y), COLOR_WHITE, 0});
        }
    }

    chunk.indexCount = static_cast<uint32_t>((chunk.mesh.size() / 4) * 6);
    chunk.isDirty = false;
}

}   // namespace Engine
