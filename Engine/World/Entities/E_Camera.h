#pragma once

#include <E_DynamicMesh.h>

// ============================================================================
// 3D Camera Entity (Inherits Velocity, MoveSpeed & Transform from E_DynamicMesh)
// ============================================================================
// - Inherits Velocity, RotationSpeed, MoveSpeed from E_DynamicMesh (for smooth camera motion!)
// - Inherits Position, Rotation, Move_Forward(), Add_Rotation(), etc. from E_Entity
// - Keeps Transparency (0.0f) and bIsVisible (false) PRIVATE so a Camera is
//   always invisible and transparent in the world!
// - Automatically registers as the active camera on creation (or via Set_Active()).
// ============================================================================
class E_Camera : public E_DynamicMesh {
private:
    // Camera is ALWAYS transparent (0.0f) and invisible (false) — hidden as private!
    using E_Entity::Transparency;
    using E_Entity::bIsVisible;
    using E_Entity::Mesh;
    using E_Entity::TextureID;
    using E_Entity::Color;
    using E_Entity::Size;

    static inline E_Camera *s_ActiveCamera = nullptr;

    // Cached camera view basis & dirty tracking (Eliminates all per-frame trig calls when stationary)
    mutable Vector3D m_CachedFwd   = { 0.0f, 0.0f, 1.0f };
    mutable Vector3D m_CachedRight = { 1.0f, 0.0f, 0.0f };
    mutable Vector3D m_CachedUp    = { 0.0f, 1.0f, 0.0f };
    mutable Vector3D m_LastRot     = { 999.0f, 999.0f, 999.0f };
    mutable Vector3D m_LastPos     = { 999.0f, 999.0f, 999.0f };
    mutable FLOAT    m_LastFOV     = 0.0f;
    mutable FLOAT    m_LastNear    = 0.0f;
    mutable FLOAT    m_LastFar     = 0.0f;

    void UpdateBasis() const {
        if (Rotation.x == m_LastRot.x && Rotation.y == m_LastRot.y && Rotation.z == m_LastRot.z) return;
        m_LastRot = Rotation;
        FLOAT pitch = Rotation.x * U_Math::DegToRad;
        FLOAT yaw   = Rotation.y * U_Math::DegToRad;
        FLOAT cp    = U_Math::Radians_Cos(pitch);
        FLOAT sp    = U_Math::Radians_Sin(pitch);
        FLOAT cy    = U_Math::Radians_Cos(yaw);
        FLOAT sy    = U_Math::Radians_Sin(yaw);

        m_CachedFwd   = { sy * cp, -sp, cy * cp };
        m_CachedRight = { cy, 0.0f, -sy };
        m_CachedUp    = { sy * sp, cp, cy * sp };
    }

public:
    FLOAT FOV      = 60.0f;  // Vertical Field of View in degrees
    FLOAT NearClip = 0.1f;   // Closest visible Z-buffer distance
    FLOAT FarClip  = 500.0f; // Farthest visible Z-buffer distance

    [[nodiscard]] Vector3D Get_Forward() const { UpdateBasis(); return m_CachedFwd; }
    [[nodiscard]] Vector3D Get_Right()   const { UpdateBasis(); return m_CachedRight; }
    [[nodiscard]] Vector3D Get_Up()      const { UpdateBasis(); return m_CachedUp; }

    E_Camera() {
        Type         = EntityType::Camera;
        bIsActive    = true;
        bIsVisible   = false; // Always invisible
        Transparency = 0.0f;  // Always transparent
        Position     = { 0.0f, 0.0f, -3.5f };
        Rotation     = { 0.0f, 0.0f,  0.0f };
        Velocity     = { 0.0f, 0.0f,  0.0f };
        MoveSpeed    = 5.0f;

        if (!s_ActiveCamera) {
            s_ActiveCamera = this;
        }
    }

    ~E_Camera() {
        if (s_ActiveCamera == this) {
            s_ActiveCamera = nullptr;
        }
    }

    // Makes this E_Camera the active viewpoint used by RHI_Manager
    void Set_Active() {
        s_ActiveCamera = this;
    }

    // Returns the currently active E_Camera (or a built-in fallback if none exists yet)
    static inline E_Camera &Get_Active() {
        static E_Camera s_FallbackCamera;
        return s_ActiveCamera ? *s_ActiveCamera : s_FallbackCamera;
    }

    // Sets the vertical Field of View in degrees (clamped between 20 and 140)
    void Set_FOV(FLOAT fovDegrees) {
        FOV = U_Math::Clamp_MinMax(fovDegrees, 20.0f, 140.0f);
    }

    // Sets the Near and Far Z-buffer clip planes
    void Set_ClipPlanes(FLOAT nearZ, FLOAT farZ) {
        NearClip = U_Math::Clamp_Min(nearZ, 0.01f);
        FarClip  = U_Math::Clamp_Min(farZ, NearClip + 1.0f);
    }

    // Pushes this camera's 3D view & projection parameters to the renderer for the frame
    void Update_CameraView() const {
        UpdateBasis();
        if (Position.x == m_LastPos.x && Position.y == m_LastPos.y && Position.z == m_LastPos.z &&
            FOV == m_LastFOV && NearClip == m_LastNear && FarClip == m_LastFar) {
            return;
        }
        m_LastPos  = Position;
        m_LastFOV  = FOV;
        m_LastNear = NearClip;
        m_LastFar  = FarClip;

        M_Mesh::Set_CameraView(
            Position,
            m_CachedRight,
            m_CachedUp,
            m_CachedFwd,
            FOV,
            NearClip,
            FarClip);
    }

    // Rotates the Camera (clamps vertical pitch to [-89, +89] degrees to prevent gimbal flip)
    void Add_Rotation(FLOAT deltaPitch, FLOAT deltaYaw, FLOAT deltaRoll = 0.0f) {
        Rotation.x = U_Math::Clamp_MinMax(Rotation.x + deltaPitch, -89.0f, 89.0f);
        Rotation.y += deltaYaw;
        while (Rotation.y >  180.0f) Rotation.y -= 360.0f;
        while (Rotation.y < -180.0f) Rotation.y += 360.0f;
        Rotation.z += deltaRoll;
    }

    // Sets the Camera's rotation in degrees (clamps vertical pitch to [-89, +89] degrees)
    void Set_Rotation(const Vector3D &rotDeg) {
        Rotation   = rotDeg;
        Rotation.x = U_Math::Clamp_MinMax(Rotation.x, -89.0f, 89.0f);
    }

    // Resets the camera to its default starting position, rotation, and zero velocity
    void Reset() {
        Position      = { 0.0f, 0.0f, -3.5f };
        Rotation      = { 0.0f, 0.0f,  0.0f };
        Velocity      = { 0.0f, 0.0f,  0.0f };
        RotationSpeed = { 0.0f, 0.0f,  0.0f };
        FOV           = 60.0f;
        NearClip      = 0.1f;
        FarClip       = 500.0f;
        bIsVisible    = false;
        Transparency  = 0.0f;
    }
};
