//
//  LinAlg.h
//  applaudio
//
//  Created by Rasmus Anthin on 2025-10-03.
//

#pragma once
#include <array>
#include <cmath>
#include <optional>
#include <algorithm>


namespace la
{
  enum Coord { X, Y, Z, W };
  enum M3Coord { m3XX, m3XY, m3XZ,
                 m3YX, m3YY, m3YZ,
                 m3ZX, m3ZY, m3ZZ };
  enum M4Coord { m4XX, m4XY, m4XZ, m4XW,
                 m4YX, m4YY, m4YZ, m4YW,
                 m4ZX, m4ZY, m4ZZ, m4ZW,
                 m4WX, m4WY, m4WZ, m4WW };

  class Vec3
  {
    std::array<float, 3> elem { 0.f, 0.f, 0.f };
    
  public:
    Vec3() = default;
    Vec3(float x, float y, float z)
      : elem({ x, y, z })
    {}
    Vec3(const std::array<float, 3>& xyz)
      : elem(xyz)
    {}
    
    float& operator[](int coord) { return elem[coord]; }
    const float& operator[](int coord) const { return elem[coord]; }
    
    inline float x() const noexcept { return elem[X]; }
    inline float y() const noexcept { return elem[Y]; }
    inline float z() const noexcept { return elem[Z]; }
    
    const std::array<float, 3>& to_arr() const { return elem; }
    
    Vec3 operator+() const
    {
      return *this;
    }
    
    Vec3 operator+(const Vec3& other) const
    {
      return { this->x() + other.x(),
               this->y() + other.y(),
               this->z() + other.z() };
    }
    
    const Vec3& operator+=(const Vec3& other)
    {
      *this = *this + other;
      return *this;
    }
    
    Vec3 operator-() const
    {
      return { -this->x(), -this->y(), -this->z() };
    }
    
    Vec3 operator-(const Vec3& other) const
    {
      return { this->x() - other.x(),
               this->y() - other.y(),
               this->z() - other.z() };
    }
    
    const Vec3& operator-=(const Vec3& other)
    {
      *this = *this - other;
      return *this;
    }
    
    Vec3 operator*(float s) const
    {
      return { this->x() * s, this->y() * s, this->z() * s };
    }
    
    const Vec3& operator*=(const float s)
    {
      *this = *this * s;
      return *this;
    }
    
    Vec3 operator/(float s) const
    {
      return { this->x() / s, this->y() / s, this->z() / s };
    }
    
    const Vec3& operator/=(const float s)
    {
      *this = *this / s;
      return *this;
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
  
  Vec3 cross(const Vec3& u, const Vec3& v)
  {
    return { u.y()*v.z() - u.z()*v.y(), u.z()*v.x() - u.x()*v.z(), u.x()*v.y() - u.y()*v.x() };
  }
  
  // /////////////////////////////////
  
  class Mtx3
  {
    std::array<float, 9> elem { 1.f, 0.f, 0.f,
                                0.f, 1.f, 0.f,
                                0.f, 0.f, 1.f };
    
  public:
    Mtx3() = default;
    Mtx3(float xx, float xy, float xz,
         float yx, float yy, float yz,
         float zx, float zy, float zz)
      : elem({ xx, xy, xz,
               yx, yy, yz,
               zx, zy, zz })
    {}
    Mtx3(const std::array<float, 9>& row_major_mat)
      : elem(row_major_mat)
    {}
    
    float& operator[](int idx) { return elem[idx]; }
    const float& operator[](int idx) const { return elem[idx]; }
    
    float& operator()(int r, int c) { return elem[r*3 + c]; }
    const float& operator()(int r, int c) const { return elem[r*3 + c]; }
    
    inline float xx() const noexcept { return elem[m3XX]; }
    inline float xy() const noexcept { return elem[m3XY]; }
    inline float xz() const noexcept { return elem[m3XZ]; }
    inline float yx() const noexcept { return elem[m3YX]; }
    inline float yy() const noexcept { return elem[m3YY]; }
    inline float yz() const noexcept { return elem[m3YZ]; }
    inline float zx() const noexcept { return elem[m3ZX]; }
    inline float zy() const noexcept { return elem[m3ZY]; }
    inline float zz() const noexcept { return elem[m3ZZ]; }
    
    const std::array<float, 9>& to_arr() const { return elem; }
    
    Vec3 transform_vec(const Vec3& local_vec) const
    {
      auto lv_x = local_vec.x();
      auto lv_y = local_vec.y();
      auto lv_z = local_vec.z();
      return { xx() * lv_x + xy() * lv_y + xz() * lv_z,
               yx() * lv_x + yy() * lv_y + yz() * lv_z,
               zx() * lv_x + zy() * lv_y + zz() * lv_z };
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
    
    bool set_column_vec(int col, const Vec3& col_vec)
    {
      if (0 <= col && col < 4)
      {
        (*this)(X, col) = col_vec[X];
        (*this)(Y, col) = col_vec[Y];
        (*this)(Z, col) = col_vec[Z];
        return true;
      }
      return false;
    }
  };
  
  Mtx3 Mtx3_Identity { 1.f, 0.f, 0.f,
                       0.f, 1.f, 0.f,
                       0.f, 0.f, 1.f };
  
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
    {}
    Mtx4(const std::array<float, 16>& row_major_mat)
      : elem(row_major_mat)
    {}
    
    float& operator[](int idx) { return elem[idx]; }
    const float& operator[](int idx) const { return elem[idx]; }
    
    float& operator()(int r, int c) { return elem[r*4 + c]; }
    const float& operator()(int r, int c) const { return elem[r*4 + c]; }
    
    inline float xx() const noexcept { return elem[m4XX]; }
    inline float xy() const noexcept { return elem[m4XY]; }
    inline float xz() const noexcept { return elem[m4XZ]; }
    inline float xw() const noexcept { return elem[m4XW]; }
    inline float yx() const noexcept { return elem[m4YX]; }
    inline float yy() const noexcept { return elem[m4YY]; }
    inline float yz() const noexcept { return elem[m4YZ]; }
    inline float yw() const noexcept { return elem[m4YW]; }
    inline float zx() const noexcept { return elem[m4ZX]; }
    inline float zy() const noexcept { return elem[m4ZY]; }
    inline float zz() const noexcept { return elem[m4ZZ]; }
    inline float zw() const noexcept { return elem[m4ZW]; }
    inline float wx() const noexcept { return elem[m4WX]; }
    inline float wy() const noexcept { return elem[m4WY]; }
    inline float wz() const noexcept { return elem[m4WZ]; }
    inline float ww() const noexcept { return elem[m4WW]; }
    
    const std::array<float, 16>& to_arr() const { return elem; }
    
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
    
    Mtx3 get_rot_matrix() const
    {
      Mtx3 rot_mtx;
      rot_mtx(X, X) = (*this)(X, X);
      rot_mtx(X, Y) = (*this)(X, Y);
      rot_mtx(X, Z) = (*this)(X, Z);
      
      rot_mtx(Y, X) = (*this)(Y, X);
      rot_mtx(Y, Y) = (*this)(Y, Y);
      rot_mtx(Y, Z) = (*this)(Y, Z);
      
      rot_mtx(Z, X) = (*this)(Z, X);
      rot_mtx(Z, Y) = (*this)(Z, Y);
      rot_mtx(Z, Z) = (*this)(Z, Z);
      
      return rot_mtx;
    }
    
    void set_rot_matrix(const Mtx3& rot_mtx)
    {
      (*this)(X, X) = rot_mtx(X, X);
      (*this)(X, Y) = rot_mtx(X, Y);
      (*this)(X, Z) = rot_mtx(X, Z);
      
      (*this)(Y, X) = rot_mtx(Y, X);
      (*this)(Y, Y) = rot_mtx(Y, Y);
      (*this)(Y, Z) = rot_mtx(Y, Z);
      
      (*this)(Z, X) = rot_mtx(Z, X);
      (*this)(Z, Y) = rot_mtx(Z, Y);
      (*this)(Z, Z) = rot_mtx(Z, Z);
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
                       
  Mtx4 look_at(const Vec3& location_pos, const Vec3& look_at_pos, const Vec3& up_dir)
  {
    auto F = look_at_pos - location_pos;
    auto f = normalize(F);
    auto up = normalize(up_dir);
    auto s = cross(f, up);
    auto u = cross(normalize(s), f);
    Mtx4 trf_lookat;
    trf_lookat.set_column_vec(X, s);
    trf_lookat.set_column_vec(Y, u);
    trf_lookat.set_column_vec(Z, -f);
    trf_lookat.set_column_vec(W, location_pos);
    return trf_lookat;
  }
  
  // /////////////////////////////////
  
  class Quat
  {
    std::array<float, 4> elem { 0.f, 0.f, 0.f, 1.f };
  
  public:
    Quat() = default;
    Quat(float x, float y, float z, float w)
      : elem({ x, y, z, w })
    {}
    Quat(const std::array<float, 4>& quat)
      : elem(quat)
    {}
    
    float& operator[](int coord) { return elem[coord]; }
    const float& operator[](int coord) const { return elem[coord]; }
    
    inline float x() const noexcept { return elem[X]; }
    inline float y() const noexcept { return elem[Y]; }
    inline float z() const noexcept { return elem[Z]; }
    inline float w() const noexcept { return elem[W]; }
    
    const std::array<float, 4>& to_arr() const { return elem; }
    
    void from_axis_angle(const Vec3& axis, float angle_rad)
    {
      auto axis_n = la::normalize(axis);
      auto half_ang = 0.5f * angle_rad;
      auto s = std::sin(half_ang);
      elem[X] = axis_n.x() * s;
      elem[Y] = axis_n.y() * s;
      elem[Z] = axis_n.z() * s;
      elem[W] = std::cos(half_ang);
      *this = this->normalize();
    }
    
    void from_angle_axis(const Vec3& angle_axis)
    {
      auto angle_rad = angle_axis.length();
      auto axis = la::normalize(angle_axis);
      from_axis_angle(axis, angle_rad);
    }
    
    void to_axis_angle(Vec3& axis, float& angle_rad) const
    {
      // Clamp w to [-1, 1] to avoid numerical issues.
      auto w_clamped = std::clamp(w(), -1.0f, 1.0f);
      
      angle_rad = 2.0f * std::acos(w_clamped);
      auto s = std::sqrt(1.0f - w_clamped * w_clamped); // sin(theta/2).
      
      if (s < 1e-6f)
      {
        // If s is too small, direction of axis is undefined.
        // Return a default axis (e.g. X-axis).
        axis = Vec3(1.0f, 0.0f, 0.0f);
      }
      else
      {
        axis[X] = x() / s;
        axis[Y] = y() / s;
        axis[Z] = z() / s;
      }
    }
    
    void to_angle_axis(Vec3& angle_axis) const
    {
      Vec3 axis;
      float angle_rad;
      to_axis_angle(axis, angle_rad);
      angle_axis = axis * angle_rad;
    }
    
    Quat operator+() const
    {
      return *this;
    }
    
    Quat operator+(const Quat& other) const
    {
      return { this->x() + other.x(),
               this->y() + other.y(),
               this->z() + other.z(),
               this->w() + other.w() };
    }
    
    const Quat& operator+=(const Quat& other)
    {
      *this = *this + other;
      return *this;
    }
    
    Quat operator-() const
    {
      return { -this->x(), -this->y(), -this->z(), -this->w() };
    }
    
    Quat operator-(const Quat& other) const
    {
      return { this->x() - other.x(),
               this->y() - other.y(),
               this->z() - other.z(),
               this->w() - other.w() };
    }
    
    const Quat& operator-=(const Quat& other)
    {
      *this = *this - other;
      return *this;
    }
    
    Quat operator*(float s) const
    {
      return { this->x() * s, this->y() * s, this->z() * s, this->w() * s };
    }
    
    const Quat& operator*=(const float s)
    {
      *this = *this * s;
      return *this;
    }
    
    Quat operator*(const Quat& b) const
    {
      const Quat& a = *this;
      Quat r;
      
      r[X] = a[W] * b[X] + a[X] * b[W] + a[Y] * b[Z] - a[Z] * b[Y];
      r[Y] = a[W] * b[Y] - a[X] * b[Z] + a[Y] * b[W] + a[Z] * b[X];
      r[Z] = a[W] * b[Z] + a[X] * b[Y] - a[Y] * b[X] + a[Z] * b[W];
      r[W] = a[W] * b[W] - a[X] * b[X] - a[Y] * b[Y] - a[Z] * b[Z];
      
      return r;
    }
    
    Quat operator/(float s) const
    {
      return { this->x() / s, this->y() / s, this->z() / s, this->w() / s };
    }
    
    const Quat& operator/=(const float s)
    {
      *this = *this / s;
      return *this;
    }
    
    inline float length_squared() const noexcept
    {
      auto px = x();
      auto py = y();
      auto pz = z();
      auto pw = w();
      return px*px + py*py + pz*pz + pw*pw;
    }
    
    inline float length() const noexcept
    {
      return std::sqrt(length_squared());
    }
    
    Quat normalize() const
    {
      auto l = this->length();
      if (std::abs(l) < 1e-6f)
        return { 0.f, 0.f, 0.f, 1.f };
      return (*this) / l;
    }
        
    Mtx3 to_rot_matrix() const
    {
      auto xx      = x() * x();
      auto xy      = x() * y();
      auto xz      = x() * z();
      auto xw      = x() * w();
      auto yy      = y() * y();
      auto yz      = y() * z();
      auto yw      = y() * w();
      auto zz      = z() * z();
      auto zw      = z() * w();
      Mtx3 mat;
      mat[0] = 1.f - 2.f * ( yy + zz ); // xx
      mat[1] =       2.f * ( xy - zw ); // xy
      mat[2] =       2.f * ( xz + yw ); // xz
      mat[3] =       2.f * ( xy + zw ); // yx
      mat[4] = 1.f - 2.f * ( xx + zz ); // yy
      mat[5] =       2.f * ( yz - xw ); // yz
      mat[6] =       2.f * ( xz - yw ); // zx
      mat[7] =       2.f * ( yz + xw ); // zy
      mat[8] = 1.f - 2.f * ( xx + yy ); // zz
      return mat;
    }
    
    void from_rot_matrix(const Mtx3& mat)
    {
      auto trace = mat.xx() + mat.yy() + mat.zz();
      auto ptrace = 1 + trace; // "pseudo trace" = 4*(intermediate scalar)^2
      if (ptrace > 0.f)
      {
        auto s = 0.5f / std::sqrt(ptrace); // intermediate scalar.
        elem[X] = (mat.zy() - mat.yz()) * s; // zy - yz
        elem[Y] = (mat.xz() - mat.zx()) * s; // xz - zx
        elem[Z] = (mat.yx() - mat.xy()) * s; // yx - xy
        elem[W] = 0.25f / s;
      }
      else
      {
        // Identify which diagonal element has the largest value
        if (mat.xx() > mat.yy() && mat.xx() > mat.zz())
        {
          // Column 0 dominant
          float s = 2.0f * std::sqrt(1.0f + mat.xx() - mat.yy() - mat.zz());
          elem[X] = 0.25f * s;
          elem[Y] = (mat.xy() + mat.yx()) / s;
          elem[Z] = (mat.xz() + mat.zx()) / s;
          elem[W] = (mat.zy() - mat.yz()) / s;
        }
        else if (mat.yy() > mat.zz())
        {
          // Column 1 dominant
          float s = 2.0f * std::sqrt(1.0f + mat.yy() - mat.xx() - mat.zz());
          elem[X] = (mat.xy() + mat.yx()) / s;
          elem[Y] = 0.25f * s;
          elem[Z] = (mat.yz() + mat.zy()) / s;
          elem[W] = (mat.xz() - mat.zx()) / s;
        }
        else
        {
          // Column 2 dominant
          float s = 2.0f * std::sqrt(1.0f + mat.zz() - mat.xx() - mat.yy());
          elem[X] = (mat.xz() + mat.zx()) / s;
          elem[Y] = (mat.yz() + mat.zy()) / s;
          elem[Z] = 0.25f * s;
          elem[W] = (mat.yx() - mat.xy()) / s;
        }
      }
      *this = normalize();
    }
  };
  
  Quat Quat_Unit { 0.f, 0.f, 0.f, 1.f };
  
  Quat normalize(const Quat& q)
  {
    return q.normalize();
  }
  
  Quat quat_from_axis_angle(const Vec3& axis, float angle_rad)
  {
    Quat q;
    q.from_axis_angle(axis, angle_rad);
    return q;
  }
  
  Quat quat_from_angle_axis(const Vec3& angle_axis)
  {
    Quat q;
    q.from_angle_axis(angle_axis);
    return q;
  }

}
