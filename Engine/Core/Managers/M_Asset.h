#pragma once

#include <pch.h>
#include <U_Variables.h>
#include <U_Colors.h>
#include <M_File.h>
#include <M_Mesh.h>
#include <M_Texture.h>
#include <M_Config.h>

// ============================================================================
// M_Asset (Unified Asset & Resource Manager)
// ============================================================================
// Single unified access point for all assets: meshes, textures, configs,
// and files. Users do not need to hunt for separate loader headers.
//
// Usage:
//   MeshData cubeMesh = M_Asset::Create_Cube(COLOR_CYAN);
//   TextureData tex   = M_Asset::Create_Checkerboard(64, 64, 8);
// ============================================================================
class M_Asset {
public:
    // ------------------------------------------------------------------------
    // 3D Meshes
    // ------------------------------------------------------------------------
    static inline MeshData Create_Cube(const COLOR &color = COLOR_WHITE) {
        return M_Mesh::Create_Cube(color);
    }

    static inline MeshData Create_Triangle(const COLOR &c1 = COLOR_RED, const COLOR &c2 = COLOR_GREEN, const COLOR &c3 = COLOR_BLUE) {
        return M_Mesh::Create_Triangle(c1, c2, c3);
    }

    static inline MeshData Create_Quad(const COLOR &color = COLOR_WHITE) {
        return M_Mesh::Create_Quad(color);
    }

    static inline MeshData Load_Mesh(const FilePath &filePath) {
        return M_Mesh::Load(filePath);
    }

    // ------------------------------------------------------------------------
    // Textures & Images (Returns cached Texture ID for GPU)
    // ------------------------------------------------------------------------
    static inline UINT Create_Checkerboard(UINT32 width = 64, UINT32 height = 64, UINT32 checkerSize = 8,
                                           const COLOR &c1 = COLOR_WHITE, const COLOR &c2 = COLOR_GRAY) {
        return M_Texture::Create_Checkerboard(width, height, checkerSize, c1, c2);
    }

    static inline UINT Load_Texture(const FilePath &filePath) {
        return M_Texture::Load(filePath);
    }

    // ------------------------------------------------------------------------
    // Configurations (.ini)
    // ------------------------------------------------------------------------
    static inline BOOL Load_Config(M_Config &outConfig, const FilePath &filePath, const String &defaultContent = "") {
        return outConfig.Load(filePath, defaultContent);
    }

    static inline BOOL Save_Config(const M_Config &inConfig, const FilePath &filePath) {
        return inConfig.Save(filePath);
    }

    // ------------------------------------------------------------------------
    // General Files
    // ------------------------------------------------------------------------
    static inline String Read_File(const FilePath &filePath) {
        return M_File::Open(filePath);
    }

    static inline BOOL Write_File(const FilePath &filePath, const String &content, BOOL bOverwrite = true) {
        return M_File::Write(filePath, content, bOverwrite);
    }

    static inline BOOL Exists(const FilePath &filePath) {
        return M_File::Exists(filePath);
    }
};
