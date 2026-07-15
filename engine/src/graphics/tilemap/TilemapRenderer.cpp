#include "graphics/tilemap/TilemapRenderer.hpp"
#include "core/Time.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "core/window_management/WindowManager.hpp"
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

void TilemapRenderer::render(TilemapComponent& tilemap, IdType windowId, const Mat4& viewProj)
{
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

    for (auto& [key, chunk] : tilemap.m_chunks) {
        if (chunk.isDirty) buildChunk(chunk, tileset);
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

    // Static geometry: the pre-baked chunk meshes.
    for (auto& [key, chunk] : tilemap.m_chunks) {
        for (size_t i = 0; i + 4 <= chunk.mesh.size(); i += 4) {
            BatchRenderer<TileVertex>::Quad quad = m_batch.nextQuad(texture);
            for (int v = 0; v < 4; v++) {
                quad.verts[v] = chunk.mesh[i + v];
                quad.verts[v].texIndex = quad.texIndex;
            }
        }
    }

    // Animated geometry: resolve each animated tile's current frame and emit it.
    const float time = Time::get().currTime();
    for (auto& [key, chunk] : tilemap.m_chunks) {
        for (const AnimatedTileInstance& a : chunk.animatedTiles) {
            TextureAtlas::Region uv = tileset.getTileUV(a.tileId, time);
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

size_t TilemapRenderer::totalIndexCount(const TilemapComponent& tilemap) const
{
    size_t total = 0;
    for (const auto& [key, chunk] : tilemap.m_chunks) {
        total += chunk.indexCount + chunk.animatedTiles.size() * 6;
    }
    return total;
}

void TilemapRenderer::buildChunk(TilemapChunk& chunk, Tileset& tileset)
{
    constexpr int S = TilemapChunk::CHUNK_SIZE;

    chunk.mesh.clear();
    chunk.animatedTiles.clear();
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

            // Animated tiles change UVs every frame, so keep them out of the
            // static mesh and re-emit them at draw time from their world corner.
            if (def->type == TileType::Animated) {
                chunk.animatedTiles.push_back({x0, y0, tile.textureId});
                continue;
            }

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
