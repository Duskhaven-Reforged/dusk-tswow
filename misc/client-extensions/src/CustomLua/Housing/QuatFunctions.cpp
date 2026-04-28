#include <ClientDetours.h>
#include <ClientLua.h>
#include <FSRoot.h>
#include <Logger.h>
#include <ClientData/SharedDefines.h>
#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>

#include "CustomLua/Housing/QuatFunctions.h"

static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

Quat normalize_quat(Quat q)
{
    float lenSq = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;

    if (lenSq <= 0.0f)
        return {1.0f, 0.0f, 0.0f, 0.0f};

    float invLen = 1.0f / sqrtf(lenSq);

    return {q.w * invLen, q.x * invLen, q.y * invLen, q.z * invLen};
}

Quat nlerp_quat(float ratio, Quat q1, Quat q2)
{
    float w = (q2.w - q1.w) * ratio + q1.w;
    float x = (q2.x - q1.x) * ratio + q1.x;
    float y = (q2.y - q1.y) * ratio + q1.y;
    float z = (q2.z - q1.z) * ratio + q1.z;

    float magSq = x * x + y * y + z * z + w * w;
    float scale = ((magSq - 0.95906597f) * -0.532516f) + 1.021435f;
    if (magSq <= 0.91521198f)
    {
        scale *= (((scale * scale * magSq) - 0.95906597f) * -0.532516f) + 1.021435f;
        if (magSq <= 0.6521197f)
            scale *= (((scale * scale * magSq) - 0.95906597f) * -0.532516f) + 1.021435f;
    }

    return {w * scale, x * scale, y * scale, z * scale};
}

Quat multiply_quat(Quat a, Quat b)
{
    return {a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z, a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x, a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w};
}

Quat axis_angle_to_quat(float x, float y, float z, float angle)
{
    float lenSq = x * x + y * y + z * z;
    if (lenSq <= 0.0f)
        return {1.0f, 0.0f, 0.0f, 0.0f};

    float invLen = 1.0f / sqrtf(lenSq);
    float half   = angle * 0.5f;
    float s      = sinf(half);
    return normalize_quat({cosf(half), x * invLen * s, y * invLen * s, z * invLen * s});
}

unsigned long long pack_quat(Quat q)
{
    q = normalize_quat(q);

    // Your packing drops the sign of w, so force w positive.
    if (q.w < 0.0f)
    {
        q.w = -q.w;
        q.x = -q.x;
        q.y = -q.y;
        q.z = -q.z;
    }

    float x = clampf(q.x, -1.0f, 1.0f);
    float y = clampf(q.y, -1.0f, 1.0f);
    float z = clampf(q.z, -1.0f, 1.0f);

    int xi = (int)(x * 2097152.0f + (x >= 0.0f ? 0.5f : -0.5f));
    int yi = (int)(y * 1048576.0f + (y >= 0.0f ? 0.5f : -0.5f));
    int zi = (int)(z * 1048576.0f + (z >= 0.0f ? 0.5f : -0.5f));

    if (xi > 2097151)
        xi = 2097151;
    if (xi < -2097152)
        xi = -2097152;

    if (yi > 1048575)
        yi = 1048575;
    if (yi < -1048576)
        yi = -1048576;

    if (zi > 1048575)
        zi = 1048575;
    if (zi < -1048576)
        zi = -1048576;

    unsigned long long xu = (unsigned long long)(unsigned)xi & 0x3FFFFFULL;
    unsigned long long yu = (unsigned long long)(unsigned)yi & 0x1FFFFFULL;
    unsigned long long zu = (unsigned long long)(unsigned)zi & 0x1FFFFFULL;

    return (xu << 42) | (yu << 21) | zu;
}

Quat euler_to_quat(float roll, float pitch, float yaw)
{
    float hr = roll * 0.5f;
    float hp = pitch * 0.5f;
    float hy = yaw * 0.5f;

    float cr = cosf(hr), sr = sinf(hr);
    float cp = cosf(hp), sp = sinf(hp);
    float cy = cosf(hy), sy = sinf(hy);

    Quat q;

    q.w = cr * cp * cy + sr * sp * sy;
    q.x = sr * cp * cy - cr * sp * sy;
    q.y = cr * sp * cy + sr * cp * sy;
    q.z = cr * cp * sy - sr * sp * cy;

    return normalize_quat(q);
}

Quat unpack_quat(unsigned long long packed)
{
    int xi = (int)((packed >> 42) & 0x3FFFFFULL);
    int yi = (int)((packed >> 21) & 0x1FFFFFULL);
    int zi = (int)(packed & 0x1FFFFFULL);

    // Sign-extend packed signed fields.
    if (xi & 0x200000)
        xi |= ~0x3FFFFF;
    if (yi & 0x100000)
        yi |= ~0x1FFFFF;
    if (zi & 0x100000)
        zi |= ~0x1FFFFF;

    float x = (float)xi / 2097152.0f;
    float y = (float)yi / 1048576.0f;
    float z = (float)zi / 1048576.0f;

    float ww = 1.0f - x * x - y * y - z * z;
    float w  = ww > 0.0f ? sqrtf(ww) : 0.0f;

    return normalize_quat({w, x, y, z});
}

unsigned long long add_euler_delta_to_packed_quat(unsigned long long packed, float deltaRoll, float deltaPitch,
                                                  float deltaYaw)
{
    Quat current = unpack_quat(packed);
    Quat delta   = euler_to_quat(deltaRoll, deltaPitch, deltaYaw);

    Quat result = multiply_quat(current, delta);

    return pack_quat(result);
}

unsigned long long add_world_euler_delta_to_packed_quat(unsigned long long packed, float deltaRoll, float deltaPitch,
                                                        float deltaYaw)
{
    Quat current = unpack_quat(packed);
    Quat delta   = euler_to_quat(deltaRoll, deltaPitch, deltaYaw);

    return pack_quat(multiply_quat(delta, current));
}

unsigned long long add_axis_delta_to_packed_quat(unsigned long long packed, float axisX, float axisY, float axisZ,
                                                 float deltaAngle)
{
    Quat current = unpack_quat(packed);
    Quat delta   = axis_angle_to_quat(axisX, axisY, axisZ, deltaAngle);

    return pack_quat(multiply_quat(delta, current));
}

void quat_to_euler(Quat q, float& roll, float& pitch, float& yaw)
{
    q = normalize_quat(q);

    float sinr_cosp = 2.0f * (q.w*q.x + q.y*q.z);
    float cosr_cosp = 1.0f - 2.0f * (q.x*q.x + q.y*q.y);
    roll = atan2f(sinr_cosp, cosr_cosp);

    float sinp = 2.0f * (q.w*q.y - q.z*q.x);
    pitch = fabsf(sinp) >= 1.0f
        ? copysignf(3.14159265358979323846f * 0.5f, sinp)
        : asinf(sinp);

    float siny_cosp = 2.0f * (q.w*q.z + q.x*q.y);
    float cosy_cosp = 1.0f - 2.0f * (q.y*q.y + q.z*q.z);
    yaw = atan2f(siny_cosp, cosy_cosp);
}
