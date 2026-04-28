#pragma once

struct Quat
{
    float w, x, y, z;
};

Quat normalize_quat(Quat q);
Quat nlerp_quat(float ratio, Quat q1, Quat q2);
Quat multiply_quat(Quat a, Quat b);
Quat axis_angle_to_quat(float x, float y, float z, float angle);
unsigned long long pack_quat(Quat q);
Quat unpack_quat(unsigned long long packed);
Quat euler_to_quat(float roll, float pitch, float yaw);
unsigned long long add_euler_delta_to_packed_quat(unsigned long long packed, float deltaRoll, float deltaPitch,
                                                  float deltaYaw);
unsigned long long add_world_euler_delta_to_packed_quat(unsigned long long packed, float deltaRoll, float deltaPitch,
                                                        float deltaYaw);
unsigned long long add_axis_delta_to_packed_quat(unsigned long long packed, float axisX, float axisY, float axisZ,
                                                 float deltaAngle);
void quat_to_euler(Quat q, float& roll, float& pitch, float& yaw);
