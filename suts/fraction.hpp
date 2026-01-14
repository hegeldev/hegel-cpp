#pragma once

#include <cstdlib>
#include <numeric>
#include <stdexcept>

// A rational number class with a bug in normalization of negative denominators
class Fraction {
public:
  Fraction(int numerator, int denominator)
      : num_(numerator), den_(denominator) {
    if (denominator == 0) {
      throw std::invalid_argument("denominator cannot be zero");
    }
    normalize();
  }

  int numerator() const { return num_; }
  int denominator() const { return den_; }

  Fraction operator+(const Fraction &other) const {
    int n = num_ * other.den_ + other.num_ * den_;
    int d = den_ * other.den_;
    return Fraction(n, d);
  }

  Fraction operator-(const Fraction &other) const {
    int n = num_ * other.den_ - other.num_ * den_;
    int d = den_ * other.den_;
    return Fraction(n, d);
  }

  Fraction operator*(const Fraction &other) const {
    return Fraction(num_ * other.num_, den_ * other.den_);
  }

  Fraction operator/(const Fraction &other) const {
    if (other.num_ == 0) {
      throw std::invalid_argument("division by zero");
    }
    return Fraction(num_ * other.den_, den_ * other.num_);
  }

  bool operator==(const Fraction &other) const {
    return num_ == other.num_ && den_ == other.den_;
  }

  bool operator!=(const Fraction &other) const { return !(*this == other); }

  // Convert to double for comparison
  double to_double() const {
    return static_cast<double>(num_) / static_cast<double>(den_);
  }

private:
  void normalize() {
    // BUG: Only uses abs(num_) for GCD, doesn't handle negative denominator
    // Should move sign to numerator and ensure denominator is always positive
    int g = std::gcd(std::abs(num_), std::abs(den_));
    if (g > 0) {
      num_ /= g;
      den_ /= g;
    }
    // Missing: if (den_ < 0) { num_ = -num_; den_ = -den_; }
  }

  int num_;
  int den_;
};
