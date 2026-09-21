#include "model/ModelAsset.hpp"

#include <cmath>

namespace Engine::Model {

Matrix4 Multiply(const Matrix4& a, const Matrix4& b) noexcept {
    Matrix4 result;
    result.values.fill(0);
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            for (std::size_t k = 0; k < 4; ++k) {
                result.values[column * 4 + row] +=
                    a.values[k * 4 + row] * b.values[column * 4 + k];
            }
        }
    }
    return result;
}

Matrix4 ToMatrix(const Transform& transform) noexcept {
    const auto& q = transform.rotation;
    const float lengthSquared = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;
    const float s = lengthSquared > 0 ? 2.0F / lengthSquared : 0.0F;
    const float xx=q.x*q.x*s, xy=q.x*q.y*s, xz=q.x*q.z*s;
    const float yy=q.y*q.y*s, yz=q.y*q.z*s, zz=q.z*q.z*s;
    const float wx=q.w*q.x*s, wy=q.w*q.y*s, wz=q.w*q.z*s;
    Matrix4 result;
    result.values = {
        (1-yy-zz)*transform.scale.x, (xy+wz)*transform.scale.x,
        (xz-wy)*transform.scale.x, 0,
        (xy-wz)*transform.scale.y, (1-xx-zz)*transform.scale.y,
        (yz+wx)*transform.scale.y, 0,
        (xz+wy)*transform.scale.z, (yz-wx)*transform.scale.z,
        (1-xx-yy)*transform.scale.z, 0,
        transform.translation.x, transform.translation.y, transform.translation.z, 1};
    return result;
}

Vec3 TransformPoint(const Matrix4& matrix, const Vec3 point) noexcept {
    const auto& m=matrix.values;
    return {m[0]*point.x+m[4]*point.y+m[8]*point.z+m[12],
            m[1]*point.x+m[5]*point.y+m[9]*point.z+m[13],
            m[2]*point.x+m[6]*point.y+m[10]*point.z+m[14]};
}

std::optional<std::size_t> ModelAsset::FindClip(const std::string_view name) const noexcept {
    for (std::size_t i=0; i<clips.size(); ++i) if (clips[i].name==name) return i;
    return std::nullopt;
}

std::optional<std::size_t> ModelAsset::FindNode(const std::string_view name) const noexcept {
    for (std::size_t i=0; i<nodes.size(); ++i) if (nodes[i].name==name) return i;
    return std::nullopt;
}

} // namespace Engine::Model
