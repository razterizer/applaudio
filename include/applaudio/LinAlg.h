//
//  LinAlg.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-10-03.
//

#pragma once
#include <array>

namespace la
{
  enum Coord { X, Y, Z, W };
  enum M4Coord { XX, XY, XZ, XW,
                 YX, YY, YZ, YW,
                 ZX, ZY, ZZ, ZW,
                 WX, WY, WZ, WW };

  class Vec3
  {
    std::array<float, 3> elem { 0.f, 0.f, 0.f };
    
  public:
    Vec3() = default;
    Vec3(float x, float y, float z)
      : elem({ x, y, z })
    {}
    
    float& operator[](int coord) { return elem[coord]; }
    const float& operator[](int coord) const { return elem[coord]; }
    
    inline float x() const noexcept { return elem[X]; }
    inline float y() const noexcept { return elem[Y]; }
    inline float z() const noexcept { return elem[Z]; }
    
    Vec3 operator-(const Vec3& other) const
    {
      return { this->x() - other.x(),
               this->y() - other.y(),
               this->z() - other.z() };
    }
    
    Vec3 operator/(float s) const
    {
      return { this->x() / s, this->y() / s, this->z() / s };
    }
    
    float length_squared() const
    {
      auto px = x();
      auto py = y();
      auto pz = z();
      return px*px + py*py + pz*pz;
    }
    
    float length() const
    {
      return std::sqrt(length_squared());
    }
  };
  
  Vec3 Vec3_Zero { 0.f, 0.f, 0.f };
  
  Vec3 normalize(const Vec3& v)
  {
    auto l = v.length();
    if (std::abs(l) < 1e-6f)
      return Vec3_Zero;
    return v / l;
  }
  
  float dot(const Vec3& u, const Vec3& v)
  {
    return u.x() * v.x() + u.y() * v.y() + u.z() * v.z();
  }
  
  // /////////////////////////////////
  
  class Mtx4
  {
    std::array<float, 16> elem { 1.f, 0.f, 0.f, 0.f,
                                 0.f, 1.f, 0.f, 0.f,
                                 0.f, 0.f, 1.f, 0.f,
                                 0.f, 0.f, 0.f, 1.f };
    
  public:
    Mtx4() = default;
    Mtx4(float xx, float xy, float xz, float xw,
         float yx, float yy, float yz, float yw,
         float zx, float zy, float zz, float zw,
         float wx, float wy, float wz, float ww)
      : elem({ xx, xy, xz, xw,
               yx, yy, yz, yw,
               zx, zy, zz, zw,
               wx, wy, wz, ww })
    {
    }
    
    float& operator[](int idx) { return elem[idx]; }
    const float& operator[](int idx) const { return elem[idx]; }
    
    float& operator()(int r, int c) { return elem[r*4 + c]; }
    const float& operator()(int r, int c) const { return elem[r*4 + c]; }
    
    inline float xx() const noexcept { return elem[XX]; }
    inline float xy() const noexcept { return elem[XY]; }
    inline float xz() const noexcept { return elem[XZ]; }
    inline float xw() const noexcept { return elem[XW]; }
    inline float yx() const noexcept { return elem[YX]; }
    inline float yy() const noexcept { return elem[YY]; }
    inline float yz() const noexcept { return elem[YZ]; }
    inline float yw() const noexcept { return elem[YW]; }
    inline float zx() const noexcept { return elem[ZX]; }
    inline float zy() const noexcept { return elem[ZY]; }
    inline float zz() const noexcept { return elem[ZZ]; }
    inline float zw() const noexcept { return elem[ZW]; }
    inline float wx() const noexcept { return elem[WX]; }
    inline float wy() const noexcept { return elem[WY]; }
    inline float wz() const noexcept { return elem[WZ]; }
    inline float ww() const noexcept { return elem[WW]; }
    
    Vec3 transform_pos(const Vec3& local_pos) const
    {
      auto lp_x = local_pos.x();
      auto lp_y = local_pos.y();
      auto lp_z = local_pos.z();
      return { xx() * lp_x + xy() * lp_y + xz() * lp_z + xw(),
               yx() * lp_x + yy() * lp_y + yz() * lp_z + yw(),
               zx() * lp_x + zy() * lp_y + zz() * lp_z + zw() };
    }
    
    Vec3 transform_vec(const Vec3& local_vec) const
    {
      auto lv_x = local_vec.x();
      auto lv_y = local_vec.y();
      auto lv_z = local_vec.z();
      return { xx() * lv_x + xy() * lv_y + xz() * lv_z,
               yx() * lv_x + yy() * lv_y + yz() * lv_z,
               zx() * lv_x + zy() * lv_y + zz() * lv_z };
    }
    
    bool get_column_vec(int col, Vec3& col_vec, float& w) const
    {
      if (0 <= col && col < 4)
      {
        col_vec = { (*this)(X, col), (*this)(Y, col), (*this)(Z, col) };
        w = (*this)(W, col);
        return true;
      }
      return false;
    }
    
    bool get_column_vec(int col, Vec3& col_vec) const
    {
      if (0 <= col && col < 4)
      {
        col_vec = { (*this)(X, col), (*this)(Y, col), (*this)(Z, col) };
        return true;
      }
      return false;
    }
    
    bool set_column_vec(int col, const Vec3& col_vec, std::optional<float> w = std::nullopt)
    {
      if (0 <= col && col < 4)
      {
        (*this)(X, col) = col_vec[X];
        (*this)(Y, col) = col_vec[Y];
        (*this)(Z, col) = col_vec[Z];
        if (w.has_value())
          (*this)(W, col) = w.value();
        return true;
      }
      return false;
    }
  };
  
  Mtx4 Mtx4_Identity { 1.f, 0.f, 0.f, 0.f,
                       0.f, 1.f, 0.f, 0.f,
                       0.f, 0.f, 1.f, 0.f,
                       0.f, 0.f, 0.f, 1.f };

}
