#pragma once

struct Quat
{
    float w, x, y, z;
};

unsigned long long pack_quat(Quat q);
Quat unpack_quat(unsigned long long packed);
Quat euler_to_quat(float roll, float pitch, float yaw);
unsigned long long add_euler_delta_to_packed_quat(unsigned long long packed, float deltaRoll, float deltaPitch,
                                                  float deltaYaw);
void quat_to_euler(Quat q, float& roll, float& pitch, float& yaw);
